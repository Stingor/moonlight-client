#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace ro::sim {

// ── L'HOMONCULE DU JOUEUR ───────────────────────────────────────────────────
//
// Ce que le serveur en dit : sa fiche, ses compétences, son auto-alimentation.
// Il n'en existe qu'un par personnage, et il n'est « là » qu'une fois que le
// serveur l'a ACCUSÉ — c'est la garde du raccourci du client, qui refuse
// d'ouvrir sa fenêtre sans lui. ⚠ LE REPOS QUE LE JOUEUR LANCE LUI-MÊME le
// rend absent : le client l'oublie dès l'annonce de la compétence, et le
// serveur ne l'accuse de nouveau qu'au prochain appel. Voir `NoticeOwnSkill`.

// LES COMPÉTENCES D'HOMONCULE commencent à 8001 et sont au plus 43. Le serveur
// aiguille seul un tel identifiant vers l'homoncule, par le même paquet que les
// compétences du joueur.
inline constexpr std::uint16_t kHomunculusSkillBase = 8001;
inline constexpr std::uint16_t kHomunculusSkillCount = 43;
[[nodiscard]] constexpr bool IsHomunculusSkill(std::uint16_t skill_id) noexcept {
  return skill_id >= kHomunculusSkillBase &&
         skill_id < kHomunculusSkillBase + kHomunculusSkillCount;
}

// La satiété se lit sur cent, et l'intimité sur mille.
inline constexpr int kHomunculusHungerMost = 100;
inline constexpr int kHomunculusIntimacyMost = 1000;

// Le plus long nom qu'un homoncule reçoit, en octets sur le fil.
inline constexpr std::size_t kHomunculusNameMaxBytes = 23;

// LES HUIT PALIERS D'INTIMITÉ, aux bornes mêmes du serveur.
enum class HomunculusIntimacyGrade : std::uint8_t {
  kHateWithPassion,
  kHate,
  kAwkward,
  kShy,
  kNeutral,
  kCordial,
  kLoyal,
  // Au-delà de mille : le serveur n'en envoie pas.
  kUnknown,
};

[[nodiscard]] HomunculusIntimacyGrade GradeOfIntimacy(int intimacy) noexcept;

// LA VITESSE D'ATTAQUE, que le paquet ne porte pas : elle se déduit du délai
// d'attaque.
[[nodiscard]] int AspdFromAmotion(int amotion_ms) noexcept;

// CE QUE DONNE LE LANCEMENT D'UNE COMPÉTENCE DE COMPAGNON — homoncule ou
// mercenaire — depuis sa fiche ou une barre.
//
// ⚠ SEULE CELLE QUI SE LANCE SUR SOI PART TOUT DE SUITE. Celle qui vise une
// cible ou une case demande une visée dont la portée se mesure depuis le
// COMPAGNON : c'est l'IA du compagnon qui s'approche et la lance, jamais le
// grimoire du joueur, qui ferait marcher le joueur.
enum class CompanionCast : std::uint8_t {
  kNow,
  kPassive,
  kNotLearned,
  kNeedsAim,
};

// `targeting` porte les drapeaux de visée du serveur.
[[nodiscard]] CompanionCast CompanionCastOf(std::uint16_t targeting, int level);

// UNE COMPÉTENCE DE L'HOMONCULE, telle que le serveur la décrit.
struct HomunculusSkill {
  std::uint16_t skill_id = 0;
  // Les drapeaux de visée du serveur ; zéro pour une passive.
  std::uint16_t targeting = 0;
  // Le niveau APPRIS.
  int level = 0;
  int sp_cost = 0;
  int range_cells = 0;
  // VRAI QUAND LE SERVEUR ACCEPTERAIT UN NIVEAU DE PLUS : niveau d'homoncule
  // atteint et maximum de l'arbre non touché. C'est son verdict, pas le nôtre.
  bool upgradable = false;
  // Le nom INTERNE que le serveur écrit (« HAMI_CASTLE »).
  std::string server_name;

  [[nodiscard]] bool passive() const noexcept { return targeting == 0; }
};

// LA FICHE, telle que la quatrième forme du paquet d'état la porte.
struct HomunculusSheet {
  std::string name;
  // Le renommage est consommé. ⚠ UN SERVEUR QUI PERMET DE RENOMMER À VOLONTÉ
  // NE LE POSE JAMAIS.
  bool renamed = false;
  // Au repos : rentré, hors de la carte.
  bool resting = false;
  bool alive = false;
  int level = 0;
  int hunger = 0;
  int intimacy = 0;
  int atk = 0;
  int matk = 0;
  int hit = 0;
  int crit = 0;
  int def = 0;
  int mdef = 0;
  int flee = 0;
  int amotion_ms = 0;
  std::int64_t hp = 0;
  std::int64_t max_hp = 0;
  std::int64_t sp = 0;
  std::int64_t max_sp = 0;
  std::int64_t exp = 0;
  // Zéro au niveau maximum.
  std::int64_t exp_next = 0;
  int skill_points = 0;
  int attack_range_cells = 0;
};

class Homunculus {
 public:
  // LE SERVEUR L'ACCUSE : il existe, et voici son acteur.
  void Acknowledge(std::uint32_t actor_id) noexcept;

  // La fiche entière. Rend vrai quand elle doit faire ÉCRIRE l'alerte de faim :
  // une satiété qui ARRIVE à un, cinq ou dix, une fois par palier.
  [[nodiscard]] bool TakeSheet(const HomunculusSheet& sheet);

  // Les trois champs qui changent seuls. ⚠ AUCUN NE TOUCHE AUX MAXIMUMS, qui
  // n'arrivent qu'avec la fiche entière.
  void SetExp(std::int64_t exp) noexcept { sheet_.exp = exp; }
  void SetHp(std::int64_t hp) noexcept { sheet_.hp = hp; }
  void SetSp(std::int64_t sp) noexcept { sheet_.sp = sp; }
  void SetIntimacy(int intimacy) noexcept { sheet_.intimacy = intimacy; }
  void SetHunger(int hunger) noexcept { sheet_.hunger = hunger; }

  // La liste entière, qui remplace la précédente.
  void TakeSkills(std::vector<HomunculusSkill> skills) { skills_ = std::move(skills); }
  // Une compétence qui change ; ajoutée si la liste ne la portait pas.
  void UpdateSkill(const HomunculusSkill& skill);

  void SetAutoFeed(bool on) noexcept { auto_feed_ = on; }

  // L'HOMONCULE VIENT D'ÊTRE ABANDONNÉ À NOTRE DEMANDE. ⚠ LE SERVEUR N'EN DIT
  // RIEN DONT ON PUISSE LE DÉDUIRE : c'est le client qui l'oublie.
  void Forget() noexcept;

  // UNE COMPÉTENCE QUE LE JOUEUR LUI-MÊME VIENT D'ANNONCER, et l'instant de
  // l'annonce. Rest (244) fait oublier l'homoncule, comme `Forget` ; une même
  // annonce relue ne compte qu'une fois. Rend vrai quand elle l'a fait oublier.
  bool NoticeOwnSkill(std::uint16_t skill_id, std::uint32_t announced_ms) noexcept;

  // UN AUTRE PERSONNAGE : tout ce qui précède appartenait au précédent.
  void Reset() noexcept { *this = Homunculus(); }

  [[nodiscard]] bool present() const noexcept { return acknowledged_; }
  [[nodiscard]] bool sheet_known() const noexcept { return sheet_known_; }
  [[nodiscard]] std::uint32_t actor_id() const noexcept { return actor_id_; }
  [[nodiscard]] const HomunculusSheet& sheet() const noexcept { return sheet_; }
  [[nodiscard]] std::span<const HomunculusSkill> skills() const noexcept { return skills_; }
  [[nodiscard]] const HomunculusSkill* FindSkill(std::uint16_t skill_id) const noexcept;
  [[nodiscard]] bool auto_feed() const noexcept { return auto_feed_; }
  // ACTIF : là, et ni au repos ni sans PV. C'est la condition que le serveur
  // pose avant d'accepter de le nourrir, de le renommer ou de l'abandonner.
  [[nodiscard]] bool active() const noexcept {
    return acknowledged_ && sheet_known_ && !sheet_.resting && sheet_.alive;
  }

 private:
  bool acknowledged_ = false;
  bool sheet_known_ = false;
  std::uint32_t actor_id_ = 0;
  HomunculusSheet sheet_;
  std::vector<HomunculusSkill> skills_;
  bool auto_feed_ = false;
  // La dernière satiété qui a fait écrire l'alerte.
  int last_hunger_warned_ = 0;
  // L'instant de la dernière annonce du joueur déjà lue.
  std::uint32_t last_noticed_ms_ = 0;
};

}  // namespace ro::sim

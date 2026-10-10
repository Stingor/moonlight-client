#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ro::sim {

// L'ÉTAT DU MERCENAIRE : sa fiche, ses compétences, et le temps de contrat
// qui lui reste.
//
// ⚠ IL EXISTE DEPUIS LA FICHE, ET SEULEMENT ELLE : la liste des compétences
// peut la précéder sans le faire paraître. Il cesse d'exister quand le contrat
// prend fin, quand il meurt, quand le joueur le renvoie — et à chaque entrée
// sur une carte, où le serveur renvoie la fiche s'il est toujours là.

// SA FICHE, telle que le serveur la tient.
struct MercenaryProfile {
  // L'identifiant de son ACTEUR dans le monde.
  std::uint32_t actor_id = 0;
  // En UTF-8.
  std::string name;
  int level = 0;
  int hp = 0;
  int max_hp = 0;
  int sp = 0;
  int max_sp = 0;
  int atk = 0;
  int matk = 0;
  int hit = 0;
  int crit = 0;
  int def = 0;
  int mdef = 0;
  int flee = 0;
  // Le délai entre deux attaques, en millisecondes.
  int amotion_ms = 0;
  int range_cells = 0;
  int faith = 0;
  std::uint32_t calls = 0;
  std::uint32_t kills = 0;
  // La fin du contrat, en secondes depuis l'époque Unix.
  std::uint32_t expire_unix_s = 0;
};

// CE QU'UNE VALEUR ISOLÉE PEUT CHANGER de la fiche.
enum class MercenaryStat : std::uint8_t {
  kHp,
  kMaxHp,
  kSp,
  kMaxSp,
  kAtk,
  kMatk,
  kHit,
  kCrit,
  kDef,
  kMdef,
  kFlee,
  kAmotion,
  kKills,
  kFaith,
};

// UNE DE SES COMPÉTENCES.
struct MercenarySkill {
  std::uint16_t skill_id = 0;
  // Les drapeaux de visée du serveur, ceux de `KnownSkill::targeting`.
  std::uint16_t targeting = 0;
  std::uint16_t level = 0;
  std::uint16_t sp_cost = 0;
  std::uint16_t range_cells = 0;
  // Le nom INTERNE du serveur, « MS_BASH ».
  std::string name;
};

// POURQUOI IL EST PARTI, pour le dire une fois dans la conversation.
enum class MercenaryFarewell : std::uint8_t {
  kNone,
  kContractEnded,
  kDied,
};

// LA GUILDE QUI L'A LOUÉ, d'après la classe de son acteur.
enum class MercenaryGuild : std::uint8_t {
  kArcher,
  kSpearman,
  kSwordman,
  // Une classe hors des trois guildes : un monstre loué.
  kMonster,
};

[[nodiscard]] MercenaryGuild MercenaryGuildOfJob(std::uint16_t job);

// LES RAPPELS DE FIN DE CONTRAT, en minutes restantes, du plus lointain au plus
// proche. Le serveur ne prévient pas avant la fin : c'est le client qui le dit.
inline constexpr std::array<std::uint32_t, 2> kMercenaryReminderMinutes = {5, 1};

// L'ASPD qu'affiche le client : (2000 - délai) / 10, arrondie vers zéro.
[[nodiscard]] int MercenaryAspd(int amotion_ms);

class MercenaryBook {
 public:
  // LA FICHE REÇUE. La fin du contrat devient une échéance sur l'horloge du
  // client : `now_unix_s` est l'heure murale à la réception, `now_ms` l'horloge
  // du client au même instant. Une fin déjà passée laisse un contrat nul.
  void Meet(MercenaryProfile profile, std::uint32_t now_ms, std::int64_t now_unix_s);

  // UNE VALEUR QUI CHANGE. Rend faux, et ne touche à rien, sans mercenaire.
  bool Change(MercenaryStat stat, std::int32_t value);

  void SetSkills(std::vector<MercenarySkill> skills);

  // IL EST PARTI : tout s'oublie. `why` se garde jusqu'à `TakeFarewell`, sauf
  // `kNone` qui ne remplace pas un adieu encore à dire.
  void Part(MercenaryFarewell why);
  [[nodiscard]] MercenaryFarewell TakeFarewell() noexcept;

  [[nodiscard]] bool present() const noexcept { return present_; }
  [[nodiscard]] const MercenaryProfile& profile() const noexcept { return profile_; }
  [[nodiscard]] std::span<const MercenarySkill> skills() const noexcept { return skills_; }
  [[nodiscard]] const MercenarySkill* FindSkill(std::uint16_t skill_id) const;

  // Le temps de contrat qui reste, en secondes, jamais négatif.
  [[nodiscard]] std::uint32_t contract_left_s(std::uint32_t now_ms) const noexcept;

  // LE RAPPEL QUI VIENT D'ÊTRE FRANCHI, en minutes restantes ; zéro sinon.
  //
  // ⚠ CHAQUE PALIER NE SONNE QU'UNE FOIS PAR CONTRAT, et un palier déjà passé
  // quand la fiche arrive ne sonne pas : le serveur la renvoie à chaque
  // changement de carte, et un rappel répété à chaque portail ne dirait plus
  // rien. Deux paliers franchis d'un coup ne disent que le plus proche.
  [[nodiscard]] std::uint32_t TakeContractReminder(std::uint32_t now_ms) noexcept;

 private:
  bool present_ = false;
  MercenaryProfile profile_;
  std::vector<MercenarySkill> skills_;
  std::uint32_t contract_deadline_ms_ = 0;
  // Les paliers de `kMercenaryReminderMinutes` encore à sonner.
  std::array<bool, kMercenaryReminderMinutes.size()> reminder_armed_ = {};
  MercenaryFarewell farewell_ = MercenaryFarewell::kNone;
};

}  // namespace ro::sim

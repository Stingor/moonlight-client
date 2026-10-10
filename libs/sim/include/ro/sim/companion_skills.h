#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ro/sim/actor.h"
#include "ro/sim/packed_position.h"

namespace ro::sim {

// ── LES SKILLS OFFENSIFS AUTOMATIQUES D'UN COMPAGNON ────────────────────────
//
// Ce que l'IA lance d'elle-même sur sa cible courante : quand, lequel, et ce
// qu'elle fait d'un refus du serveur. Un skill offensif est un skill du
// compagnon, appris, qui vise un ENNEMI pour le blesser.
//
// Les skills AU SOL ne partent pas d'eux-mêmes : choisir leur case demande les
// comptages d'ennemis et la « meilleure case » des règles si/alors. Ceux de
// SOUTIEN et ceux SUR SOI non plus : ils ne visent pas l'ennemi. Ni les DEBUFFS
// (voir `IsCompanionDebuff`), ni ceux qui coûtent ce qui ne se rattrape pas
// (voir `NeverCastAutomatically`).

// QUAND L'IA LANCE SES SKILLS OFFENSIFS.
enum class OffensiveSkills : std::uint8_t {
  kNever,
  // À portée d'attaque simple de sa cible seulement.
  kInCombat,
  // Dès que la cible est à portée du skill, en poursuite comprise.
  kChaseAndCombat,
  // Jamais d'attaque simple : il s'approche à portée du skill et lance ; sans
  // skill lançable, il attend à portée sans frapper.
  kSkillsOnly,
};

// LE MODE D'USINE : « en combat », choix du joueur, pour que les compagnons
// emploient leurs skills sans qu'on ait à le régler.
inline constexpr OffensiveSkills kFactoryOffensiveSkills = OffensiveSkills::kInCombat;
// LE DÉLAI D'USINE ENTRE DEUX SKILLS.
inline constexpr std::uint32_t kFactorySkillDelayMs = 400;
// UN SKILL ÉMIS SANS TRACE dans ce délai — ni incantation, ni coup qui le porte
// — est tenu pour refusé : le serveur en tait la plupart des refus.
inline constexpr std::uint32_t kSkillTraceMs = 2000;
// UN SKILL REFUSÉ ne se retente pas avant ce délai, au lieu d'être rejoué à
// chaque tick.
inline constexpr std::uint32_t kSkillRefusalSuspendMs = 5000;
// L'ÉMOTICÔNE « … » (penser) que le serveur fait jouer à l'homoncule quand il
// refuse son skill.
inline constexpr std::uint16_t kEmotionThink = 9;

// UN SKILL PERMIS OU NON, et à quel niveau. Zéro : le niveau appris.
struct SkillPermission {
  std::uint16_t skill_id = 0;
  bool allowed = true;
  std::uint16_t level = 0;

  [[nodiscard]] friend bool operator==(const SkillPermission&, const SkillPermission&) = default;
};

// LES RÉGLAGES DES SKILLS AUTOMATIQUES.
struct CompanionSkillSettings {
  OffensiveSkills offensive = kFactoryOffensiveSkills;
  // Un skill absent de la liste est permis au niveau appris : d'usine, tous.
  std::vector<SkillPermission> permissions;
  // Un skill ne part pas s'il laisserait moins que ces SP.
  std::int64_t sp_reserve_points = 0;
  std::uint32_t delay_ms = kFactorySkillDelayMs;

  [[nodiscard]] friend bool operator==(const CompanionSkillSettings&,
                                       const CompanionSkillSettings&) = default;
};

// UN SKILL QUE LE COMPAGNON POSSÈDE, tel que son livre le décrit.
struct CompanionSkillFact {
  std::uint16_t skill_id = 0;
  // Le niveau appris.
  std::uint16_t level = 0;
  // Les drapeaux de visée du serveur (`kSkillOnEnemy`…).
  std::uint16_t targeting = 0;
  // Le coût du niveau APPRIS : le livre ne dit pas celui des autres niveaux.
  int sp_cost = 0;
  int range_cells = 0;
  // La recharge qui lui reste, telle que le serveur l'a annoncée au maître.
  std::uint32_t cooldown_remaining_ms = 0;
  // LE RAYON DE SA ZONE, en cases autour de la case visée, d'après les tables
  // du client ; zéro quand elles n'en déclarent pas.
  int area_radius_cells = 0;

  [[nodiscard]] friend bool operator==(const CompanionSkillFact&,
                                       const CompanionSkillFact&) = default;
};

// LA DERNIÈRE TRACE DE CE SKILL sur le compagnon, ou rien : une incantation, un
// coup qui le porte, l'emploi d'un skill de soutien ou de soi (`skill_use`),
// une case visée (`ground_skill`). ⚠ LA PLUPART DES BUFFS ET DES SOINS N'ONT NI
// INCANTATION NI COUP : sans les deux derniers, ils passeraient pour refusés.
[[nodiscard]] std::optional<std::uint32_t> LastTraceMs(const Actor& self,
                                                       std::uint16_t skill_id) noexcept;

// UN SKILL QUI NE PART JAMAIS DE LUI-MÊME, quel que soit le réglage : il coûte
// ce qui ne se rattrape pas. Il reste lançable à la main.
[[nodiscard]] bool NeverCastAutomatically(std::uint16_t skill_id) noexcept;

// UN DEBUFF : il vise l'ennemi sans le blesser. Le paquet du livre ne le dit
// pas ; la liste vient de la base de skills du serveur. Ils relèvent des
// tactiques par monstre et des règles si/alors, pas des skills offensifs.
[[nodiscard]] bool IsCompanionDebuff(std::uint16_t skill_id) noexcept;

// Appris, visant un ennemi hors du sol, ni debuff ni interdit d'automatisme.
[[nodiscard]] bool IsOffensiveCompanionSkill(const CompanionSkillFact& skill) noexcept;

// L'INSTANT D'UNE DÉCISION : où sont le compagnon et sa cible, et ce qu'il a.
struct SkillMoment {
  const Actor& self;
  CellPosition self_cell;
  CellPosition target_cell;
  // Arrêté, et son dernier déplacement posé : le serveur mesure la portée sur
  // sa position COURANTE.
  bool stopped = false;
  std::int64_t sp = 0;
  bool sp_known = false;
  std::uint32_t now_ms = 0;
};

// LE SKILL CHOISI, et le niveau auquel il part.
struct SkillPick {
  std::uint16_t skill_id = 0;
  std::uint16_t level = 0;

  [[nodiscard]] friend bool operator==(const SkillPick&, const SkillPick&) = default;
};

// AUCUNE RÈGLE : le skill vient des skills automatiques.
inline constexpr int kNoRule = -1;
// UNE RÈGLE D'UNE LISTE QUI A CHANGÉ depuis l'envoi : elle ne désigne plus rien.
inline constexpr int kOrphanRule = -2;

// UN SKILL PARTI, d'après sa trace : lequel, sur qui, et la règle qui l'a
// lancé.
struct SkillTrace {
  std::uint16_t skill_id = 0;
  std::uint32_t target_id = 0;
  int rule_index = kNoRule;

  [[nodiscard]] friend bool operator==(const SkillTrace&, const SkillTrace&) = default;
};

class CompanionSkillCaster {
 public:
  // À CHAQUE TICK, avant de choisir : relit les traces des skills émis (une
  // incantation, un coup qui les porte), l'émoticône de refus, et suspend ce qui
  // n'a laissé aucune trace à temps.
  void Observe(const Actor& self, std::uint32_t now_ms);
  // LE SERVEUR A DIT NON (`ZC_ACK_TOUSESKILL` en échec) : ce skill se suspend,
  // ou la règle qui l'a lancé.
  void NoteRefused(std::uint16_t skill_id, std::uint32_t now_ms);
  // Le premier skill permis, dans l'ordre du livre, qui part à cet instant ;
  // `forced_level`, s'il n'est pas nul, impose son niveau, borné à l'appris.
  [[nodiscard]] std::optional<SkillPick> Choose(const CompanionSkillSettings& settings,
                                                std::span<const CompanionSkillFact> skills,
                                                const SkillMoment& moment,
                                                std::uint16_t forced_level = 0) const;
  // LE SKILL D'OUVERTURE d'une tactique, s'il part à cet instant : possédé, visant
  // un ennemi, hors de ceux qui ne partent jamais seuls. Un debuff y est permis,
  // puisque le joueur l'a nommé.
  [[nodiscard]] std::optional<SkillPick> ChooseOpening(const CompanionSkillSettings& settings,
                                                       std::span<const CompanionSkillFact> skills,
                                                       const SkillMoment& moment,
                                                       std::uint16_t skill_id) const;
  // LE SKILL D'UNE RÈGLE PEUT PARTIR à cet instant, portée mise à part : appris,
  // hors recharge, non suspendu, sans trace attendue, ni incantation en cours ni
  // délai entre deux skills (`delay_ms`) qui court, et la SP le permet au-dessus
  // de `sp_reserve_points`.
  [[nodiscard]] bool RuleCastable(const CompanionSkillFact& skill, std::uint32_t delay_ms,
                                  std::int64_t sp_reserve_points, const SkillMoment& moment) const;
  // Le skill vient de partir sur `target_id` (zéro pour une case) : il attend sa
  // trace. `rule_index` : la règle qui l'a lancé.
  void NoteSent(const SkillPick& pick, std::uint32_t target_id, std::uint32_t now_ms,
                int rule_index = kNoRule);
  // LES RÈGLES DONT UN SKILL A ÉTÉ REFUSÉ depuis le dernier appel. ⚠ UN REFUS
  // SUSPEND LA RÈGLE, pas le skill : une autre règle peut le lancer autrement.
  [[nodiscard]] std::vector<int> TakeRefusedRules();
  // LA LISTE DES RÈGLES A CHANGÉ : chaque skill qui attend sa trace, chaque
  // trace et chaque refus pas encore lus suivent leur règle à son nouveau rang
  // (`old_to_new`, par ancien rang). Une règle retirée ou modifiée a pour rang
  // `kOrphanRule` : sa trace ne compte plus pour personne, et son refus ne
  // suspend rien.
  void RemapRuleSkills(std::span<const int> old_to_new) noexcept;
  // Les skills de cette règle qui attendent encore leur trace.
  [[nodiscard]] int PendingOfRule(int rule_index) const noexcept;
  // Ceux de cette règle sur `target_id`.
  [[nodiscard]] int PendingOfRuleOn(int rule_index, std::uint32_t target_id) const noexcept;
  // LES SKILLS QUI ONT LAISSÉ LEUR TRACE depuis le dernier appel, avec leur
  // cible : c'est à la trace, et non à l'envoi, qu'un skill compte comme lancé.
  // Un refus n'y figure jamais.
  [[nodiscard]] std::vector<SkillTrace> TakeTraced();
  // Les skills AUTOMATIQUES émis sur `target_id` qui attendent encore leur trace.
  [[nodiscard]] int PendingOn(std::uint32_t target_id) const noexcept;
  // UN SKILL D'ORDRE DIRECT vient de partir : il compte dans le délai entre deux
  // skills, et l'émoticône de refus qui le suit est la sienne. Son absence de
  // trace ne suspend rien : c'est l'affaire de l'ordre.
  void NoteDirectSent(std::uint16_t skill_id, std::uint32_t now_ms);

  [[nodiscard]] bool Suspended(std::uint16_t skill_id, std::uint32_t now_ms) const noexcept;
  // LES SUSPENSIONS EN COURS : chaque skill, et ce qui lui reste.
  [[nodiscard]] std::vector<std::pair<std::uint16_t, std::uint32_t>> SuspensionsLeft(
      std::uint32_t now_ms) const;
  // Suspend ce skill jusqu'à `until_ms`, comme un refus l'aurait fait.
  void Suspend(std::uint16_t skill_id, std::uint32_t until_ms);
  // LA PLUS LONGUE PORTÉE parmi les skills qui partiraient à cet instant si la
  // cible était à portée et le compagnon arrêté ; vide sans eux. C'est
  // jusque-là qu'un compagnon « en poursuite et en combat » s'approche.
  [[nodiscard]] std::optional<int> ReachNow(const CompanionSkillSettings& settings,
                                            std::span<const CompanionSkillFact> skills,
                                            const SkillMoment& moment) const;
  // LA PLUS LONGUE PORTÉE parmi les skills offensifs permis, vide sans eux :
  // c'est jusque-là qu'un compagnon en « skills seuls » s'approche.
  [[nodiscard]] static std::optional<int> LongestReach(const CompanionSkillSettings& settings,
                                                       std::span<const CompanionSkillFact> skills);

 private:
  struct Pending {
    std::uint16_t skill_id = 0;
    std::uint32_t sent_ms = 0;
    std::uint32_t target_id = 0;
    int rule_index = kNoRule;
  };
  // Le refus de ce skill émis : la règle qui l'a lancé se suspend, sinon le
  // skill.
  void Refuse(const Pending& refused, std::uint32_t now_ms);
  struct Suspension {
    std::uint16_t skill_id = 0;
    std::uint32_t until_ms = 0;
  };

  // Ce qui bride tous les skills à cet instant, portée et arrêt mis à part.
  [[nodiscard]] bool Held(const CompanionSkillSettings& settings, const SkillMoment& moment) const;
  // Ce skill pourrait partir, quel qu'il soit : appris, hors recharge, non
  // suspendu, sans trace attendue, et la SP le permet.
  [[nodiscard]] bool CastableNow(const CompanionSkillSettings& settings,
                                 const CompanionSkillFact& skill, const SkillMoment& moment) const;
  // Ce skill partirait, portée et arrêt mis à part : son niveau, zéro sinon.
  [[nodiscard]] std::uint16_t CastableLevel(const CompanionSkillSettings& settings,
                                            const CompanionSkillFact& skill,
                                            const SkillMoment& moment) const;

  std::optional<std::uint32_t> last_sent_ms_;
  // Le dernier skill émis, automatique ou direct : l'émoticône de refus qui le
  // suit lui revient.
  std::optional<Pending> last_emitted_;
  // Les skills automatiques émis dont on attend une trace, un au plus par skill.
  std::vector<Pending> pending_;
  std::vector<SkillTrace> traced_;
  std::vector<Suspension> suspensions_;
  std::vector<int> refused_rules_;
};

}  // namespace ro::sim

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace ro::sim {

// ── LES RÈGLES SI/ALORS D'UN COMPAGNON (niveau 3 de l'ADR 0092) ─────────────
//
// Une règle a UNE action et de une à trois conditions, toutes requises : un OU
// s'écrit avec deux règles. Le vocabulaire est FERMÉ : chaque champ est un rang
// d'énumération ou un nombre borné, et `ValidRule` refuse tout le reste. Un
// fichier modifié à la main peut donc tout dire, sauf sortir du cadre.
//
// 🔴 UNE DONNÉE INCONNUE REND LA CONDITION FAUSSE, jamais vraie, y compris pour
// les formes négatives : « ne porte pas S » sur un statut que le client ne voit
// pas est faux. Voir `CompanionBrain` pour la lecture.

// AU PLUS CE NOMBRE DE RÈGLES par profil.
inline constexpr std::size_t kCompanionRuleMost = 16;
// DE UNE À TROIS CONDITIONS par règle.
inline constexpr std::size_t kRuleConditionMost = 3;
// AU PLUS CE NOMBRE D'AMIS par profil.
inline constexpr std::size_t kCompanionFriendMost = 32;
// LES BORNES DES NOMBRES d'une condition ou d'une action.
inline constexpr int kRulePercentMost = 100;
inline constexpr int kRulePointsMost = 99999;
inline constexpr int kRuleSecondsMost = 3600;
// ⚠ UN PLAFOND ABSOLU, contre les valeurs aberrantes : la distance qui compte
// en jeu s'arrête au rayon de vue que le serveur annonce.
inline constexpr int kRuleCellsMost = 20;
inline constexpr int kRuleCountMost = 50;
inline constexpr int kRuleSkillLevelMost = 10;
inline constexpr int kRulePerTargetMost = 99;
inline constexpr std::uint32_t kRuleDelayMostMs = 3600000;

// DE QUI PARLE UNE CONDITION.
enum class RuleSubject : std::uint8_t { kOwner, kCompanion, kTarget, kSurroundings };

// CE QU'ELLE EN DIT. Les sujets permis et les champs lus sont ceux de
// `ValidCondition`.
enum class RulePredicate : std::uint8_t {
  // PV en pour cent (`number`) : maître, compagnon (sous / au-dessus), cible
  // (sous seulement).
  kHpPercent,
  // SP du compagnon, en pour cent ou en points (`number`).
  kSpPercent,
  kSpPoints,
  // Le maître ou le compagnon est attaqué sans interruption depuis `seconds`.
  kAttackedFor,
  // Au moins `number` monstres l'attaquent, comptés par le poids de leur
  // tactique (1 sans ligne).
  kAttackersWeight,
  // Porte le statut `id` : maître ; compagnon quand le client le voit.
  kHasStatus,
  // Le maître est assis.
  kSitting,
  // Le compagnon est au repos, en poursuite ou en combat (`choice`, voir
  // `CompanionActivity`).
  kActivity,
  // Le compagnon a lancé le skill `id` il y a moins de `seconds`, d'après les
  // traces de ses skills ; « n'est pas » : il ne l'a pas lancé depuis, ce qui
  // entretient un buff.
  kCastWithin,
  // Le compagnon est apparu ou a changé de carte il y a moins de `seconds`.
  kAppearedWithin,
  // Le compagnon est à plus de `cells` du maître.
  kFartherThan,
  // La race, l'élément, la taille, le rang de la cible (`choice`), d'après la
  // fiche de monstre du serveur.
  kRace,
  kElement,
  kSize,
  kMonsterRank,
  // La cible est de la classe de monstre `id`.
  kMonsterJob,
  // La cible incante.
  kCasting,
  // La cible attaque le maître, le compagnon ou un ami (`choice`, voir
  // `RuleWhom`).
  kAttacks,
  // Au moins `number` ennemis, comptés par leur poids, à moins de `cells` du
  // compagnon, du maître ou de la cible (`choice`, voir `RuleAround`).
  kEnemiesNear,
};
inline constexpr RulePredicate kLastRulePredicate = RulePredicate::kEnemiesNear;

// LA COMPARAISON : sous / au-dessus pour une mesure, est / n'est pas pour un
// fait. Un compte « au moins N » et une durée s'écrivent `kIs`.
enum class RuleComparison : std::uint8_t { kBelow, kAbove, kIs, kIsNot };

// CE QUE FAIT LE COMPAGNON, pour `kActivity`.
enum class CompanionActivity : std::uint8_t { kResting, kChasing, kFighting };
// QUI LA CIBLE ATTAQUE, pour `kAttacks`.
enum class RuleWhom : std::uint8_t { kOwner, kCompanion, kFriend };
// AUTOUR DE QUI ON COMPTE, pour `kEnemiesNear`.
enum class RuleAround : std::uint8_t { kCompanion, kOwner, kTarget };

// LES BORNES DES CHOIX lus sur la fiche de monstre : les constantes du serveur.
inline constexpr int kMonsterRaceCount = 10;
inline constexpr int kMonsterElementCount = 10;
inline constexpr int kMonsterSizeCount = 3;
// Ordinaire, mini-boss, MVP.
inline constexpr int kMonsterRankCount = 3;

struct RuleCondition {
  RuleSubject subject = RuleSubject::kCompanion;
  RulePredicate predicate = RulePredicate::kHpPercent;
  RuleComparison comparison = RuleComparison::kBelow;
  // Un pour cent, des points, un compte.
  int number = 0;
  int seconds = 0;
  int cells = 0;
  // Un statut, un skill, une classe de monstre.
  std::uint16_t id = 0;
  // Le rang dans l'énumération du prédicat.
  int choice = 0;

  [[nodiscard]] friend bool operator==(const RuleCondition&, const RuleCondition&) = default;
};

// L'ACTION D'UNE RÈGLE.
enum class RuleAction : std::uint8_t {
  kAttack,
  // Lancer `skill_id` au niveau `skill_level` (zéro : le niveau appris, et
  // jamais au-delà).
  kCastSkill,
  kReturnToOwner,
  // S'écarter de l'acteur visé de `step_cells`.
  kStepAway,
  // Met fin à la lecture et retient l'offensive ce tick : ni règle suivante, ni
  // attaque, ni skill automatique, ni harcèlement, ni ouverture. Les
  // déplacements de sûreté restent — suivi du maître, laisse, repli et fuite
  // sous X % de PV, évitement — et un ordre direct passe toujours.
  kDoNothing,
};
inline constexpr RuleAction kLastRuleAction = RuleAction::kDoNothing;

// CE QUE VISE L'ACTION. Le genre du skill borne ce qui se vise : un skill de
// soi ne se vise pas ailleurs, un skill au sol vise une case.
enum class RuleAim : std::uint8_t {
  // La cible du combat, celle des tactiques.
  kTarget,
  kSelf,
  kOwner,
  kOwnerTarget,
  kOwnerAttacker,
  // Le plus blessé des amis à portée, PV connus seulement.
  kFriend,
  kCellUnderTarget,
  kCellUnderOwner,
  // La case où la zone du skill couvre le plus d'ennemis, comptés par leur
  // poids.
  kBestCell,
};
inline constexpr RuleAim kLastRuleAim = RuleAim::kBestCell;

// Une cible d'action qui est une case.
[[nodiscard]] constexpr bool AimsAtCell(RuleAim aim) noexcept {
  return aim == RuleAim::kCellUnderTarget || aim == RuleAim::kCellUnderOwner ||
         aim == RuleAim::kBestCell;
}
// Une cible d'action ennemie : la cible, ce que le maître attaque, ce qui
// l'attaque.
[[nodiscard]] constexpr bool AimsAtEnemy(RuleAim aim) noexcept {
  return aim == RuleAim::kTarget || aim == RuleAim::kOwnerTarget || aim == RuleAim::kOwnerAttacker;
}

struct CompanionRule {
  RuleAction action = RuleAction::kAttack;
  RuleAim aim = RuleAim::kTarget;
  std::uint16_t skill_id = 0;
  std::uint16_t skill_level = 0;
  int step_cells = 0;
  std::vector<RuleCondition> conditions;
  // LES ATTRIBUTS FACULTATIFS. Zéro : aucun délai propre, aucun quota.
  std::uint32_t own_delay_ms = 0;
  int per_target_most = 0;
  bool ignores_sp_reserve = false;

  [[nodiscard]] friend bool operator==(const CompanionRule&, const CompanionRule&) = default;
};

// LES DRAPEAUX DE VISÉE dont un skill doit porter l'un pour être lancé sur
// cette cible d'action. Un skill de soutien se vise aussi sur soi : le serveur
// l'accepte.
[[nodiscard]] std::uint16_t RuleAimTargeting(RuleAim aim) noexcept;

// DANS LE VOCABULAIRE ET SES BORNES, sujet permis pour le prédicat compris.
[[nodiscard]] bool ValidCondition(const RuleCondition& condition) noexcept;
// Une règle entière, ce qui se juge sans le compagnon : de une à trois
// conditions valides, une cible d'action permise pour l'action, et un skill
// qui peut partir d'une règle. Le genre du skill, qui demande son livre, se
// juge à la lecture.
[[nodiscard]] bool ValidRule(const CompanionRule& rule) noexcept;

// UNE RÈGLE LIT UN STATUT DU COMPAGNON : la liste de ses statuts vaut alors
// d'être demandée au serveur.
[[nodiscard]] bool ReadsCompanionStatus(std::span<const CompanionRule> rules) noexcept;

// UN NOM D'AMI recevable : non vide, sans saut de ligne.
[[nodiscard]] bool ValidFriendName(const std::string& name) noexcept;

}  // namespace ro::sim

#pragma once

// ENTRE LE PROFIL DE L'IA ET LA FENÊTRE « IA DU COMPAGNON ». La fenêtre ne
// connaît pas l'IA : elle reçoit des nombres, des rangs de liste et des
// libellés traduits, et rend des intentions. Ici seulement les deux se voient :
// les listes de la fenêtre suivent les énumérations de l'IA, ses bornes sont
// celles de l'IA et du fichier, et le fichier source le vérifie à la
// compilation.

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_rules.h"
#include "ro/sim/companion_skills.h"
#include "ro/ui/companion_ai_window.h"

#include "game/companion_ai_file.h"

namespace moonclient {

// ── RÉGLAGES ────────────────────────────────────────────────────────────────

// Les réglages du niveau 1, une permission par skill offensif montré.
[[nodiscard]] ro::ui::CompanionBaseFields BaseFieldsOf(
    const ro::sim::CompanionAiSettings& settings,
    std::span<const ro::ui::OffensiveSkillRow> offensive);
// Les réglages avec ces champs. Les permissions des skills qui ne sont pas
// montrés restent telles quelles.
[[nodiscard]] ro::sim::CompanionAiSettings WithBaseFields(
    ro::sim::CompanionAiSettings settings, const ro::ui::CompanionBaseFields& fields);

// ── TACTIQUES ───────────────────────────────────────────────────────────────

// La clé d'une ligne générique de la fenêtre, et le rang de la fenêtre d'une
// clé générique (moins de zéro pour une ligne de monstre).
[[nodiscard]] ro::sim::TacticKey GenericKeyOf(int generic) noexcept;
[[nodiscard]] int GenericIndexOf(ro::sim::TacticKeyKind kind) noexcept;

// ── RÈGLES ──────────────────────────────────────────────────────────────────

[[nodiscard]] ro::ui::RuleFields RuleFieldsOf(const ro::sim::CompanionRule& rule);
// La règle que ces champs disent, sans la juger : `ValidRule` le fait.
[[nodiscard]] ro::sim::CompanionRule RuleOf(const ro::ui::RuleFields& fields);
// LA RÈGLE QU'« AJOUTER » POSE : « Si PV du compagnon sous 30 %, alors revenir
// au maître ».
[[nodiscard]] ro::sim::CompanionRule DefaultCompanionRule();

// CE QUI NOMME, pour les phrases et les listes : un skill, un statut, un
// monstre par sa classe, et les mots de la fiche de monstre par leur rang.
struct RuleNames {
  std::function<std::string(std::uint16_t)> skill;
  std::function<std::string(std::uint16_t)> status;
  std::function<std::string(std::uint16_t)> monster;
  std::function<std::string(int)> race;
  std::function<std::string(int)> element;
  std::function<std::string(int)> size;
};

// LA RÈGLE EN UNE PHRASE, traduite : « Si PV du maître < 50 %, alors lancer
// Healing Hands niv. 5 sur le maître ».
[[nodiscard]] std::string RuleSentence(const ro::sim::CompanionRule& rule, const RuleNames& names);

// UNE RÈGLE ÉCARTÉE À LA LECTURE, en une phrase traduite qui dit sa place dans
// le fichier et sa raison.
[[nodiscard]] std::string RejectedRuleLine(const RejectedRule& rejected);

// UN SKILL DU COMPAGNON, tel que les listes des règles le proposent.
struct RuleSkillFact {
  std::uint16_t skill_id = 0;
  int level = 0;
  std::uint16_t targeting = 0;

  [[nodiscard]] friend bool operator==(const RuleSkillFact&, const RuleSkillFact&) = default;
};

// LE VOCABULAIRE DES RÈGLES QUE LA FENÊTRE PROPOSE, et les listes que ses
// conditions déroulent. Les listes vivent ici : les sortes de condition les
// désignent.
//
// ⚠ À REFAIRE EN ENTIER à chaque changement : une liste refaite seule laisserait
// les sortes désigner l'ancienne.
class RuleVocabularyStore {
 public:
  // `statuses` et `monsters` : les identifiants à proposer, dans l'ordre voulu.
  // Ceux que les règles nomment déjà s'y ajoutent.
  // Rend vrai quand il a été refait : seulement quand une entrée a changé.
  bool Build(std::span<const RuleSkillFact> skills, std::span<const std::uint16_t> statuses,
             std::span<const std::uint16_t> monsters, std::span<const ro::sim::CompanionRule> rules,
             const RuleNames& names);
  [[nodiscard]] ro::ui::RuleVocabulary view() const;

 private:
  std::vector<ro::ui::RuleChoice> statuses_;
  std::vector<ro::ui::RuleChoice> cast_skills_;
  std::vector<ro::ui::RuleChoice> monsters_;
  std::vector<ro::ui::RuleChoice> activities_;
  std::vector<ro::ui::RuleChoice> races_;
  std::vector<ro::ui::RuleChoice> elements_;
  std::vector<ro::ui::RuleChoice> sizes_;
  std::vector<ro::ui::RuleChoice> ranks_;
  std::vector<ro::ui::RuleChoice> whom_;
  std::vector<ro::ui::RuleChoice> around_;
  std::vector<ro::ui::RuleConditionKind> conditions_;
  std::vector<ro::ui::RuleActionKind> actions_;
  std::vector<ro::ui::RuleSkillChoice> skills_;
  // Les entrées du dernier vocabulaire bâti.
  bool built_ = false;
  std::vector<RuleSkillFact> seen_skills_;
  std::vector<std::uint16_t> seen_statuses_;
  std::vector<std::uint16_t> seen_monsters_;
  std::vector<std::uint16_t> seen_named_;
  std::size_t seen_language_ = 0;
};

}  // namespace moonclient

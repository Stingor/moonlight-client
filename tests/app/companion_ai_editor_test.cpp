// LES GESTES DE LA FENÊTRE « IA DU COMPAGNON » SUR LE PROFIL : les règles et
// leurs champs inconnus, la remise à zéro qui ne vise que son compagnon, les
// amis, et la fenêtre suivie d'une image à l'autre.

#include "game/companion_ai_editor.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_rules.h"
#include "ro/ui/companion_ai_window.h"

#include "game/companion_ai_fields.h"

using moonclient::AiEditResult;
using moonclient::AiEditSave;
using moonclient::AiEditTarget;
using moonclient::CompanionProfile;
using moonclient::CompanionProfileEdits;
using ro::sim::CompanionRule;
using ro::ui::CompanionAiVerb;
using ro::ui::CompanionAiWindowAction;

namespace {

constexpr std::uint16_t kFilirJob = 6003;
constexpr std::uint16_t kArcherJob = 6017;
constexpr int kNoteHp = 40;

struct Bench {
  CompanionProfile profile;
  CompanionProfileEdits edits;

  [[nodiscard]] AiEditTarget Target() {
    return AiEditTarget{.companion = ro::sim::AiCompanion::kHomunculus,
                        .job = kFilirJob,
                        .profile = &profile,
                        .profile_name = std::string(moonclient::kOwnProfileName),
                        .edits = &edits};
  }
  AiEditResult Do(CompanionAiVerb verb, std::size_t index = 0, std::size_t to = 0) {
    CompanionAiWindowAction action;
    action.verb = verb;
    action.index = index;
    action.to = to;
    return moonclient::ApplyAiEdit(action, Target(), {});
  }
};

// UNE RÈGLE RECONNAISSABLE à son seuil.
[[nodiscard]] CompanionRule RuleAt(int hp_percent) {
  CompanionRule rule = moonclient::DefaultCompanionRule();
  rule.conditions.front().number = hp_percent;
  return rule;
}

}  // namespace

TEST_CASE("Éditeur de l'IA : les champs inconnus suivent leur règle",
          "[app][companion_ai_editor]") {
  Bench bench;
  bench.profile.rules = {RuleAt(10), RuleAt(kNoteHp), RuleAt(60)};
  bench.profile.rule_extras = {"", "note: soin", ""};
  // LA RÈGLE 2 MONTE EN TÊTE : sa note la suit.
  REQUIRE(bench.Do(CompanionAiVerb::kMoveRule, 1, 0).accepted);
  CHECK(bench.profile.rules.front() == RuleAt(kNoteHp));
  CHECK(bench.profile.rule_extras == std::vector<std::string>{"note: soin", "", ""});
  REQUIRE(bench.edits.Find(kFilirJob, moonclient::kOwnProfileName)->rules.has_value());
  CHECK(bench.edits.Find(kFilirJob, moonclient::kOwnProfileName)->rule_extras ==
        bench.profile.rule_extras);
  // UNE COPIE PREND LA NOTE AUSSI ; une suppression l'emporte avec elle.
  REQUIRE(bench.Do(CompanionAiVerb::kDuplicateRule, 0).accepted);
  CHECK(bench.profile.rule_extras == std::vector<std::string>{"note: soin", "note: soin", "", ""});
  REQUIRE(bench.Do(CompanionAiVerb::kRemoveRule, 0).accepted);
  REQUIRE(bench.Do(CompanionAiVerb::kRemoveRule, 0).accepted);
  CHECK(bench.profile.rule_extras == std::vector<std::string>{"", ""});
  CHECK(bench.profile.rules == std::vector<CompanionRule>{RuleAt(10), RuleAt(60)});
}

TEST_CASE("Éditeur de l'IA : une règle refusée dit pourquoi, le plafond aussi",
          "[app][companion_ai_editor]") {
  Bench bench;
  bench.profile.rules = {RuleAt(10)};
  CompanionAiWindowAction edit;
  edit.verb = CompanionAiVerb::kEditRule;
  edit.index = 0;
  edit.rule = moonclient::RuleFieldsOf(RuleAt(10));
  edit.rule.conditions.clear();
  AiEditResult refused = moonclient::ApplyAiEdit(edit, bench.Target(), {});
  CHECK_FALSE(refused.accepted);
  CHECK_FALSE(refused.refusal.empty());
  CHECK(refused.refused_rule == 0);
  CHECK(bench.profile.rules == std::vector<CompanionRule>{RuleAt(10)});
  // Un rang hors de la liste : rien, sans un mot.
  CHECK_FALSE(bench.Do(CompanionAiVerb::kRemoveRule, 3).accepted);
  // AU PLAFOND, dupliquer et ajouter se refusent en le disant.
  while (bench.profile.rules.size() < ro::sim::kCompanionRuleMost) {
    REQUIRE(bench.Do(CompanionAiVerb::kAddRule).accepted);
  }
  refused = bench.Do(CompanionAiVerb::kDuplicateRule, 0);
  CHECK_FALSE(refused.accepted);
  CHECK_FALSE(refused.refusal.empty());
  CHECK_FALSE(bench.Do(CompanionAiVerb::kAddRule).accepted);
  CHECK(bench.profile.rules.size() == ro::sim::kCompanionRuleMost);
  CHECK(bench.profile.rule_extras.size() == ro::sim::kCompanionRuleMost);
}

TEST_CASE("Éditeur de l'IA : repartir de l'IA basique ne vise que son compagnon",
          "[app][companion_ai_editor]") {
  Bench bench;
  bench.profile.rules = {RuleAt(10)};
  bench.profile.rule_extras = {""};
  bench.profile.friends = {"Alice"};
  REQUIRE(bench.profile.tactics.Set(
      ro::sim::CompanionTactic{.key = moonclient::GenericKeyOf(ro::ui::kTacticGenericChests)}));
  // LA CONFIRMATION VISAIT LE MERCENAIRE : rien ne bouge.
  CompanionAiWindowAction reset;
  reset.verb = CompanionAiVerb::kResetToBasic;
  reset.companion_class = kArcherJob;
  AiEditResult result = moonclient::ApplyAiEdit(reset, bench.Target(), {});
  CHECK_FALSE(result.accepted);
  CHECK(bench.profile.rules.size() == 1);
  CHECK(bench.edits.empty());
  // POUR CE COMPAGNON : tout repart, et s'enregistre aussitôt.
  reset.companion_class = kFilirJob;
  result = moonclient::ApplyAiEdit(reset, bench.Target(), {});
  CHECK(result.accepted);
  CHECK(result.save == AiEditSave::kNow);
  CHECK(bench.profile.rules.empty());
  CHECK(bench.profile.friends.empty());
  CHECK(bench.profile.tactics.lines().empty());
  CHECK(moonclient::ProfileIsBasic(ro::sim::AiCompanion::kHomunculus, kFilirJob, bench.profile));
  CHECK(bench.edits.Find(kFilirJob, moonclient::kOwnProfileName)->tactics.size() == 1);
}

TEST_CASE("Éditeur de l'IA : un ami tapé perd ses espaces, et se refuse vide ou en double",
          "[app][companion_ai_editor]") {
  Bench bench;
  CHECK(moonclient::SetProfileFriend(bench.Target(), "  Bob ", true).accepted);
  CHECK(bench.profile.friends == std::vector<std::string>{"Bob"});
  AiEditResult refused = moonclient::SetProfileFriend(bench.Target(), "   ", true);
  CHECK_FALSE(refused.accepted);
  CHECK_FALSE(refused.refusal.empty());
  refused = moonclient::SetProfileFriend(bench.Target(), "Bob", true);
  CHECK_FALSE(refused.accepted);
  CHECK_FALSE(refused.refusal.empty());
  for (std::size_t rank = 1; rank < ro::sim::kCompanionFriendMost; ++rank) {
    REQUIRE(
        moonclient::SetProfileFriend(bench.Target(), "Ami" + std::to_string(rank), true).accepted);
  }
  CHECK_FALSE(moonclient::SetProfileFriend(bench.Target(), "Encore", true).accepted);
  CHECK(moonclient::SetProfileFriend(bench.Target(), "Bob", false).accepted);
  CHECK(bench.profile.friends.size() == ro::sim::kCompanionFriendMost - 1);
}

TEST_CASE("Éditeur de l'IA : sans classe connue, aucun geste ne passe",
          "[app][companion_ai_editor]") {
  Bench bench;
  AiEditTarget unknown = bench.Target();
  unknown.job = 0;
  CompanionAiWindowAction add;
  add.verb = CompanionAiVerb::kAddRule;
  CHECK_FALSE(moonclient::ApplyAiEdit(add, unknown, {}).accepted);
  CHECK(bench.profile.rules.empty());
}

TEST_CASE("Éditeur de l'IA : la fenêtre suivie d'une image à l'autre",
          "[app][companion_ai_editor]") {
  ro::ui::CompanionAiWindowState window;
  moonclient::AiWindowWatch watch;
  window.open = true;
  window.companion = ro::ui::kCompanionAiMercenary;
  CHECK_FALSE(moonclient::FollowAiWindow(window, watch, {true, true}));
  // UNE CONFIRMATION EN ATTENTE SUR LE MERCENAIRE, qui part : l'enregistrement
  // part, l'onglet passe à l'homoncule, et la confirmation tombe.
  window.reset_asked = true;
  window.expanded_rule = 2;
  CHECK(moonclient::FollowAiWindow(window, watch, {true, false}));
  CHECK(window.open);
  CHECK(window.companion == ro::ui::kCompanionAiHomunculus);
  CHECK_FALSE(window.reset_asked);
  CHECK(window.expanded_rule == -1);
  // FERMÉE (croix ou Échap) : l'enregistrement part, l'état repart de zéro.
  window.reset_asked = true;
  window.open = false;
  CHECK(moonclient::FollowAiWindow(window, watch, {true, false}));
  CHECK_FALSE(window.reset_asked);
  CHECK_FALSE(moonclient::FollowAiWindow(window, watch, {true, false}));
  // Plus aucun compagnon : elle se ferme.
  window.open = true;
  static_cast<void>(moonclient::FollowAiWindow(window, watch, {true, false}));
  CHECK(moonclient::FollowAiWindow(window, watch, {false, false}));
  CHECK_FALSE(window.open);
}
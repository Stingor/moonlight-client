// « MON PROFIL » ENTIER DANS LE FICHIER DE L'IA DES COMPAGNONS : réglages,
// règles et amis font l'aller-retour, et une règle écartée reste dans le
// fichier.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_rules.h"
#include "ro/sim/companion_tactics.h"

#include "game/companion_ai_file.h"

using moonclient::CompanionAiFileRead;
using moonclient::CompanionProfile;
using moonclient::CompanionProfiles;
using ro::sim::CompanionRule;
using ro::sim::RuleAction;
using ro::sim::RuleAim;
using ro::sim::RuleComparison;
using ro::sim::RuleCondition;
using ro::sim::RulePredicate;
using ro::sim::RuleSubject;

namespace {

constexpr std::uint16_t kFilirJob = 6003;
constexpr std::uint16_t kHealingHandsId = 8001;
constexpr std::uint16_t kMoonlightId = 8009;
constexpr int kLowHpPercent = 50;
constexpr int kTwoEnemies = 2;
constexpr int kNearCells = 3;
// Le plafond absolu des distances des réglages, celui du fichier.
constexpr int kSettingCellsCap = 50;
constexpr std::uint32_t kRuleDelayMs = 10000;

[[nodiscard]] CompanionRule HealOwner() {
  return CompanionRule{.action = RuleAction::kCastSkill,
                       .aim = RuleAim::kOwner,
                       .skill_id = kHealingHandsId,
                       .skill_level = 3,
                       .conditions = {RuleCondition{.subject = RuleSubject::kOwner,
                                                    .predicate = RulePredicate::kHpPercent,
                                                    .comparison = RuleComparison::kBelow,
                                                    .number = kLowHpPercent}},
                       .own_delay_ms = kRuleDelayMs,
                       .ignores_sp_reserve = true};
}

[[nodiscard]] CompanionRule MoonlightCrowd() {
  return CompanionRule{
      .action = RuleAction::kCastSkill,
      .aim = RuleAim::kTarget,
      .skill_id = kMoonlightId,
      .conditions = {RuleCondition{.subject = RuleSubject::kSurroundings,
                                   .predicate = RulePredicate::kEnemiesNear,
                                   .comparison = RuleComparison::kIs,
                                   .number = kTwoEnemies,
                                   .cells = kNearCells,
                                   .choice = static_cast<int>(ro::sim::RuleAround::kCompanion)},
                     RuleCondition{.subject = RuleSubject::kTarget,
                                   .predicate = RulePredicate::kMonsterRank,
                                   .comparison = RuleComparison::kIsNot,
                                   .choice = 2}},
      .per_target_most = 2};
}

}  // namespace

TEST_CASE("Profil de compagnon : réglages, règles et amis font l'aller-retour",
          "[app][companion_ai_profile]") {
  CompanionProfiles profiles;
  CompanionProfile& filir = profiles.At(kFilirJob, moonclient::kOwnProfileName);
  ro::sim::CompanionAiSettings settings =
      ro::sim::BasicCompanionAiSettings(ro::sim::AiCompanion::kHomunculus, kFilirJob);
  settings.follow_distance_cells = 5;
  settings.assist_owner = true;
  settings.owner_seated = ro::sim::OwnerSeated::kStayBesidePassive;
  settings.skills.offensive = ro::sim::OffensiveSkills::kSkillsOnly;
  settings.skills.sp_reserve_points = 40;
  settings.skills.permissions = {ro::sim::SkillPermission{.skill_id = kMoonlightId, .level = 2}};
  filir.settings = settings;
  filir.rules = {HealOwner(), MoonlightCrowd()};
  // Une règle lue porte ses champs inconnus : aucun ici.
  filir.rule_extras = {"", ""};
  filir.friends = {"Alice", "Bob le Bricoleur"};

  const std::string text = moonclient::FormatCompanionAi(profiles);
  CHECK(text.find("version: 1") != std::string::npos);
  CHECK(text.find("rules:") != std::string::npos);
  const CompanionAiFileRead read = moonclient::ParseCompanionAi(text);
  CHECK(read.rejected.empty());
  CHECK(read.profiles == profiles);
}

TEST_CASE("Profil de compagnon : une règle écartée reste à sa place dans le fichier",
          "[app][companion_ai_profile]") {
  const std::string current =
      "version: 1\n"
      "types:\n"
      "  - class: 6003\n"
      "    profiles:\n"
      "      - name: My profile\n"
      "        rules:\n"
      "          - action: dance\n"
      "            when: []\n"
      "          - action: do_nothing\n"
      "            when:\n"
      "              - subject: owner\n"
      "                predicate: sitting\n"
      "                comparison: is\n";
  const CompanionAiFileRead read = moonclient::ParseCompanionAi(current);
  REQUIRE(read.profiles.Find(kFilirJob, moonclient::kOwnProfileName) != nullptr);
  const CompanionProfile& filir = *read.profiles.Find(kFilirJob, moonclient::kOwnProfileName);
  REQUIRE(filir.rules.size() == 1);
  CHECK(filir.rules.front().action == RuleAction::kDoNothing);
  REQUIRE(filir.rejected_rules.size() == 1);
  CHECK(filir.rejected_rules.front().position == 0);
  CHECK(filir.rejected_rules.front().why == moonclient::RuleRejection::kUnknownWord);
  CHECK(filir.rejected_rules.front().field == "action");
  CHECK(read.rejected.size() == 1);

  moonclient::CompanionProfileEdits edits;
  edits.SetRules(kFilirJob, moonclient::kOwnProfileName, {HealOwner(), MoonlightCrowd()});
  const std::optional<std::string> merged = moonclient::MergeCompanionAi(current, edits);
  REQUIRE(merged.has_value());
  CHECK(merged->find("action: dance") != std::string::npos);
  const CompanionAiFileRead again = moonclient::ParseCompanionAi(*merged);
  const CompanionProfile& rewritten = *again.profiles.Find(kFilirJob, moonclient::kOwnProfileName);
  CHECK(rewritten.rules == std::vector<CompanionRule>{HealOwner(), MoonlightCrowd()});
  REQUIRE(rewritten.rejected_rules.size() == 1);
  CHECK(rewritten.rejected_rules.front().position == 0);
}

TEST_CASE("Profil de compagnon : un réglage absent ou hors du cadre garde l'IA basique",
          "[app][companion_ai_profile]") {
  const CompanionAiFileRead read = moonclient::ParseCompanionAi(
      "version: 1\n"
      "types:\n"
      "  - class: 6003\n"
      "    profiles:\n"
      "      - name: My profile\n"
      "        settings:\n"
      "          follow_cells: 6\n"
      "          flee_below_hp_percent: 250\n"
      "        friends: [Alice, [pas un nom]]\n");
  REQUIRE(read.profiles.Find(kFilirJob, moonclient::kOwnProfileName) != nullptr);
  const CompanionProfile& filir = *read.profiles.Find(kFilirJob, moonclient::kOwnProfileName);
  REQUIRE(filir.settings.has_value());
  CHECK(filir.settings->follow_distance_cells == 6);
  CHECK(filir.settings->flee_below_hp_percent == 0);
  // Filir attaque de lui-même dans l'IA basique, et le fichier n'en dit rien.
  CHECK(filir.settings->attack_on_own);
  CHECK(filir.friends == std::vector<std::string>{"Alice"});
  CHECK(read.rejected.size() == 2);

  const ro::sim::CompanionAiSettings applied =
      moonclient::SettingsOfProfile(ro::sim::AiCompanion::kHomunculus, kFilirJob, &filir);
  CHECK(applied.follow_distance_cells == 6);
  CHECK(applied.friends == filir.friends);
}

TEST_CASE("Profil de compagnon : une distance au-delà du plafond y est ramenée, pas en deçà",
          "[app][companion_ai_profile]") {
  const CompanionAiFileRead read = moonclient::ParseCompanionAi(
      "version: 1\n"
      "types:\n"
      "  - class: 6003\n"
      "    profiles:\n"
      "      - name: My profile\n"
      "        settings:\n"
      "          follow_cells: 99\n"
      "          leash_still_cells: 19\n"
      "          engage_walking_cells: 18\n"
      "          engage_still_cells: 7\n"
      "        rules:\n"
      "          - action: step_away\n"
      "            aim: target\n"
      "            step_cells: 30\n"
      "            when:\n"
      "              - subject: surroundings\n"
      "                predicate: enemies_near\n"
      "                comparison: is\n"
      "                number: 2\n"
      "                cells: 25\n"
      "                choice: companion\n");
  const CompanionProfile* filir = read.profiles.Find(kFilirJob, moonclient::kOwnProfileName);
  REQUIRE(filir != nullptr);
  REQUIRE(filir->settings.has_value());
  CHECK(filir->settings->follow_distance_cells == kSettingCellsCap);
  // EN DEÇÀ DU PLAFOND, la valeur du joueur reste, même au-delà du rayon de vue
  // du serveur : c'est l'IA qui la borne en jeu.
  CHECK(filir->settings->leash_still_cells == 19);
  CHECK(filir->settings->engage_walking_cells == 18);
  CHECK(filir->settings->engage_still_cells == 7);
  CHECK(read.rejected.empty());
  REQUIRE(filir->rules.size() == 1);
  CHECK(filir->rejected_rules.empty());
  CHECK(filir->rules.front().step_cells == ro::sim::kRuleCellsMost);
  REQUIRE(filir->rules.front().conditions.size() == 1);
  CHECK(filir->rules.front().conditions.front().cells == ro::sim::kRuleCellsMost);
}

TEST_CASE("Profil de compagnon : au-delà des plafonds, règles et amis restent dans le fichier",
          "[app][companion_ai_profile]") {
  std::string current =
      "version: 1\n"
      "types:\n"
      "  - class: 6003\n"
      "    profiles:\n"
      "      - name: My profile\n"
      "        rules:\n";
  // Une règle de plus que le plafond, la dernière marquée.
  for (std::size_t rank = 0; rank <= ro::sim::kCompanionRuleMost; ++rank) {
    current += "          - action: do_nothing\n";
    current += rank == ro::sim::kCompanionRuleMost ? "            mark: extra\n" : "";
    current +=
        "            when:\n"
        "              - subject: owner\n"
        "                predicate: sitting\n"
        "                comparison: is\n";
  }
  current += "        friends:\n";
  for (std::size_t rank = 0; rank <= ro::sim::kCompanionFriendMost; ++rank) {
    current += "          - Friend" + std::to_string(rank) + "\n";
  }
  const CompanionAiFileRead read = moonclient::ParseCompanionAi(current);
  const CompanionProfile& filir = *read.profiles.Find(kFilirJob, moonclient::kOwnProfileName);
  CHECK(filir.rules.size() == ro::sim::kCompanionRuleMost);
  REQUIRE(filir.rejected_rules.size() == 1);
  CHECK(filir.rejected_rules.front().why == moonclient::RuleRejection::kBeyondLimit);
  CHECK(filir.friends.size() == ro::sim::kCompanionFriendMost);
  CHECK(std::ranges::none_of(read.rejected, [](const std::string& why) {
    return why.find("illisible") != std::string::npos;
  }));

  moonclient::CompanionProfileEdits edits;
  edits.SetRules(kFilirJob, moonclient::kOwnProfileName, filir.rules);
  edits.SetFriends(kFilirJob, moonclient::kOwnProfileName, filir.friends);
  const std::optional<std::string> merged = moonclient::MergeCompanionAi(current, edits);
  REQUIRE(merged.has_value());
  const std::string last_friend = "Friend" + std::to_string(ro::sim::kCompanionFriendMost);
  CHECK(merged->find("mark: extra") != std::string::npos);
  CHECK(merged->find(last_friend) != std::string::npos);
}

TEST_CASE("Profil de compagnon : un réglage hors du cadre reste tel quel dans le fichier",
          "[app][companion_ai_profile]") {
  const std::string current =
      "version: 1\n"
      "types:\n"
      "  - class: 6003\n"
      "    profiles:\n"
      "      - name: My profile\n"
      "        settings:\n"
      "          follow_cells: 6\n"
      "          flee_below_hp_percent: 250\n"
      "          from_a_later_client: yes\n";
  const CompanionAiFileRead read = moonclient::ParseCompanionAi(current);
  ro::sim::CompanionAiSettings settings =
      *read.profiles.Find(kFilirJob, moonclient::kOwnProfileName)->settings;
  settings.follow_distance_cells = 4;
  moonclient::CompanionProfileEdits edits;
  edits.SetSettings(kFilirJob, moonclient::kOwnProfileName, settings);
  std::optional<std::string> merged = moonclient::MergeCompanionAi(current, edits);
  REQUIRE(merged.has_value());
  CHECK(merged->find("follow_cells: 4") != std::string::npos);
  CHECK(merged->find("flee_below_hp_percent: 250") != std::string::npos);
  CHECK(merged->find("from_a_later_client: yes") != std::string::npos);
  // CHANGÉ EN JEU, il s'écrit.
  settings.flee_below_hp_percent = 20;
  edits.SetSettings(kFilirJob, moonclient::kOwnProfileName, settings);
  merged = moonclient::MergeCompanionAi(current, edits);
  REQUIRE(merged.has_value());
  CHECK(merged->find("flee_below_hp_percent: 20") != std::string::npos);
}

TEST_CASE(
    "Profil de compagnon : une règle réécrite garde ses champs inconnus, et ses choix en mots",
    "[app][companion_ai_profile]") {
  const std::string current =
      "version: 1\n"
      "types:\n"
      "  - class: 6003\n"
      "    profiles:\n"
      "      - name: My profile\n"
      "        rules:\n"
      "          - action: do_nothing\n"
      "            from_a_later_client: 7\n"
      "            when:\n"
      "              - subject: companion\n"
      "                predicate: activity\n"
      "                comparison: is\n"
      "                choice: 2\n";
  const CompanionAiFileRead read = moonclient::ParseCompanionAi(current);
  const CompanionProfile& filir = *read.profiles.Find(kFilirJob, moonclient::kOwnProfileName);
  REQUIRE(filir.rules.size() == 1);
  // UN RANG ÉCRIT EN NOMBRE se relit encore.
  CHECK(filir.rules.front().conditions.front().choice ==
        static_cast<int>(ro::sim::CompanionActivity::kFighting));
  moonclient::CompanionProfileEdits edits;
  edits.SetRules(kFilirJob, moonclient::kOwnProfileName, filir.rules);
  const std::optional<std::string> merged = moonclient::MergeCompanionAi(current, edits);
  REQUIRE(merged.has_value());
  CHECK(merged->find("from_a_later_client: 7") != std::string::npos);
  CHECK(merged->find("choice: fighting") != std::string::npos);
  CHECK(moonclient::ParseCompanionAi(*merged).profiles == read.profiles);
}

TEST_CASE("Profil de compagnon : les champs inconnus d'une règle la suivent quand elle bouge",
          "[app][companion_ai_profile]") {
  const std::string current =
      "version: 1\n"
      "types:\n"
      "  - class: 6003\n"
      "    profiles:\n"
      "      - name: My profile\n"
      "        rules:\n"
      "          - action: do_nothing\n"
      "            when:\n"
      "              - subject: owner\n"
      "                predicate: sitting\n"
      "                comparison: is\n"
      "          - action: return_to_owner\n"
      "            note: soin d'urgence\n"
      "            when:\n"
      "              - subject: owner\n"
      "                predicate: sitting\n"
      "                comparison: is_not\n";
  const CompanionAiFileRead read = moonclient::ParseCompanionAi(current);
  const CompanionProfile& filir = *read.profiles.Find(kFilirJob, moonclient::kOwnProfileName);
  REQUIRE(filir.rules.size() == 2);
  REQUIRE(filir.rule_extras.size() == 2);
  CHECK(filir.rule_extras.front().empty());
  CHECK(filir.rule_extras.back().find("note") != std::string::npos);
  // LA SECONDE RÈGLE PASSE EN TÊTE, avec sa note.
  moonclient::CompanionProfileEdits edits;
  edits.SetRules(kFilirJob, moonclient::kOwnProfileName, {filir.rules.back(), filir.rules.front()},
                 {filir.rule_extras.back(), filir.rule_extras.front()});
  std::optional<std::string> merged = moonclient::MergeCompanionAi(current, edits);
  REQUIRE(merged.has_value());
  const CompanionProfile moved =
      *moonclient::ParseCompanionAi(*merged).profiles.Find(kFilirJob, moonclient::kOwnProfileName);
  REQUIRE(moved.rules.size() == 2);
  CHECK(moved.rules.front().action == RuleAction::kReturnToOwner);
  CHECK(moved.rule_extras.front().find("soin d'urgence") != std::string::npos);
  CHECK(moved.rule_extras.back().empty());
  // SUPPRIMÉE, sa note disparaît avec elle.
  edits.SetRules(kFilirJob, moonclient::kOwnProfileName, {filir.rules.front()},
                 {filir.rule_extras.front()});
  merged = moonclient::MergeCompanionAi(current, edits);
  REQUIRE(merged.has_value());
  CHECK(merged->find("note") == std::string::npos);
}

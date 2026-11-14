// LE FICHIER DES DÉLAIS DE L'IA DES COMPAGNONS : l'aller-retour, le temps passé
// hors du jeu qui vieillit la mémoire, la fusion qui garde les autres, et un
// fichier qu'on ne comprend pas qui ne se réécrit pas.

#include "game/companion_ai_state_file.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_memory.h"
#include "ro/sim/companion_rules.h"

using moonclient::CharacterKey;
using moonclient::StoredCompanionState;
using ro::sim::CompanionRuleMemory;
using ro::sim::CompanionSkillMemory;

namespace {

constexpr std::uint16_t kLifJob = 6001;
constexpr std::uint16_t kArcherJob = 6017;
constexpr std::int64_t kWrittenWallMs = 1760000000000;
constexpr std::uint32_t kActedAgoMs = 4000;
constexpr std::uint32_t kSuspendedForMs = 3000;
constexpr std::uint16_t kBuffId = 8002;

[[nodiscard]] ro::sim::CompanionRule Buff() {
  return ro::sim::CompanionRule{
      .action = ro::sim::RuleAction::kCastSkill,
      .aim = ro::sim::RuleAim::kSelf,
      .skill_id = kBuffId,
      .conditions = {ro::sim::RuleCondition{.subject = ro::sim::RuleSubject::kCompanion,
                                            .predicate = ro::sim::RulePredicate::kCastWithin,
                                            .comparison = ro::sim::RuleComparison::kIsNot,
                                            .seconds = 60,
                                            .id = kBuffId}},
      .own_delay_ms = 10000};
}

[[nodiscard]] StoredCompanionState LifState() {
  return StoredCompanionState{
      .job = kLifJob,
      .written_wall_ms = kWrittenWallMs,
      .standby = true,
      .skills = {CompanionSkillMemory{
          .skill_id = kBuffId, .cast_ago_ms = kActedAgoMs, .suspended_for_ms = kSuspendedForMs}},
      .rules_by_profile = {{"My profile",
                            {CompanionRuleMemory{.index = 2,
                                                 .rule = Buff(),
                                                 .acted_ago_ms = kActedAgoMs,
                                                 .suspended_for_ms = kSuspendedForMs}}}}};
}

}  // namespace

TEST_CASE("Délais des compagnons : l'aller-retour du fichier", "[app][companion_state]") {
  const CharacterKey me{.cid = 150001, .name = "Stingor"};
  const std::optional<std::string> text = moonclient::MergeCompanionState("", me, {LifState()});
  REQUIRE(text.has_value());
  const moonclient::CompanionStateRead read = moonclient::ParseCompanionState(*text);
  CHECK_FALSE(read.unreadable);
  REQUIRE(read.characters.size() == 1);
  CHECK(read.characters.front().key == me);
  REQUIRE(read.characters.front().companions.size() == 1);
  CHECK(read.characters.front().companions.front() == LifState());
}

TEST_CASE("Délais des compagnons : l'absence vieillit la mémoire", "[app][companion_state]") {
  const StoredCompanionState state = LifState();
  SECTION("une absence plus courte que la suspension : tout tient") {
    constexpr std::int64_t kAwayMs = 1000;
    const ro::sim::CompanionMemory memory =
        moonclient::MemoryOf(state, "My profile", kWrittenWallMs + kAwayMs);
    REQUIRE(memory.rules.size() == 1);
    CHECK(memory.rules.front().acted_ago_ms == kActedAgoMs + kAwayMs);
    CHECK(memory.rules.front().suspended_for_ms == kSuspendedForMs - kAwayMs);
    CHECK(memory.rules.front().index == 2);
    CHECK(memory.standby);
  }
  SECTION("une suspension échue pendant l'absence a disparu") {
    constexpr std::int64_t kAwayMs = 5000;
    const ro::sim::CompanionMemory memory =
        moonclient::MemoryOf(state, "My profile", kWrittenWallMs + kAwayMs);
    REQUIRE(memory.rules.size() == 1);
    CHECK_FALSE(memory.rules.front().suspended_for_ms.has_value());
    REQUIRE(memory.skills.size() == 1);
    CHECK_FALSE(memory.skills.front().suspended_for_ms.has_value());
  }
  SECTION("au-delà de l'horizon, plus rien") {
    const ro::sim::CompanionMemory memory = moonclient::MemoryOf(
        state, "My profile", kWrittenWallMs + ro::sim::kCompanionMemoryHorizonMs);
    CHECK(memory.rules.empty());
    CHECK(memory.skills.empty());
  }
  SECTION("une horloge revenue en arrière ne rajeunit rien") {
    const ro::sim::CompanionMemory memory =
        moonclient::MemoryOf(state, "My profile", kWrittenWallMs - kActedAgoMs);
    REQUIRE(memory.rules.size() == 1);
    CHECK(memory.rules.front().acted_ago_ms == kActedAgoMs);
  }
  SECTION("un autre profil n'a pas ces règles") {
    CHECK(moonclient::MemoryOf(state, "Farm", kWrittenWallMs).rules.empty());
  }
}

TEST_CASE("Délais des compagnons : la fusion garde les autres et les nœuds inconnus",
          "[app][companion_state]") {
  const std::string current =
      "version: 1\n"
      "from_elsewhere: keep-me\n"
      "characters:\n"
      "  - cid: 150002\n"
      "    name: Autre\n"
      "    companions:\n"
      "      - class: 6001\n"
      "        written_at_ms: 5\n"
      "        standby: false\n"
      "  - cid: 150001\n"
      "    name: Stingor\n"
      "    hint: kept\n"
      "    companions:\n"
      "      - class: 6017\n"
      "        written_at_ms: 7\n"
      "        standby: true\n"
      "      - class: 6001\n"
      "        written_at_ms: 9\n"
      "        standby: false\n";
  const CharacterKey me{.cid = 150001, .name = "Stingor"};
  const std::optional<std::string> merged =
      moonclient::MergeCompanionState(current, me, {LifState()});
  REQUIRE(merged.has_value());
  CHECK(merged->find("from_elsewhere: keep-me") != std::string::npos);
  CHECK(merged->find("hint: kept") != std::string::npos);
  const moonclient::CompanionStateRead read = moonclient::ParseCompanionState(*merged);
  REQUIRE(read.characters.size() == 2);
  CHECK(read.characters.front().key.cid == 150002);
  const std::vector<StoredCompanionState>& mine = read.characters.back().companions;
  REQUIRE(mine.size() == 2);
  // Le mercenaire, que cette écriture ne nomme pas, reste tel quel.
  CHECK(mine.front().job == kArcherJob);
  CHECK(mine.front().written_wall_ms == 7);
  CHECK(mine.back() == LifState());

  // UN FICHIER QU'ON NE COMPREND PAS NE SE RÉÉCRIT PAS.
  CHECK_FALSE(moonclient::MergeCompanionState("version: 99\n", me, {LifState()}).has_value());
  CHECK_FALSE(moonclient::MergeCompanionState("version: [1\n", me, {LifState()}).has_value());
  CHECK(moonclient::ParseCompanionState("version: 99\n").unknown_version);
}

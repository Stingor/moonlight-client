// L'OBJECTIF DU CODEX : le sort épinglé, et ce qui y mène, qu'une offre du
// choix de niveau signale.

#include "game/survivors_objective.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_state.h"

#include "survivors/survivors_save.h"

namespace {

using ro::sim::OfferKind;
using ro::sim::SurvivorsOffer;

constexpr std::uint16_t kNapalmBeatId = 11;
constexpr std::uint16_t kSoulStrikeId = 13;
constexpr std::uint16_t kFireBoltId = 19;
constexpr std::uint16_t kNapalmVulcanId = 400;
constexpr std::uint16_t kBashId = 5;
constexpr std::uint16_t kHeadCrushId = 398;
constexpr std::uint16_t kGolemCardId = 4072;
constexpr std::uint16_t kHornetCardId = 4019;

[[nodiscard]] bool Has(const std::vector<std::uint16_t>& ids, std::uint16_t id) {
  return std::ranges::find(ids, id) != ids.end();
}

}  // namespace

TEST_CASE("Objectif survivors : une union mène à ses deux sorts", "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const moonclient::SurvivorsObjectivePath path =
      moonclient::SurvivorsObjectivePathOf(rules, kNapalmVulcanId);
  CHECK(Has(path.skill_ids, kNapalmVulcanId));
  CHECK(Has(path.skill_ids, kSoulStrikeId));
  CHECK(Has(path.skill_ids, kNapalmBeatId));
  CHECK_FALSE(Has(path.skill_ids, kFireBoltId));
  CHECK(path.card_item_ids.empty());

  CHECK(moonclient::OfferServesObjective(
      path, SurvivorsOffer{.kind = OfferKind::kSkillLevel, .id = kNapalmBeatId, .next_level = 4}));
  CHECK(moonclient::OfferServesObjective(
      path, SurvivorsOffer{.kind = OfferKind::kNewSkill, .id = kSoulStrikeId, .next_level = 1}));
  CHECK_FALSE(moonclient::OfferServesObjective(
      path, SurvivorsOffer{.kind = OfferKind::kNewSkill, .id = kFireBoltId, .next_level = 1}));
  CHECK_FALSE(moonclient::OfferServesObjective(path, SurvivorsOffer{.kind = OfferKind::kHeal}));
}

TEST_CASE("Objectif survivors : un évolué mène à son sort et à sa carte", "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const moonclient::SurvivorsObjectivePath path =
      moonclient::SurvivorsObjectivePathOf(rules, kHeadCrushId);
  CHECK(Has(path.skill_ids, kBashId));
  CHECK(path.card_item_ids == std::vector<std::uint16_t>{kGolemCardId});
  CHECK(moonclient::OfferServesObjective(
      path, SurvivorsOffer{.kind = OfferKind::kNewCard, .id = kGolemCardId, .next_level = 1}));
  CHECK_FALSE(moonclient::OfferServesObjective(
      path, SurvivorsOffer{.kind = OfferKind::kCardLevel, .id = kHornetCardId, .next_level = 2}));
  // Une carte et un sort ne se confondent pas par leur Id.
  CHECK_FALSE(moonclient::OfferServesObjective(
      path, SurvivorsOffer{.kind = OfferKind::kNewSkill, .id = kGolemCardId, .next_level = 1}));
}

TEST_CASE("Objectif survivors : un sort de base ne mène qu'à lui, aucun ne mène à rien",
          "[app][survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const moonclient::SurvivorsObjectivePath fire_bolt =
      moonclient::SurvivorsObjectivePathOf(rules, kFireBoltId);
  CHECK(fire_bolt.skill_ids == std::vector<std::uint16_t>{kFireBoltId});
  CHECK(fire_bolt.card_item_ids.empty());
  const moonclient::SurvivorsObjectivePath none = moonclient::SurvivorsObjectivePathOf(rules, 0);
  CHECK(none.skill_ids.empty());
  CHECK_FALSE(moonclient::OfferServesObjective(
      none, SurvivorsOffer{.kind = OfferKind::kNewSkill, .id = kFireBoltId, .next_level = 1}));
}

TEST_CASE("Objectif survivors : le carnet le garde", "[app][survivors]") {
  moonclient::SurvivorsSave save;
  save.options.objective_skill_id = kNapalmVulcanId;
  const ro::core::Result<moonclient::SurvivorsSave> read =
      moonclient::ReadSurvivorsSave(moonclient::WriteSurvivorsSave(save));
  REQUIRE(read.has_value());
  CHECK(read->options.objective_skill_id == kNapalmVulcanId);
}

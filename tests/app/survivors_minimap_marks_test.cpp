// Les symboles de la minimap : ce qui en a un, dans quel ordre, à quel écart.

#include "survivors/survivors_minimap_marks.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "survivors/survivors_gems.h"

using ro::sim::FieldPoint;
using ro::sim::kSubunitsPerCell;
using ro::sim::PickupKind;
using ro::ui::SurvivorsMinimapDot;
using ro::ui::SurvivorsMinimapMark;

namespace {

// Baphomet, le boss final ; Mastering, un mini-boss ; Poring, un commun.
constexpr std::uint16_t kBossFoeId = 1039;
constexpr std::uint16_t kMiniBossFoeId = 1090;
constexpr std::uint16_t kCommonFoeId = 1002;
constexpr std::int32_t kLivingHp = 10;

[[nodiscard]] FieldPoint CellsFromHero(std::int32_t east, std::int32_t north) {
  return FieldPoint{.x = east * kSubunitsPerCell, .y = north * kSubunitsPerCell};
}

}  // namespace

TEST_CASE("Minimap : plantes, objets, cartes, coffres, Emperium, mini-boss puis boss",
          "[app][survivors][minimap]") {
  ro::sim::SurvivorsField field;
  field.hero.position = CellsFromHero(0, 0);
  field.foes.push_back({.foe_id = kBossFoeId, .position = CellsFromHero(1, 0), .hp = kLivingHp});
  field.foes.push_back({.foe_id = kCommonFoeId, .position = CellsFromHero(2, 0), .hp = kLivingHp});
  field.foes.push_back(
      {.foe_id = kMiniBossFoeId, .position = CellsFromHero(3, 0), .hp = kLivingHp});
  field.pickups.push_back({.kind = PickupKind::kChest, .position = CellsFromHero(4, 0)});
  field.pickups.push_back({.kind = PickupKind::kGold, .position = CellsFromHero(5, 0)});
  field.pickups.push_back({.kind = PickupKind::kCard, .position = CellsFromHero(6, 0)});
  field.gems.push_back({.position = CellsFromHero(7, 0), .exp = moonclient::kEmperiumFromExp});
  field.gems.push_back({.position = CellsFromHero(8, 0), .exp = 1});
  field.plants.push_back({.position = CellsFromHero(9, -3)});

  std::vector<SurvivorsMinimapDot> dots;
  moonclient::SurvivorsMinimapDotsOf(field, ro::sim::ShippedSurvivorsRules(), dots);
  // Ni le Poring ni la gemme bleue.
  REQUIRE(dots.size() == 7);
  CHECK(dots[0].mark == SurvivorsMinimapMark::kPlant);
  CHECK(dots[0].offset_cells.x == 9.0F);
  CHECK(dots[0].offset_cells.y == -3.0F);
  CHECK(dots[1].mark == SurvivorsMinimapMark::kItem);
  CHECK(dots[2].mark == SurvivorsMinimapMark::kCard);
  CHECK(dots[3].mark == SurvivorsMinimapMark::kChest);
  CHECK(dots[4].mark == SurvivorsMinimapMark::kEmperium);
  CHECK(dots[5].mark == SurvivorsMinimapMark::kMiniBoss);
  CHECK(dots[6].mark == SurvivorsMinimapMark::kBoss);
  CHECK(dots[6].offset_cells.x == 1.0F);
}

TEST_CASE("Minimap : un allié debout se marque, pas un allié à terre",
          "[app][survivors][minimap]") {
  ro::sim::SurvivorsField field;
  field.hero.position = ro::sim::FieldPoint{.x = 0, .y = 0};
  field.allies.resize(2);
  field.allies[0].hero.position = ro::sim::FieldPoint{.x = ro::sim::kSubunitsPerCell * 3, .y = 0};
  field.allies[1].hero.position = ro::sim::FieldPoint{.x = ro::sim::kSubunitsPerCell * 5, .y = 0};
  field.allies[1].hero.down = true;
  std::vector<SurvivorsMinimapDot> dots;
  moonclient::SurvivorsMinimapDotsOf(field, ro::sim::ShippedSurvivorsRules(), dots);
  REQUIRE(dots.size() == 1);
  CHECK(dots[0].mark == SurvivorsMinimapMark::kAlly);
  CHECK(dots[0].offset_cells.x == 3.0F);
}

TEST_CASE("Minimap : un boss mort ne se marque plus", "[app][survivors][minimap]") {
  ro::sim::SurvivorsField field;
  field.foes.push_back({.foe_id = kBossFoeId, .position = CellsFromHero(1, 0), .hp = 0});
  std::vector<SurvivorsMinimapDot> dots{{}};
  moonclient::SurvivorsMinimapDotsOf(field, ro::sim::ShippedSurvivorsRules(), dots);
  CHECK(dots.empty());
}

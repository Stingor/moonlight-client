// LES ZONES DE L'IA DU COMPAGNON AU SOL : la forme est la règle de distance de
// l'IA (disque pour le suivi et l'éloignement, carré pour l'engagement), le
// rayon suit son réglage, et celui « en marche » quand le maître marche ; la
// case gardée se marque, un survol ne garde qu'une zone, plus épaisse, et la
// case à cocher les garde fenêtre fermée.

#include "game/companion_ai_zones.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_sight.h"

using moonclient::CompanionZone;
using moonclient::CompanionZoneKind;
using moonclient::CompanionZonesAsked;
using moonclient::CompanionZoneShape;
using moonclient::CompanionZonesPlan;
using moonclient::ZoneCellPaint;
using ro::ui::CompanionAiZoneSlider;

namespace {

constexpr std::uint16_t kOwnerX = 150;
constexpr std::uint16_t kOwnerY = 120;
constexpr int kFollowCells = 3;
constexpr int kLeashStillCells = 8;
constexpr int kLeashWalkingCells = 5;
constexpr int kEngageStillCells = 6;
constexpr int kEngageWalkingCells = 4;
// Au-delà du rayon, de quoi voir que la zone s'arrête.
constexpr int kBeyondCells = 2;

[[nodiscard]] moonclient::CompanionZoneRadii Radii() {
  return moonclient::CompanionZoneRadii{.follow_cells = kFollowCells,
                                        .leash_still_cells = kLeashStillCells,
                                        .leash_walking_cells = kLeashWalkingCells,
                                        .engage_still_cells = kEngageStillCells,
                                        .engage_walking_cells = kEngageWalkingCells};
}

[[nodiscard]] const CompanionZone* Find(const CompanionZonesPlan& plan, CompanionZoneKind kind) {
  const auto found = std::ranges::find(plan.zones, kind, &CompanionZone::kind);
  return found == plan.zones.end() ? nullptr : &*found;
}

[[nodiscard]] bool Painted(const std::vector<ZoneCellPaint>& paints, int dx, int dy) {
  return std::ranges::any_of(paints, [dx, dy](const ZoneCellPaint& paint) {
    return paint.cell_x == kOwnerX + dx && paint.cell_y == kOwnerY + dy;
  });
}

[[nodiscard]] std::vector<ZoneCellPaint> PaintsOf(const CompanionZonesPlan& plan) {
  std::vector<ZoneCellPaint> paints;
  moonclient::PaintCompanionZones(plan, ro::sim::CellPosition{kOwnerX, kOwnerY, 0}, paints);
  return paints;
}

}  // namespace

TEST_CASE("Zones de l'IA : chaque forme est la règle de distance de l'IA", "[app][companion_ai]") {
  CHECK(moonclient::ShapeOfZone(CompanionZoneKind::kFollow) == CompanionZoneShape::kDisc);
  CHECK(moonclient::ShapeOfZone(CompanionZoneKind::kLeash) == CompanionZoneShape::kDisc);
  CHECK(moonclient::ShapeOfZone(CompanionZoneKind::kEngage) == CompanionZoneShape::kSquare);
  const ro::sim::CellPosition owner{kOwnerX, kOwnerY, 0};
  for (const int radius : {0, 1, kFollowCells, kLeashStillCells}) {
    for (int dy = -radius - kBeyondCells; dy <= radius + kBeyondCells; ++dy) {
      for (int dx = -radius - kBeyondCells; dx <= radius + kBeyondCells; ++dx) {
        const ro::sim::CellPosition cell{static_cast<std::uint16_t>(kOwnerX + dx),
                                         static_cast<std::uint16_t>(kOwnerY + dy), 0};
        // LE SUIVI ET L'ÉLOIGNEMENT : la distance euclidienne tronquée de l'IA.
        CHECK(moonclient::ZoneHolds(CompanionZoneShape::kDisc, radius, dx, dy) ==
              (ro::sim::WholeCellDistance(owner, cell) <= radius));
        // L'ENGAGEMENT : le carré du serveur.
        CHECK(moonclient::ZoneHolds(CompanionZoneShape::kSquare, radius, dx, dy) ==
              (std::max(std::abs(dx), std::abs(dy)) <= radius));
      }
    }
  }
  // Le coin du carré n'est pas dans le disque : trois cases en diagonale font
  // plus de quatre cases entières.
  CHECK_FALSE(
      moonclient::ZoneHolds(CompanionZoneShape::kDisc, kFollowCells, kFollowCells, kFollowCells));
  CHECK(
      moonclient::ZoneHolds(CompanionZoneShape::kSquare, kFollowCells, kFollowCells, kFollowCells));
}

TEST_CASE("Zones de l'IA : le rayon suit le réglage, et la marche du maître",
          "[app][companion_ai]") {
  ro::sim::CompanionAiSettings settings;
  settings.follow_distance_cells = kFollowCells;
  settings.leash_still_cells = kLeashStillCells;
  settings.leash_walking_cells = kLeashWalkingCells;
  settings.engage_still_cells = kEngageStillCells;
  settings.engage_walking_cells = kEngageWalkingCells;
  const moonclient::CompanionZoneRadii radii = moonclient::ZoneRadiiOf(settings);
  CHECK(radii.follow_cells == kFollowCells);
  CHECK(radii.leash_walking_cells == kLeashWalkingCells);
  CHECK(radii.engage_still_cells == kEngageStillCells);

  const CompanionZonesPlan still = moonclient::PlanCompanionZones({.radii = radii});
  REQUIRE(still.zones.size() == 3);
  CHECK(Find(still, CompanionZoneKind::kFollow)->radius_cells == kFollowCells);
  CHECK(Find(still, CompanionZoneKind::kLeash)->radius_cells == kLeashStillCells);
  CHECK(Find(still, CompanionZoneKind::kEngage)->radius_cells == kEngageStillCells);
  CHECK_FALSE(still.zones.front().emphasized);

  const CompanionZonesPlan walking =
      moonclient::PlanCompanionZones({.radii = radii, .owner_walking = true});
  CHECK(Find(walking, CompanionZoneKind::kLeash)->radius_cells == kLeashWalkingCells);
  CHECK(Find(walking, CompanionZoneKind::kEngage)->radius_cells == kEngageWalkingCells);

  // « AUCUN » NE SE DESSINE ; un suivi nul garde la case du maître.
  const CompanionZonesPlan none = moonclient::PlanCompanionZones({.radii = {}});
  REQUIRE(none.zones.size() == 1);
  CHECK(none.zones.front().kind == CompanionZoneKind::kFollow);
  const std::vector<ZoneCellPaint> owner_only = PaintsOf(none);
  REQUIRE(owner_only.size() == 1);
  CHECK(Painted(owner_only, 0, 0));
  CHECK(owner_only.front().color == moonclient::kFollowZoneColor);
}

TEST_CASE("Zones de l'IA : le bord se peint, de la forme de sa zone", "[app][companion_ai]") {
  const CompanionZonesPlan follow =
      moonclient::PlanCompanionZones({.radii = {.follow_cells = kFollowCells}});
  const std::vector<ZoneCellPaint> paints = PaintsOf(follow);
  // Le bord et non l'aplat : la case du maître reste nue.
  CHECK_FALSE(Painted(paints, 0, 0));
  CHECK(Painted(paints, kFollowCells, 0));
  CHECK(Painted(paints, 0, -kFollowCells));
  CHECK_FALSE(Painted(paints, kFollowCells, kFollowCells));
  CHECK_FALSE(Painted(paints, kFollowCells + 1, 0));
  for (const ZoneCellPaint& paint : paints) {
    CHECK(moonclient::ZoneHolds(CompanionZoneShape::kDisc, kFollowCells, paint.cell_x - kOwnerX,
                                paint.cell_y - kOwnerY));
    CHECK(paint.color == moonclient::kFollowZoneColor);
  }

  // L'ENGAGEMENT EST UN CARRÉ : ses coins se peignent, en rouge.
  const CompanionZonesPlan engage = moonclient::PlanCompanionZones(
      {.radii = {.follow_cells = 0, .engage_still_cells = kEngageWalkingCells},
       .hovered = CompanionAiZoneSlider::kEngageStill});
  const std::vector<ZoneCellPaint> square = PaintsOf(engage);
  CHECK(Painted(square, kEngageWalkingCells, kEngageWalkingCells));
  CHECK(Painted(square, -kEngageWalkingCells, kEngageWalkingCells));
  CHECK(std::ranges::all_of(square, [](const ZoneCellPaint& paint) {
    return paint.color[0] == moonclient::kEngageZoneColor[0] &&
           paint.color[3] == moonclient::kEmphasizedZoneAlpha;
  }));
}

TEST_CASE("Zones de l'IA : la case gardée se marque, pleine", "[app][companion_ai]") {
  const ro::sim::CellPosition held{kOwnerX + kLeashStillCells, kOwnerY, 0};
  const CompanionZonesPlan plan =
      moonclient::PlanCompanionZones({.radii = Radii(), .held_cell = held});
  REQUIRE(plan.held_cell.has_value());
  const std::vector<ZoneCellPaint> paints = PaintsOf(plan);
  CHECK(std::ranges::any_of(paints, [](const ZoneCellPaint& paint) {
    return paint.cell_x == kOwnerX + kLeashStillCells && paint.cell_y == kOwnerY &&
           paint.color == moonclient::kHeldCellColor;
  }));
  // Sans case gardée, rien de cette couleur.
  const std::vector<ZoneCellPaint> unheld =
      PaintsOf(moonclient::PlanCompanionZones({.radii = Radii()}));
  CHECK(std::ranges::none_of(unheld, [](const ZoneCellPaint& paint) {
    return paint.color == moonclient::kHeldCellColor;
  }));
}

TEST_CASE("Zones de l'IA : un curseur survolé ne garde que sa zone, plus épaisse",
          "[app][companion_ai]") {
  const ro::sim::CellPosition held{kOwnerX + 1, kOwnerY, 0};
  // LE CURSEUR « EN MARCHE » MONTRE SA ZONE, maître immobile ou non.
  const CompanionZonesPlan leash = moonclient::PlanCompanionZones(
      {.radii = Radii(), .hovered = CompanionAiZoneSlider::kLeashWalking, .held_cell = held});
  REQUIRE(leash.zones.size() == 1);
  CHECK(leash.zones.front().kind == CompanionZoneKind::kLeash);
  CHECK(leash.zones.front().radius_cells == kLeashWalkingCells);
  CHECK(leash.zones.front().emphasized);
  CHECK_FALSE(leash.held_cell.has_value());

  const CompanionZonesPlan engage = moonclient::PlanCompanionZones(
      {.radii = Radii(), .owner_walking = true, .hovered = CompanionAiZoneSlider::kEngageStill});
  REQUIRE(engage.zones.size() == 1);
  CHECK(engage.zones.front().radius_cells == kEngageStillCells);

  // Un curseur à « aucun » ne montre rien.
  CHECK(moonclient::PlanCompanionZones({.radii = {.follow_cells = kFollowCells},
                                        .hovered = CompanionAiZoneSlider::kLeashStill})
            .zones.empty());

  // PLUS ÉPAISSE : la case juste en deçà du bord se peint aussi.
  const std::vector<ZoneCellPaint> thin =
      PaintsOf(moonclient::PlanCompanionZones({.radii = {.follow_cells = kFollowCells}}));
  const std::vector<ZoneCellPaint> thick = PaintsOf(moonclient::PlanCompanionZones(
      {.radii = {.follow_cells = kFollowCells}, .hovered = CompanionAiZoneSlider::kFollow}));
  CHECK(thick.size() > thin.size());
  CHECK_FALSE(Painted(thin, kFollowCells - 1, 0));
  CHECK(Painted(thick, kFollowCells - 1, 0));
  CHECK(thick.front().color[3] == moonclient::kEmphasizedZoneAlpha);
}

TEST_CASE("Zones de l'IA : fenêtre ouverte ou case cochée, et le bon compagnon",
          "[app][companion_ai]") {
  CHECK_FALSE(moonclient::CompanionZonesShown(false, false));
  CHECK(moonclient::CompanionZonesShown(true, false));
  CHECK(moonclient::CompanionZonesShown(false, true));

  using ro::sim::AiCompanion;
  CHECK(moonclient::ZonesCompanion(AiCompanion::kMercenary, true, true) == AiCompanion::kMercenary);
  CHECK(moonclient::ZonesCompanion(AiCompanion::kMercenary, true, false) ==
        AiCompanion::kHomunculus);
  CHECK_FALSE(moonclient::ZonesCompanion(AiCompanion::kHomunculus, false, false).has_value());
}

// LA CAMÉRA DE L'ARÈNE (U9, conception §4.2) : ce qu'elle voit du sol tient
// dans le cadre d'apparition jusqu'au recul le plus lointain, sa descente finit
// à 1,2 s — bornée sur le sol généré, dont elle ne voit pas le bord —, son
// lacet tourne au clic droit hors de la descente, et un point de l'écran se
// ramène au sol à tout lacet.

#include "survivors/arena_camera.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/render/terrain_mesh.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"
#include "ro/sim/world.h"

#include "survivors/arena_ground_follow.h"
#include "survivors/arena_ground_plan.h"

using moonclient::ArenaCamera;
using moonclient::ArenaDescentDistance;
using moonclient::ArenaFarthestDistance;
using moonclient::GroundCellsUnder;
using moonclient::GroundFootprint;
using moonclient::GroundFootprintOf;
using moonclient::kArenaDescentFromFactor;
using moonclient::kArenaDescentMs;
using moonclient::kArenaEntryDistance;
using moonclient::kArenaFarthestReachDistance;
using moonclient::kArenaNearestDistance;

namespace {

constexpr float kWideAspect = 16.0F / 9.0F;
constexpr float kWideTallAspect = 16.0F / 10.0F;
constexpr float kUltraWideAspect = 21.0F / 9.0F;
constexpr float kSuperUltraWideAspect = 32.0F / 9.0F;
// Un héros qui n'est PAS à l'origine : une pose juste par hasard le serait.
constexpr glm::vec3 kHeroGround{1000.0F, 35.0F, 1200.0F};
constexpr std::uint32_t kEnteredMs = 5000;
constexpr float kManyNotches = 40.0F;
constexpr std::uint32_t kShakeLastsMs = 2000;
constexpr std::uint32_t kFrameMs = 7;
constexpr double kSymmetryShare = 0.001;
// Au-delà de la tolérance de la recherche du plus lointain.
constexpr float kBeyondFarthestDistance = 2.0F;
// Le recul le plus lointain en 16:9, que le cadre d'apparition livré permet au
// sol, et l'écart toléré.
constexpr float kWideFarthestDistance = 517.0F;
constexpr float kFarthestMarginDistance = 2.0F;
// Deux largeurs vues égales au calcul flottant près, en cases.
constexpr float kSameWidthCells = 0.001F;
// Le pas des lacets éprouvés, en degrés, et un tour.
constexpr float kYawStepDegrees = 15.0F;
constexpr float kFullTurnDegrees = 360.0F;
// Un glissé du clic droit, en points : un degré par point, comme en jeu.
constexpr float kDragPx = 30.0F;
// Un point du sol retrouvé sous l'écran, à cette erreur près, en cases.
constexpr double kGroundMatchCells = 0.001;
// La fenêtre de la conversion, en points.
constexpr float kViewportWidth = 1600.0F;
constexpr float kViewportHeight = 900.0F;

[[nodiscard]] float CellsOf(std::int32_t units) {
  return static_cast<float>(units) / static_cast<float>(ro::sim::kSubunitsPerCell);
}

[[nodiscard]] const ro::sim::SpawnFrame& Frame() {
  return ro::sim::ShippedSurvivorsRules().spawn_frame;
}

// La caméra au plus loin que la molette permet, posée hors de toute descente.
[[nodiscard]] ro::render::Camera FarthestCamera(float aspect_ratio) {
  ArenaCamera camera(Frame());
  const ro::sim::World nobody;
  static_cast<void>(camera.Pose(kHeroGround, kArenaDescentMs, aspect_ratio, 0, nobody));
  camera.Wheel(-kManyNotches);
  return camera.Pose(kHeroGround, kArenaDescentMs, aspect_ratio, 0, nobody);
}

void CheckInsideSpawnFrame(float aspect_ratio) {
  const ro::sim::SpawnFrame& frame = Frame();
  INFO("rapport " << aspect_ratio << ", recul " << FarthestCamera(aspect_ratio).distance);
  CHECK(FarthestCamera(aspect_ratio).distance == ArenaFarthestDistance(aspect_ratio, frame));
  const std::optional<GroundFootprint> seen =
      GroundFootprintOf(FarthestCamera(aspect_ratio), aspect_ratio);
  REQUIRE(seen.has_value());
  CHECK(seen->west_cells <= CellsOf(frame.half_width_units));
  CHECK(seen->east_cells <= CellsOf(frame.half_width_units));
  CHECK(seen->north_cells <= CellsOf(frame.north_units));
  CHECK(seen->south_cells <= CellsOf(frame.south_units));
  // Et la caméra voit bien quelque chose de chaque côté.
  CHECK(seen->west_cells > 0.0F);
  CHECK(seen->north_cells > 0.0F);
  CHECK(seen->south_cells > 0.0F);
}

}  // namespace

TEST_CASE("Caméra de l'arène : au plus loin, en 16:9, l'image tient dans le cadre d'apparition",
          "[app][survivors]") {
  CheckInsideSpawnFrame(kWideAspect);
}

TEST_CASE("Caméra de l'arène : le plus lointain est le plus grand recul dont le sol tient",
          "[app][survivors]") {
  // Aucun format ne recule moins que l'entrée, le 16:10 plus haut compris.
  for (const float aspect :
       {kWideAspect, kWideTallAspect, kUltraWideAspect, kSuperUltraWideAspect}) {
    const float farthest = ArenaFarthestDistance(aspect, Frame());
    INFO("rapport " << aspect << ", recul " << farthest);
    CHECK(farthest >= kArenaEntryDistance);
    CHECK(farthest <= kArenaFarthestReachDistance);
    if (farthest == kArenaEntryDistance || farthest == kArenaFarthestReachDistance) continue;
    // Un pas de plus déborde du cadre au sol.
    ro::render::Camera seen = FarthestCamera(aspect);
    seen.distance = farthest + kBeyondFarthestDistance;
    const std::optional<GroundFootprint> ground = GroundFootprintOf(seen, aspect);
    REQUIRE(ground.has_value());
    CHECK((ground->north_cells > CellsOf(Frame().north_units) ||
           ground->south_cells > CellsOf(Frame().south_units) ||
           ground->east_cells > CellsOf(Frame().half_width_units)));
  }
  // 🔴 LA TÊTE D'UN GRAND MONSTRE NÉ AU SUD NE BORNE PLUS LE RECUL : il paraît
  // en fondu. En 16:9, seul le sol borne, vers 517.
  CHECK(ArenaFarthestDistance(kWideAspect, Frame()) ==
        Catch::Approx(kWideFarthestDistance).margin(kFarthestMarginDistance));
}

TEST_CASE("Caméra de l'arène : l'entrée se fait au recul de 400", "[app][survivors]") {
  ArenaCamera camera(Frame());
  const ro::sim::World nobody;
  CHECK(camera.Pose(kHeroGround, kArenaDescentMs, kWideAspect, 0, nobody).distance ==
        kArenaEntryDistance);
}

TEST_CASE("Caméra de l'arène : au plus loin, en 21:9, l'image tient dans le cadre d'apparition",
          "[app][survivors]") {
  CheckInsideSpawnFrame(kUltraWideAspect);
}

TEST_CASE("Caméra de l'arène : en 32:9, la largeur vue reste celle du 21:9", "[app][survivors]") {
  // Jusqu'au 21:9, le champ est celui de RO ; au-delà, il se resserre.
  CHECK(moonclient::ArenaFovDegrees(kWideAspect) == moonclient::kArenaFovDegrees);
  CHECK(moonclient::ArenaFovDegrees(kUltraWideAspect) == moonclient::kArenaFovDegrees);
  CHECK(moonclient::ArenaFovDegrees(kSuperUltraWideAspect) < moonclient::kArenaFovDegrees);
  CheckInsideSpawnFrame(kSuperUltraWideAspect);
  // Au même recul, l'image plus large ne voit pas plus loin sur les côtés.
  ro::render::Camera at_widest = FarthestCamera(kUltraWideAspect);
  at_widest.distance = kArenaEntryDistance;
  ro::render::Camera at_wider = FarthestCamera(kSuperUltraWideAspect);
  at_wider.distance = kArenaEntryDistance;
  const std::optional<GroundFootprint> widest = GroundFootprintOf(at_widest, kUltraWideAspect);
  const std::optional<GroundFootprint> wider = GroundFootprintOf(at_wider, kSuperUltraWideAspect);
  REQUIRE(widest.has_value());
  REQUIRE(wider.has_value());
  CHECK(wider->east_cells <= widest->east_cells + kSameWidthCells);
}

TEST_CASE("Caméra de l'arène : l'image est symétrique et voit plus loin au nord qu'au sud",
          "[app][survivors]") {
  const std::optional<GroundFootprint> seen =
      GroundFootprintOf(FarthestCamera(kWideAspect), kWideAspect);
  REQUIRE(seen.has_value());
  CHECK(seen->west_cells == Catch::Approx(seen->east_cells).epsilon(kSymmetryShare));
  // 🔴 LE NORD EST EN HAUT : l'œil se tient au sud du héros et regarde vers le
  // nord, où le sol s'étend plus loin.
  CHECK(seen->north_cells > seen->south_cells);
}

TEST_CASE("Caméra de l'arène : la descente part de haut et finit à 1,2 s", "[app][survivors]") {
  CHECK(ArenaDescentDistance(kArenaEntryDistance, 0) ==
        Catch::Approx(kArenaEntryDistance * kArenaDescentFromFactor));
  // À mi-course, la caméra est encore haute ; elle ralentit en arrivant.
  CHECK(ArenaDescentDistance(kArenaEntryDistance, kArenaDescentMs / 2) > kArenaEntryDistance);
  CHECK(ArenaDescentDistance(kArenaEntryDistance, kArenaDescentMs / 2) <
        kArenaEntryDistance * kArenaDescentFromFactor);
  CHECK(ArenaDescentDistance(kArenaEntryDistance, kArenaDescentMs) == kArenaEntryDistance);

  // La descente suit l'arrivée de l'étage, pas l'horloge de l'image : elle
  // finit avec elle.
  CHECK(kArenaDescentMs == moonclient::kArrivalMs);
  ArenaCamera camera(Frame());
  const ro::sim::World nobody;
  camera.Enter();
  CHECK(camera.Pose(kHeroGround, 0, kWideAspect, kEnteredMs, nobody).distance >
        kArenaEntryDistance);
  CHECK(camera.Pose(kHeroGround, kArenaDescentMs / 2, kWideAspect, kEnteredMs, nobody).distance >
        kArenaEntryDistance);
  CHECK(camera.Pose(kHeroGround, kArenaDescentMs, kWideAspect, kEnteredMs, nobody).distance ==
        kArenaEntryDistance);
}

TEST_CASE("Caméra de l'arène : la molette reste entre 300 et le plus lointain du format",
          "[app][survivors]") {
  ArenaCamera camera(Frame());
  const ro::sim::World nobody;
  CHECK(camera.wheel_distance() == kArenaEntryDistance);
  static_cast<void>(camera.Pose(kHeroGround, kArenaDescentMs, kWideAspect, 0, nobody));
  const float farthest = ArenaFarthestDistance(kWideAspect, Frame());
  CHECK(camera.farthest_distance() == farthest);
  camera.Wheel(kManyNotches);
  CHECK(camera.wheel_distance() == kArenaNearestDistance);
  camera.Wheel(-kManyNotches);
  CHECK(camera.wheel_distance() == farthest);
  camera.Wheel(1.0F);
  CHECK(camera.wheel_distance() < farthest);
  CHECK(camera.wheel_distance() > kArenaNearestDistance);
  // UN FORMAT QUI RECULE MOINS ramène le recul tenu dans ses bornes.
  camera.Wheel(-kManyNotches);
  static_cast<void>(camera.Pose(kHeroGround, kArenaDescentMs, kSuperUltraWideAspect, 0, nobody));
  CHECK(camera.wheel_distance() == ArenaFarthestDistance(kSuperUltraWideAspect, Frame()));
  CHECK(camera.wheel_distance() < farthest);
}

TEST_CASE("Caméra de l'arène : ni la molette, ni la secousse, ni la descente ne tournent",
          "[app][survivors]") {
  ArenaCamera camera(Frame());
  const ro::sim::World nobody;
  camera.Enter();
  camera.Shake(kShakeLastsMs);
  for (std::uint32_t now_ms = 0; now_ms < kShakeLastsMs + kArenaDescentMs; now_ms += kFrameMs) {
    camera.Wheel(now_ms % 2 == 0 ? 1.0F : -1.0F);
    const std::uint32_t arrival_ms = std::min(now_ms, kArenaDescentMs);
    // Le clic droit glissé pendant la descente ne tourne rien.
    if (arrival_ms < kArenaDescentMs) camera.Orbit(kDragPx);
    const ro::render::Camera& posed =
        camera.Pose(kHeroGround, arrival_ms, kSuperUltraWideAspect, now_ms, nobody);
    REQUIRE(posed.yaw_degrees == 0.0F);
  }
  // La secousse finie, le héros est visé au centre, sans reste de tremblement.
  const ro::render::Camera& rested =
      camera.Pose(kHeroGround, kArenaDescentMs, kWideAspect, kShakeLastsMs * 2, nobody);
  CHECK(rested.look_at == kHeroGround);
  CHECK(rested.pitch_degrees == Catch::Approx(ro::render::kGameElevationDegrees));
}

TEST_CASE("Caméra de l'arène : sur le sol généré, la descente ne voit pas le bord de la fenêtre",
          "[app][survivors]") {
  // LE HÉROS AU DÉPART est à au moins la borne, plus la marge, de chaque bord
  // de la fenêtre du départ.
  const float reach = static_cast<float>(moonclient::kGroundDescentReachCells);
  const moonclient::GroundWindowOrigin window =
      moonclient::GroundWindowAround(ro::sim::kTerrainStartCell, ro::sim::kTerrainStartCell);
  const std::int32_t start = ro::sim::kTerrainStartCell;
  const std::int32_t nearest_edge_cells =
      std::min({start - window.x_cells, window.x_cells + moonclient::kGroundWindowCells - start,
                start - window.y_cells, window.y_cells + moonclient::kGroundWindowCells - start});
  CHECK(moonclient::kGroundDescentReachCells + moonclient::kGroundEdgeMarginCells <=
        nearest_edge_cells);

  const ro::sim::World nobody;
  constexpr std::uint32_t kDescentFrameMs = 16;
  // Le plus proche, l'entrée et le plus lointain de la molette, à chaque format.
  for (const float aspect_ratio :
       {kWideAspect, kWideTallAspect, kUltraWideAspect, kSuperUltraWideAspect}) {
    for (const float notches : {kManyNotches, 0.0F, -kManyNotches}) {
      INFO("rapport " << aspect_ratio << ", crans " << notches);
      ArenaCamera camera(Frame());
      camera.BoundDescent(reach);
      static_cast<void>(camera.Pose(kHeroGround, kArenaDescentMs, aspect_ratio, 0, nobody));
      camera.Wheel(notches);
      camera.Enter();
      for (std::uint32_t arrival_ms = 0; arrival_ms <= kArenaDescentMs;
           arrival_ms += kDescentFrameMs) {
        const ro::render::Camera& posed =
            camera.Pose(kHeroGround, arrival_ms, aspect_ratio, kEnteredMs, nobody);
        const std::optional<GroundFootprint> seen = GroundFootprintOf(posed, aspect_ratio);
        REQUIRE(seen.has_value());
        CHECK(std::max({seen->west_cells, seen->east_cells, seen->north_cells,
                        seen->south_cells}) <= reach);
      }
      // ELLE DESCEND TOUJOURS : partie de plus haut que le recul où elle se pose.
      CHECK(camera.Pose(kHeroGround, 0, aspect_ratio, kEnteredMs, nobody).distance >
            camera.wheel_distance());
    }
  }

  // SANS BORNE, au plus loin du 21:9, la descente voyait au-delà du bord.
  ArenaCamera unbounded(Frame());
  static_cast<void>(unbounded.Pose(kHeroGround, kArenaDescentMs, kUltraWideAspect, 0, nobody));
  unbounded.Wheel(-kManyNotches);
  const std::optional<GroundFootprint> wide = GroundFootprintOf(
      unbounded.Pose(kHeroGround, 0, kUltraWideAspect, kEnteredMs, nobody), kUltraWideAspect);
  REQUIRE(wide.has_value());
  CHECK(wide->west_cells > static_cast<float>(nearest_edge_cells));
}

TEST_CASE("Caméra de l'arène : le clic droit glissé tourne, le double clic revient au nord",
          "[app][survivors]") {
  ArenaCamera camera(Frame());
  const ro::sim::World nobody;
  static_cast<void>(camera.Pose(kHeroGround, kArenaDescentMs, kWideAspect, 0, nobody));
  // LA RÈGLE DU JEU : un degré par point, la droite tournant le lacet du jeu
  // dans le sens croissant, soit le nôtre dans l'autre.
  camera.Orbit(kDragPx);
  CHECK(camera.Pose(kHeroGround, kArenaDescentMs, kWideAspect, 0, nobody).yaw_degrees ==
        Catch::Approx(-kDragPx));
  CHECK(camera.yaw_degrees() == Catch::Approx(-kDragPx));
  // Le lacet reste dans un seul tour, quelle que soit la longueur du glissé.
  for (int turn = 0; turn < 20; ++turn) camera.Orbit(kFullTurnDegrees);
  CHECK(std::abs(camera.yaw_degrees()) <= kFullTurnDegrees / 2.0F);
  CHECK(camera.yaw_degrees() == Catch::Approx(-kDragPx).margin(kGroundMatchCells));
  camera.ResetYaw();
  CHECK(camera.Pose(kHeroGround, kArenaDescentMs, kWideAspect, 0, nobody).yaw_degrees == 0.0F);
  // Une partie neuve repart du nord.
  camera.Orbit(kDragPx);
  camera.Enter();
  CHECK(camera.yaw_degrees() == 0.0F);
}

TEST_CASE("Caméra de l'arène : tournée, elle recule moins pour tenir dans le cadre",
          "[app][survivors]") {
  const ro::sim::SpawnFrame& frame = Frame();
  bool shortened = false;
  for (const float aspect_ratio : {kWideAspect, kUltraWideAspect}) {
    const float north_farthest = ArenaFarthestDistance(aspect_ratio, frame);
    for (float yaw = 0.0F; yaw < kFullTurnDegrees; yaw += kYawStepDegrees) {
      INFO("rapport " << aspect_ratio << ", lacet " << yaw);
      const float farthest = ArenaFarthestDistance(aspect_ratio, frame, yaw);
      CHECK(farthest >= kArenaEntryDistance);
      CHECK(farthest <= kArenaFarthestReachDistance);
      shortened = shortened || farthest < north_farthest;
      // AU-DESSUS DU RECUL D'ENTRÉE, l'emprise tournée tient dans le cadre.
      if (farthest > kArenaEntryDistance) {
        ArenaCamera camera(frame);
        const ro::sim::World nobody;
        static_cast<void>(camera.Pose(kHeroGround, kArenaDescentMs, aspect_ratio, 0, nobody));
        camera.Wheel(-kManyNotches);
        camera.Orbit(-yaw);
        const ro::render::Camera& posed =
            camera.Pose(kHeroGround, kArenaDescentMs, aspect_ratio, 0, nobody);
        CHECK(posed.distance == Catch::Approx(farthest).margin(kFarthestMarginDistance));
        const std::optional<GroundFootprint> seen = GroundFootprintOf(posed, aspect_ratio);
        REQUIRE(seen.has_value());
        CHECK(seen->west_cells <= CellsOf(frame.half_width_units));
        CHECK(seen->east_cells <= CellsOf(frame.half_width_units));
        CHECK(seen->north_cells <= CellsOf(frame.north_units));
        CHECK(seen->south_cells <= CellsOf(frame.south_units));
        // Revenue au nord, la caméra rend le recul de la molette.
        camera.ResetYaw();
        CHECK(camera.Pose(kHeroGround, kArenaDescentMs, aspect_ratio, 0, nobody).distance ==
              north_farthest);
      }
    }
  }
  // Le cadre, plus large que profond, raccourcit bien le recul de biais.
  CHECK(shortened);
}

TEST_CASE("Caméra de l'arène : un point de l'écran retombe sur son point du sol à tout lacet",
          "[app][survivors]") {
  // UN SOL EN PENTE, pour que le plan ne soit pas celui de l'horizon : la case
  // à l'est est plus haute, celle du nord plus basse. 🔴 LE NORD EST VERS LES Z
  // DÉCROISSANTS.
  const float cell = ro::render::kWorldUnitsPerCell;
  const glm::vec3 east = kHeroGround + glm::vec3(cell, 2.0F, 0.0F);
  const glm::vec3 north = kHeroGround + glm::vec3(0.0F, -1.0F, -cell);
  const glm::vec2 wanted_cells(3.5F, -2.25F);
  const glm::vec3 wanted =
      kHeroGround + wanted_cells.x * (east - kHeroGround) + wanted_cells.y * (north - kHeroGround);
  for (float yaw = -180.0F; yaw < 180.0F; yaw += kYawStepDegrees) {
    INFO("lacet " << yaw);
    ro::render::Camera camera;
    camera.look_at = kHeroGround;
    camera.distance = kArenaEntryDistance;
    camera.pitch_degrees = moonclient::kArenaElevationDegrees;
    camera.fov_degrees = moonclient::kArenaFovDegrees;
    camera.yaw_degrees = yaw;
    const glm::vec4 clip = camera.ProjectionMatrix(kViewportWidth / kViewportHeight) *
                           camera.ViewMatrix() * glm::vec4(wanted, 1.0F);
    REQUIRE(clip.w > 0.0F);
    const glm::vec2 ndc = glm::vec2(clip) / clip.w;
    const glm::vec2 screen((ndc.x + 1.0F) * 0.5F * kViewportWidth,
                           (1.0F - ndc.y) * 0.5F * kViewportHeight);
    const std::optional<glm::vec2> found =
        GroundCellsUnder(camera, screen, kViewportWidth, kViewportHeight, kHeroGround, east, north);
    REQUIRE(found.has_value());
    CHECK(found->x == Catch::Approx(wanted_cells.x).margin(kGroundMatchCells));
    CHECK(found->y == Catch::Approx(wanted_cells.y).margin(kGroundMatchCells));
  }
  // LE HAUT DE L'ÉCRAN regarde le nord tourné du lacet : à 90°, l'ouest.
  ro::render::Camera turned;
  turned.look_at = kHeroGround;
  turned.distance = kArenaEntryDistance;
  turned.pitch_degrees = moonclient::kArenaElevationDegrees;
  turned.fov_degrees = moonclient::kArenaFovDegrees;
  turned.yaw_degrees = 90.0F;
  const std::optional<glm::vec2> up = GroundCellsUnder(
      turned, glm::vec2(kViewportWidth / 2.0F, 0.0F), kViewportWidth, kViewportHeight, kHeroGround,
      kHeroGround + glm::vec3(cell, 0.0F, 0.0F), kHeroGround + glm::vec3(0.0F, 0.0F, -cell));
  REQUIRE(up.has_value());
  CHECK(up->x < 0.0F);
  CHECK(up->y == Catch::Approx(0.0).margin(kGroundMatchCells));
  // UN RAYON QUI LONGE LE SOL ne le rencontre pas.
  ro::render::Camera grazing = turned;
  grazing.pitch_degrees = 0.0F;
  CHECK_FALSE(GroundCellsUnder(grazing, glm::vec2(kViewportWidth / 2.0F, kViewportHeight / 2.0F),
                               kViewportWidth, kViewportHeight, kHeroGround,
                               kHeroGround + glm::vec3(cell, 0.0F, 0.0F),
                               kHeroGround + glm::vec3(0.0F, 0.0F, -cell))
                  .has_value());
}

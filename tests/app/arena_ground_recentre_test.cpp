// UN RECENTRAGE NE SE VOIT PAS (étage 4) : la scène de l'arène sur le sol
// généré d'une prairie synthétique, peinte hors écran juste avant et juste
// après un recentrage forcé, la caméra déplacée du décalage que la scène rend
// et rien de plus. Les deux images sont égales à la tolérance des snapshots.
// Sans le décalage, elles diffèrent : l'essai mord.
//
// Sans contexte OpenGL (un poste sans carte graphique), l'essai se saute.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/image.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/gl_context.h"
#include "ro/render/image_diff.h"
#include "ro/render/offscreen_target.h"
#include "ro/render/sprite_catalog.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "arena_ground_synthetic.h"
#include "options/graphics_options.h"
#include "support/temp_directory.h"
#include "survivors/arena_camera.h"
#include "survivors/arena_ground_follow.h"
#include "survivors/arena_ground_window.h"
#include "survivors/arena_scene.h"
#include "survivors/horde_composer.h"
#include "survivors/presentation_clock.h"

namespace {

constexpr int kWidth = 256;
constexpr int kHeight = 144;
constexpr std::uint64_t kSeed = 20261084;
// La tolérance des snapshots du sol : la part de pixels qui peuvent différer.
constexpr double kMostDifferingShare = 0.01;
constexpr std::size_t kRgbaBytes = 4;
constexpr std::uint8_t kOpaque = 255;
// Le même point du monde, compté depuis deux coins, au centième de case près :
// le sol arrondi se calcule en flottants depuis le coin de sa fenêtre.
constexpr float kSamePointUnits = 0.05F;
// LE HÉROS, près de la frontière est de la première fenêtre : ce que la caméra
// voit tient dans les deux fenêtres.
constexpr float kHeroEastCells = 30.0F;
constexpr float kHeroNorthCells = 5.0F;

struct Painter {
  moonclient::ArenaScene& scene;
  moonclient::HordeComposer& composer;
  ro::render::OffscreenTarget& target;
  const moonclient::GraphicsOptions& graphics;

  ro::formats::Image Paint(const ro::render::Camera& camera) const {
    constexpr float kAspect = static_cast<float>(kWidth) / static_cast<float>(kHeight);
    moonclient::LitFrame frame = scene.Begin(graphics, 0);
    target.Bind();
    scene.Paint(frame, camera, kAspect, graphics, 0, 0, composer);
    auto painted = target.ReadPixels();
    REQUIRE(painted.has_value());
    for (std::size_t alpha = kRgbaBytes - 1; alpha < painted->pixels.size(); alpha += kRgbaBytes) {
      painted->pixels[alpha] = kOpaque;
    }
    return std::move(*painted);
  }
};

double DifferingShare(const ro::formats::Image& left, const ro::formats::Image& right) {
  const auto compared = ro::render::CompareImages(left, right);
  REQUIRE(compared.has_value());
  REQUIRE(compared->compared > 0);
  return static_cast<double>(compared->differing) / static_cast<double>(compared->compared);
}

}  // namespace

TEST_CASE("Recentrage : deux images de part et d'autre d'un recentrage forcé sont égales",
          "[app][survivors][snapshot]") {
  auto context = ro::render::GlContext::CreateHeadless(kWidth, kHeight);
  if (!context) SKIP("pas de contexte OpenGL : " + ro::core::Describe(context.error()));
  auto target = ro::render::OffscreenTarget::Create(kWidth, kHeight);
  REQUIRE(target.has_value());

  const ro::test_support::TempDirectory client;
  moonclient::test_support::FillSyntheticPrairie(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"essai.grf"}});
  REQUIRE(vfs.has_value());
  auto sources =
      moonclient::OpenGroundSources(*vfs, moonclient::test_support::ShippedPlains(), kSeed,
                                    moonclient::test_support::SyntheticPrairieLook());
  REQUIRE(sources.has_value());
  const moonclient::GroundWindowOrigin start =
      moonclient::GroundWindowAround(ro::sim::kTerrainStartCell, ro::sim::kTerrainStartCell);
  moonclient::ArenaScene scene;
  REQUIRE(scene
              .OpenGround(*vfs, std::move(*sources), start, "essai", {},
                          moonclient::GroundBuildThread::kCaller)
              .has_value());

  ro::render::ActorWardrobe wardrobe(*vfs, ro::render::SpriteCatalog{});
  moonclient::HordeComposer composer(wardrobe);
  const moonclient::GraphicsOptions graphics;
  const Painter painter{
      .scene = scene, .composer = composer, .target = *target, .graphics = graphics};

  constexpr float kHalfWindow = static_cast<float>(moonclient::kGroundWindowCells) / 2.0F;
  const glm::vec2 hero(static_cast<float>(start.x_cells) + kHalfWindow + kHeroEastCells,
                       static_cast<float>(start.y_cells) + kHalfWindow + kHeroNorthCells);
  moonclient::ArenaCamera arena_camera(ro::sim::ShippedSurvivorsRules().spawn_frame);
  const ro::sim::World nobody;
  constexpr float kAspect = static_cast<float>(kWidth) / static_cast<float>(kHeight);
  static_cast<void>(
      arena_camera.Pose(scene.GroundAtCell(hero), moonclient::kArrivalMs, kAspect, 0, nobody));
  const glm::vec3 ground_before = scene.GroundAtCell(hero);
  const ro::formats::Image before = painter.Paint(arena_camera.camera());

  const moonclient::GroundWindowOrigin east{
      .x_cells = start.x_cells + moonclient::kGroundRecentreCells, .y_cells = start.y_cells};
  auto moved = scene.Recentre(east);
  REQUIRE(moved.has_value());
  CHECK(moved->from == start);
  CHECK(moved->to == east);
  CHECK(scene.window_origin() == east);
  // LA MÊME CASE DU MONDE, comptée depuis le nouveau coin, tombe là où le
  // décalage dit.
  const glm::vec3 ground_after = scene.GroundAtCell(hero);
  CHECK(std::abs(ground_after.x - (ground_before.x + moved->world_delta.x)) < kSamePointUnits);
  CHECK(std::abs(ground_after.y - (ground_before.y + moved->world_delta.y)) < kSamePointUnits);
  CHECK(std::abs(ground_after.z - (ground_before.z + moved->world_delta.z)) < kSamePointUnits);

  // SANS LE DÉCALAGE, la caméra regarde un autre coin du sol : l'essai mord.
  const ro::formats::Image unshifted = painter.Paint(arena_camera.camera());
  CHECK(DifferingShare(before, unshifted) > kMostDifferingShare);

  arena_camera.Shift(moved->world_delta);
  const ro::formats::Image after = painter.Paint(arena_camera.camera());
  CHECK(DifferingShare(before, after) <= kMostDifferingShare);
}

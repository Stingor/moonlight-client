// LE SOL GÉNÉRÉ, PEINT HORS ÉCRAN (étage 4) : la fenêtre d'une graine fixe,
// bâtie sur une installation SYNTHÉTIQUE — textures unies, un modèle en
// pyramide, la lumière d'une carte d'essai — et peinte par la séquence de
// `LitStage`, se compare à son image de référence : une sous l'ombre cuite,
// une sous l'ombre du soleil en temps réel. Le premier passage écrit chaque
// image, qui se relit avant d'être gardée ; les suivants s'y comparent.
//
// Sans contexte OpenGL (un poste sans carte graphique), l'essai se saute.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/image.h"
#include "ro/render/gl_context.h"
#include "ro/render/image_diff.h"
#include "ro/render/offscreen_target.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "arena_ground_synthetic.h"
#include "options/graphics_options.h"
#include "support/image_builder.h"
#include "support/temp_directory.h"
#include "survivors/arena_camera.h"
#include "survivors/arena_ground_window.h"
#include "world/lit_stage.h"

using ro::test_support::BuildTga;
using ro::test_support::TempDirectory;
using ro::test_support::TgaPixel;

// Le dossier des références, que `tests/CMakeLists.txt` pose ; à défaut, celui
// du dépôt vu depuis sa racine.
#ifndef MOONCLIENT_SNAPSHOT_DIR
#define MOONCLIENT_SNAPSHOT_DIR "tests/app/snapshots"
#endif

namespace {

constexpr int kWidth = 256;
constexpr int kHeight = 144;
constexpr std::uint64_t kSeed = 20261084;
// La part de pixels qui peuvent différer d'une carte graphique à l'autre.
constexpr double kMostDifferingShare = 0.01;
constexpr std::uint8_t kOpaque = 255;
constexpr std::size_t kRgbaBytes = 4;
constexpr std::string_view kBakedReferenceFile = "arena_ground_window.tga";
constexpr std::string_view kSunReferenceFile = "arena_ground_window_sun.tga";

// Les effets que la scène glisse entre ses passes : aucun ici.
class NoLayers : public moonclient::LitLayers {
 public:
  void DrawBehindBodies(const ro::render::Camera& /*camera*/, float /*aspect_ratio*/,
                        const moonclient::LitFrame& /*frame*/) override {}
  void DrawOverWater(const ro::render::Camera& /*camera*/, float /*aspect_ratio*/,
                     const moonclient::LitFrame& /*frame*/) override {}
};

std::filesystem::path ReferencePath(std::string_view reference_file) {
  return std::filesystem::path(MOONCLIENT_SNAPSHOT_DIR) / reference_file;
}

void WriteReference(const ro::formats::Image& image, const std::filesystem::path& path) {
  std::vector<TgaPixel> pixels;
  pixels.reserve(static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height));
  for (std::size_t offset = 0; offset + kRgbaBytes <= image.pixels.size(); offset += kRgbaBytes) {
    pixels.push_back(TgaPixel{.red = image.pixels[offset],
                              .green = image.pixels[offset + 1],
                              .blue = image.pixels[offset + 2]});
  }
  constexpr std::uint8_t kRgbBits = 24;
  const std::vector<std::uint8_t> bytes =
      BuildTga(image.width, image.height, pixels, {.bits_per_pixel = kRgbBits, .rle = true});
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char*>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  REQUIRE(file.good());
}

// La fenêtre de la graine fixe, peinte par `LitStage` sous ces réglages, puis
// comparée à sa référence — écrite si elle manque.
void PaintAndCompare(const moonclient::GraphicsOptions& graphics, std::string_view reference_file) {
  auto context = ro::render::GlContext::CreateHeadless(kWidth, kHeight);
  if (!context) SKIP("pas de contexte OpenGL : " + ro::core::Describe(context.error()));
  auto target = ro::render::OffscreenTarget::Create(kWidth, kHeight);
  REQUIRE(target.has_value());

  const TempDirectory client;
  moonclient::test_support::FillSyntheticPrairie(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"essai.grf"}});
  REQUIRE(vfs.has_value());
  auto sources =
      moonclient::OpenGroundSources(*vfs, moonclient::test_support::ShippedPlains(), kSeed,
                                    moonclient::test_support::SyntheticPrairieLook());
  REQUIRE(sources.has_value());
  CHECK(sources->library.missing_terrain_textures == 0);
  CHECK(sources->library.unreadable_models == 0);
  auto window = moonclient::BuildGroundWindow(
      *vfs, *sources,
      moonclient::GroundWindowAround(ro::sim::kTerrainStartCell, ro::sim::kTerrainStartCell),
      "essai");
  REQUIRE(window.has_value());
  CHECK(window->stage.scene.model_instance_count > 0);

  moonclient::LitStage lit;
  REQUIRE(lit.Open(*vfs, std::move(window->stage), moonclient::test_support::kSyntheticReferenceMap)
              .has_value());
  moonclient::LitFrame frame = lit.Begin(graphics, /*fog_shown=*/false, 0);
  // La caméra de l'arène, au recul d'entrée, sur le centre de la fenêtre.
  constexpr float kCentreCell = static_cast<float>(moonclient::kGroundWindowCells) / 2.0F;
  const std::array<float, 3> centre =
      moonclient::GroundPositionAt(lit.stage(), kCentreCell, kCentreCell);
  moonclient::ArenaCamera arena_camera(ro::sim::ShippedSurvivorsRules().spawn_frame);
  const ro::sim::World nobody;
  constexpr float kAspect = static_cast<float>(kWidth) / static_cast<float>(kHeight);
  const ro::render::Camera& camera = arena_camera.Pose(glm::vec3(centre[0], centre[1], centre[2]),
                                                       moonclient::kArrivalMs, kAspect, 0, nobody);
  target->Bind();
  NoLayers layers;
  lit.Paint(frame, camera, kAspect, graphics, std::nullopt, 0, 0, moonclient::LitActors{}, layers);
  auto painted = target->ReadPixels();
  REQUIRE(painted.has_value());
  for (std::size_t alpha = kRgbaBytes - 1; alpha < painted->pixels.size(); alpha += kRgbaBytes) {
    painted->pixels[alpha] = kOpaque;
  }

  const std::filesystem::path reference_path = ReferencePath(reference_file);
  if (!std::filesystem::exists(reference_path)) {
    WriteReference(*painted, reference_path);
    WARN("référence écrite, à relire avant de la garder : " + reference_path.string());
    return;
  }
  std::ifstream file(reference_path, std::ios::binary);
  const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
  auto reference = ro::formats::DecodeImage(reference_file, bytes);
  REQUIRE(reference.has_value());
  const auto compared = ro::render::CompareImages(*painted, *reference);
  REQUIRE(compared.has_value());
  CHECK(compared->compared > 0);
  CHECK(static_cast<double>(compared->differing) <=
        kMostDifferingShare * static_cast<double>(compared->compared));
}

}  // namespace

TEST_CASE("Sol dessiné : la fenêtre d'une graine fixe, peinte hors écran, rend sa référence",
          "[app][survivors][snapshot]") {
  PaintAndCompare(moonclient::GraphicsOptions(), kBakedReferenceFile);
}

// ⚠ LA CARTE D'OMBRE NAÎT DANS CETTE IMAGE MÊME, la cible hors écran déjà liée :
// c'est la première image d'une scène qui demande l'ombre du soleil.
TEST_CASE("Sol dessiné sous l'ombre du soleil : la première image rend sa référence",
          "[app][survivors][snapshot]") {
  moonclient::GraphicsOptions graphics;
  REQUIRE(graphics.Change(ro::ui::OptionsWindowAction{
      .changed = moonclient::GraphicsRowId(moonclient::GraphicsRow::kSunShadows), .on = true}));
  REQUIRE(graphics.sun_shadows());
  PaintAndCompare(graphics, kSunReferenceFile);
}

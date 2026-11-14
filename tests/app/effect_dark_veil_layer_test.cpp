// LE VOILE DE TÉNÈBRES DANS SA PASSE À LUI : par-dessus tout le monde, sous les
// fenêtres. Voir `OverWorldLayer`.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_dark_veil.h"
#include "ro/render/effect_dark_veil_seed.h"
#include "ro/render/effect_frame.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "game/over_world_paint.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/temp_directory.h"

using ro::test_support::BmpPixel;
using ro::test_support::BuildBmp;
using ro::test_support::BuildGrf;
using ro::test_support::BuildTga;
using ro::test_support::GrfFileSpec;
using ro::test_support::TempDirectory;
using ro::test_support::TgaPixel;

namespace {

constexpr std::uint32_t kBlindDarkness = 334;
constexpr std::uint32_t kBearer = 7000;
constexpr std::int32_t kImageSide = 8;
constexpr float kViewportWidth = 1280.0F;
constexpr float kViewportHeight = 720.0F;
// Assez de tics pour que le voile, né invisible, soit venu.
constexpr std::int32_t kTicksToRise = 30;
constexpr std::uint32_t kFirstFrameMs = 100;
// L'opacité du bord de l'image du cœur : moins que le plein, comme celle du jeu.
constexpr std::uint8_t kEdgeAlpha = 245;

// Un client qui porte les deux images du voile : un cœur transparent au coin
// bas-gauche et à 245 ailleurs, un cadre blanc.
void FillVeilClient(const TempDirectory& client) {
  const ro::render::EffectDarkVeilPattern& pattern =
      ro::render::EffectDarkVeilPatternsFor(kBlindDarkness).front();
  std::vector<TgaPixel> core(static_cast<std::size_t>(kImageSide * kImageSide),
                             TgaPixel{.red = 255, .green = 255, .blue = 255, .alpha = kEdgeAlpha});
  core[static_cast<std::size_t>((kImageSide - 1) * kImageSide)].alpha = 0;
  const std::vector<BmpPixel> frame(static_cast<std::size_t>(kImageSide * kImageSide),
                                    BmpPixel{.red = 255, .green = 255, .blue = 255});
  client.WriteFile(
      "effets.grf",
      BuildGrf({GrfFileSpec{std::string(pattern.textures[ro::render::kDarkVeilCoreTexture]),
                            BuildTga(kImageSide, kImageSide, core)},
                GrfFileSpec{std::string(pattern.textures[ro::render::kDarkVeilFrameTexture]),
                            BuildBmp(kImageSide, kImageSide, frame)}}));
}

[[nodiscard]] ro::render::Camera GameCamera() {
  ro::render::Camera camera;
  camera.distance = ro::render::kGameViewDistance;
  camera.pitch_degrees = ro::render::kGameElevationDegrees;
  camera.fov_degrees = ro::render::kGameFovDegrees;
  return camera;
}

[[nodiscard]] bool IsVeilLot(const ro::render::DrawnScreenQuads& lot) {
  const ro::render::EffectDarkVeilPattern& pattern =
      ro::render::EffectDarkVeilPatternsFor(kBlindDarkness).front();
  const std::string_view path = *lot.texture_path;
  return path.starts_with(pattern.textures[ro::render::kDarkVeilCoreTexture]) ||
         path == pattern.textures[ro::render::kDarkVeilFrameTexture];
}

}  // namespace

TEST_CASE("le voile se peint dans sa passe à lui, pas avec les effets", "[dark_veil_layer]") {
  const TempDirectory client;
  FillVeilClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"effets.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  REQUIRE(nodes.PlayProcedural(kBlindDarkness, glm::vec3{0.0F, 0.0F, 0.0F}, std::nullopt, 0.0F,
                               kBearer));
  const ro::render::Camera camera = GameCamera();
  static_cast<void>(nodes.Drawn(kFirstFrameMs, camera));
  static_cast<void>(nodes.Drawn(
      kFirstFrameMs + static_cast<std::uint32_t>(kTicksToRise * ro::render::kEffectTickMs),
      camera));

  // 🔴 LA PASSE DES EFFETS NE LE PORTE PLUS : elle se peint avant les jauges,
  // les noms, les bulles et les chiffres, qui passaient donc par-dessus lui.
  for (const ro::render::DrawnScreenQuads& lot :
       nodes.ScreenQuads(camera, kViewportWidth, kViewportHeight)) {
    CHECK_FALSE(IsVeilLot(lot));
  }

  // SA PASSE À LUI le rend seul : le cœur, étiré pour rejoindre le noir, puis
  // le cadre.
  const std::span<const ro::render::DrawnScreenQuads> veil =
      nodes.ScreenQuads(camera, kViewportWidth, kViewportHeight, true);
  REQUIRE(veil.size() == 2);
  for (const ro::render::DrawnScreenQuads& lot : veil) {
    CHECK(IsVeilLot(lot));
    CHECK_FALSE(lot.quads.empty());
  }
  CHECK(veil[0].texture_path->ends_with(ro::render::kDarkVeilJoinedCoreSuffix));
  // L'image étirée : son bord, à 245 dans le fichier, est opaque.
  const ro::formats::Image& joined = *veil[0].texture;
  REQUIRE(!joined.pixels.empty());
  CHECK(joined.pixels[static_cast<std::size_t>(kImageSide - 1) * 4 + 3] == 255);
}

TEST_CASE("au-dessus du monde, le voile d'abord et le HUD de jeu par-dessus", "[dark_veil_layer]") {
  // Le classement PvP et l'étiquette de ce qui est armé restent lisibles sous
  // la nuit : ils se peignent APRÈS le voile, dans la même couche.
  REQUIRE(moonclient::kOverWorldPasses.size() == 2);
  CHECK(moonclient::kOverWorldPasses[0] == moonclient::OverWorldPass::kDarkVeil);
  CHECK(moonclient::kOverWorldPasses[1] == moonclient::OverWorldPass::kGameHud);
}

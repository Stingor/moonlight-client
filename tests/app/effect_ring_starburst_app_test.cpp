// LA GERBE NOIRE D'EF_BLACKDEVIL VUE PAR L'APPLICATION : aucun de ses deux
// nœuds ne naît au tic zéro, c'est l'horloge de l'effet qui les sème, au tic un
// puis au tic deux, et ils vivent ensuite du mouvement de la gerbe.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_ring_starburst_seed.h"
#include "ro/vfs/vfs.h"

#include "effects/effect_node_view.h"
#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/temp_directory.h"

using Catch::Approx;
using ro::test_support::BuildGrf;
using ro::test_support::BuildTga;
using ro::test_support::GrfFileSpec;
using ro::test_support::TempDirectory;
using ro::test_support::TgaPixel;

namespace {

constexpr std::uint32_t kBlackDevil = 487;
constexpr std::int32_t kPixels = 4;
constexpr std::uint32_t kFirstFrameMs = 100;

// Un client qui ne porte que l'anneau noir de la gerbe.
void FillClient(const TempDirectory& client) {
  const std::vector<TgaPixel> texture(static_cast<std::size_t>(kPixels * kPixels),
                                      TgaPixel{.red = 20, .green = 20, .blue = 20});
  client.WriteFile("gerbe.grf", BuildGrf({GrfFileSpec{"data\\texture\\effect\\ring_black.tga",
                                                      BuildTga(kPixels, kPixels, texture)}}));
}

std::uint32_t PassTicks(moonclient::EffectNodesInPlay& nodes, std::uint32_t since_ms,
                        std::int32_t ticks) {
  const ro::render::Camera camera;
  const std::uint32_t until_ms =
      since_ms + static_cast<std::uint32_t>(ticks * ro::render::kEffectTickMs);
  static_cast<void>(nodes.Drawn(until_ms, camera));
  return until_ms;
}

std::size_t CountStarbursts(const moonclient::EffectNodesInPlay& nodes) {
  std::size_t count = 0;
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    count += node.shape == ro::render::PooledNodeShape::kRingStarburst ? 1U : 0U;
  }
  return count;
}

}  // namespace

TEST_CASE("la gerbe noire naît au tic un puis au tic deux, et son cercle grandit ensuite",
          "[effect_ring_starburst_app]") {
  const TempDirectory client;
  FillClient(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"gerbe.grf"}});
  REQUIRE(vfs.has_value());
  moonclient::EffectNodesInPlay nodes(*vfs);
  // L'effet est connu, bien que rien ne naisse au tic zéro : l'horloge est posée.
  REQUIRE(moonclient::EffectNodesInPlay::KnowsProcedural(kBlackDevil));
  REQUIRE(nodes.PlayProcedural(kBlackDevil, glm::vec3(0.0F, 0.0F, 0.0F)));
  std::uint32_t now_ms = PassTicks(nodes, kFirstFrameMs, 0);
  CHECK(CountStarbursts(nodes) == 0);
  // Le premier passage voit l'âge zéro, où rien ne naît ; le suivant voit l'âge un.
  now_ms = PassTicks(nodes, now_ms, 1);
  CHECK(CountStarbursts(nodes) == 0);
  now_ms = PassTicks(nodes, now_ms, 1);
  CHECK(CountStarbursts(nodes) == 1);
  now_ms = PassTicks(nodes, now_ms, 1);
  CHECK(CountStarbursts(nodes) == 2);
  // Vingt tics plus tard, la première piste du premier nœud a grandi d'un
  // dixième par tic depuis deux et demi, sans dépasser ce que vingt-deux tics
  // permettent.
  static_cast<void>(PassTicks(nodes, now_ms, 20));
  bool seen = false;
  for (const ro::render::PooledEffectNode& node : nodes.living_nodes()) {
    if (node.shape != ro::render::PooledNodeShape::kRingStarburst) {
      continue;
    }
    seen = true;
    CHECK(node.trail_variant == 12);
    CHECK(node.tracks[0].radius_at_anchor > 2.5F);
    CHECK(node.tracks[0].radius_at_anchor <= 2.7F + 0.1F * 22.0F + 1e-4F);
    CHECK(node.tracks[0].degrees == 360);
  }
  CHECK(seen);
}

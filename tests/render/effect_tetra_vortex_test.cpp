// LE TOURBILLON DE TETRA VORTEX : le sous-type 140, les effets 804 et 970 à 973.
//
// Son bâtisseur, son mouvement de particules en quatre pistes, et sa figure,
// l'arc de ruban (`effect_arc_ribbon.h`).
#include "ro/render/effect_tetra_vortex.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_arc_ribbon.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_tetra_vortex_seed.h"

#include "support/effect_bearers.h"

using Catch::Approx;
using ro::render::AdvanceTetraVortexTracks;
using ro::render::ComposeArcRibbonQuads;
using ro::render::EffectNodeLifeFor;
using ro::render::EffectNodePool;
using ro::render::EffectNodeScheduleLastSowTick;
using ro::render::EffectNodeScheduleSowsAt;
using ro::render::EffectTetraVortexPatternsFor;
using ro::render::EffectTetraVortexPatternTable;
using ro::render::EffectTetraVortexTintFor;
using ro::render::EffectTetraVortexTintIsWritten;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTickMs;
using ro::render::kEffectTracksPerNode;
using ro::render::kTetraVortexSchedule;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::SeedTetraVortexTracks;
using ro::render::SowTetraVortexNode;

namespace {

constexpr std::uint32_t kFireVortexEffectId = 804;
constexpr std::uint32_t kRedVortexEffectId = 970;
constexpr std::int32_t kFrameCount = 8;
constexpr std::int32_t kTicksToLastFrame = 21;
constexpr std::int32_t kTicksToFirstFade = 24;
constexpr std::int32_t kTicksToDark = 63;
constexpr float kTolerance = 1.0e-4F;

// Les pistes du semis, tournées d'autant de tics que demandé.
std::array<EffectTrack, kEffectTracksPerNode> Advanced(std::uint32_t effect_id,
                                                       std::int32_t ticks) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedTetraVortexTracks(effect_id, glm::vec3{0.0F, 0.0F, 0.0F});
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceTetraVortexTracks(tracks, 0, 0, kFrameCount);
  }
  return tracks;
}

}  // namespace

TEST_CASE("Tourbillon : cinq sites, un bâtisseur, et la famille est connue",
          "[render][effectnode]") {
  const auto table = EffectTetraVortexPatternTable();
  REQUIRE(table.size() == 5);
  const std::array<std::uint32_t, 5> attendus = {804, 970, 971, 972, 973};
  for (std::size_t rang = 0; rang < attendus.size(); ++rang) {
    CHECK(table[rang].effect_id == attendus[rang]);
    CHECK(table[rang].client_function == "Effect_InitTetraVortex");
    CHECK(ro::render::KnowsProceduralEffect(attendus[rang]));
  }
  CHECK(EffectTetraVortexPatternsFor(805).empty());
}

TEST_CASE("Tourbillon : la parure vient de l'identifiant, et le 804 n'en reçoit aucune",
          "[render][effectnode]") {
  CHECK(EffectTetraVortexTintFor(970) == 20);
  CHECK(EffectTetraVortexTintFor(971) == 18);
  CHECK(EffectTetraVortexTintFor(972) == 17);
  CHECK(EffectTetraVortexTintFor(973) == 19);
  CHECK(EffectTetraVortexTintIsWritten(973));
  // Le `switch` n'a pas de cas pour lui : l'octet n'est pas écrit.
  CHECK_FALSE(EffectTetraVortexTintIsWritten(kFireVortexEffectId));
  CHECK(EffectTetraVortexTintFor(kFireVortexEffectId) == 0);
}

TEST_CASE("Tourbillon : quatre semis, aux tics 0, 25, 50 et 75", "[render][effectnode]") {
  std::vector<std::int32_t> sowing;
  for (std::int32_t tick = 0; tick <= 200; ++tick) {
    if (EffectNodeScheduleSowsAt(kTetraVortexSchedule, tick)) {
      sowing.push_back(tick);
    }
  }
  CHECK(sowing == std::vector<std::int32_t>{0, 25, 50, 75});
  CHECK(EffectNodeScheduleLastSowTick(kTetraVortexSchedule) == 80);
}

TEST_CASE("Tourbillon : une seule piste vive, au point visé, trente-cinq plus haut",
          "[render][effectnode]") {
  const glm::vec3 aimed{12.0F, 3.0F, -7.0F};
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedTetraVortexTracks(kRedVortexEffectId, aimed);
  const EffectTrack& living = tracks[0];
  CHECK(living.active);
  CHECK(living.alpha == 200);
  CHECK(living.ticks == 0);
  CHECK(living.degrees == 120);
  CHECK(living.radius_at_anchor == Approx(30.0F));
  CHECK(living.value[10] == Approx(1.0F));
  CHECK(living.value[8] == Approx(0.0F));
  CHECK(living.mark[0] == 0);
  CHECK(living.mark[2] == 20);
  CHECK(living.anchor.x == Approx(12.0F));
  CHECK(living.anchor.y == Approx(38.0F));
  CHECK(living.anchor.z == Approx(-7.0F));
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    CHECK_FALSE(tracks[rang].active);
  }
}

TEST_CASE("Tourbillon : le nœud porte huit images, le mot cinq et la vie de l'effet",
          "[render][effectnode]") {
  const glm::vec3 bearer{1.0F, 0.0F, 2.0F};
  const glm::vec3 aimed{40.0F, 0.0F, 40.0F};
  const std::optional<PooledEffectNode> node =
      SowTetraVortexNode(kRedVortexEffectId, bearer, aimed, EffectNodeLifeFor(kRedVortexEffectId));
  REQUIRE(node.has_value());
  CHECK(node->shape == PooledNodeShape::kTetraVortex);
  CHECK(node->ground_position == bearer);
  CHECK(node->node.render_word == 5U);
  CHECK(node->node.life_ticks == 600);
  REQUIRE(node->resource_paths.size() == 8);
  CHECK(node->resource_paths.front() == "data\\texture\\effect\\tv-1.bmp");
  CHECK(node->resource_paths.back() == "data\\texture\\effect\\tv-8.bmp");
  // Sans point visé, le client dessinerait au vecteur de création : rien ici.
  CHECK_FALSE(SowTetraVortexNode(kRedVortexEffectId, bearer, std::nullopt, 600).has_value());
}

TEST_CASE("Tourbillon : une image tous les trois tics, puis quinze d'alpha par cran",
          "[render][effectnode]") {
  CHECK(Advanced(kRedVortexEffectId, 2)[0].mark[0] == 0);
  CHECK(Advanced(kRedVortexEffectId, 3)[0].mark[0] == 1);
  const EffectTrack last = Advanced(kRedVortexEffectId, kTicksToLastFrame)[0];
  CHECK(last.mark[0] == 7);
  CHECK(last.alpha == 200);
  const EffectTrack first_notch = Advanced(kRedVortexEffectId, kTicksToFirstFade)[0];
  CHECK(first_notch.mark[0] == 7);
  CHECK(first_notch.alpha == 185);
  CHECK(first_notch.ticks == kTicksToFirstFade);
  // Quatorze crans font moins dix : la borne arrête à zéro.
  CHECK(Advanced(kRedVortexEffectId, kTicksToDark - 1)[0].alpha == 5);
  CHECK(Advanced(kRedVortexEffectId, kTicksToDark)[0].alpha == 0);
  CHECK(Advanced(kRedVortexEffectId, kTicksToDark + 30)[0].alpha == 0);
  // Une piste éteinte ne compte pas.
  CHECK(Advanced(kRedVortexEffectId, 9)[1].ticks == 0);
}

TEST_CASE("Tourbillon : les angles de panneau se rangent tels quels sur les deux premières",
          "[render][effectnode]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedTetraVortexTracks(kRedVortexEffectId, glm::vec3{0.0F});
  AdvanceTetraVortexTracks(tracks, 333, 47, kFrameCount);
  CHECK(tracks[0].style == 333);
  CHECK(tracks[1].style == 47);
}

TEST_CASE("Tourbillon : la figure de l'arc peint l'image courante, dans sa parure",
          "[render][effectnode]") {
  const EffectTrack red = Advanced(kRedVortexEffectId, 6)[0];
  const std::vector<EffectWorldQuad> layers = ComposeArcRibbonQuads(red, 0, 0);
  // La parure 20 empile deux couches : rouge, puis rouge pâle.
  REQUIRE(layers.size() == 2);
  for (const EffectWorldQuad& layer : layers) {
    CHECK(layer.texture == 2);
  }
  CHECK(layers[0].tint[1] == 5);
  CHECK(layers[1].tint[1] == 155);

  // Le 804 garde la parure zéro : une seule couche blanche.
  const EffectTrack feu = Advanced(kFireVortexEffectId, 0)[0];
  CHECK(ComposeArcRibbonQuads(feu, 0, 0).size() == 1);

  // Éteint par l'alpha, il ne peint plus rien.
  CHECK(ComposeArcRibbonQuads(Advanced(kRedVortexEffectId, kTicksToDark)[0], 0, 0).empty());
}

TEST_CASE("Tourbillon : la troupe le fait défiler, et le point visé ne suit pas le porteur",
          "[render][effectnode]") {
  EffectNodePool pool;
  const glm::vec3 bearer{0.0F, 0.0F, 0.0F};
  const glm::vec3 aimed{50.0F, 0.0F, 0.0F};
  std::optional<PooledEffectNode> node =
      SowTetraVortexNode(kRedVortexEffectId, bearer, aimed, EffectNodeLifeFor(kRedVortexEffectId));
  REQUIRE(node.has_value());
  pool.AnchorNextTo(17U, bearer);
  pool.Add(std::move(*node));
  pool.AnchorNextTo(0U);

  ro::test_support::MoveBearer(pool, 17U, glm::vec3{-20.0F, 0.0F, 5.0F});
  pool.FaceCamera(90, 30);
  for (std::int32_t tick = 0; tick < 3; ++tick) {
    pool.Advance(kEffectTickMs);
  }
  REQUIRE(pool.size() == 1);
  const PooledEffectNode& alive = pool.nodes().front();
  CHECK(alive.tracks[0].mark[0] == 1);
  CHECK(alive.tracks[0].style == 90);
  CHECK(alive.tracks[1].style == 30);
  CHECK(alive.tracks[0].anchor.x == Approx(aimed.x).margin(kTolerance));
  CHECK(alive.tracks[0].anchor.z == Approx(aimed.z).margin(kTolerance));
}

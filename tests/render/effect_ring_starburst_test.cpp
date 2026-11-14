// LA GERBE D'ANNEAUX (sous-type 107) : quatre jupes dont le cercle grandit
// pendant que le ruban s'ouvre, se referme et se couche.

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_ring_starburst_seed.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_figures.h"

using Catch::Approx;
using ro::render::AdvanceRingStarburstTracks;
using ro::render::ComposeEffectTrackSkirt;
using ro::render::EffectRingStarburstLastSowTick;
using ro::render::EffectRingStarburstPattern;
using ro::render::EffectRingStarburstPatternsFor;
using ro::render::EffectRingStarburstPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kRingStarburstDiesAfterTick;
using ro::render::kRingStarburstFullSweep;
using ro::render::kRingStarburstInnerRadius;
using ro::render::SeedEffectRingStarburstTracks;
using ro::render::SowEffectRingStarburstNode;

namespace {

constexpr std::uint32_t kBlackDevilEffect = 487;
constexpr std::uint8_t kDarkGreyVariant = 12;

[[nodiscard]] EffectRingStarburstPattern TestPattern() {
  EffectRingStarburstPattern pattern;
  pattern.effect_id = 1;
  pattern.client_function = "test";
  pattern.texture = "test.tga";
  return pattern;
}

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Advance(
    std::array<EffectTrack, kEffectTracksPerNode> tracks, int ticks) {
  for (int tick = 0; tick < ticks; ++tick) {
    AdvanceRingStarburstTracks(tracks);
  }
  return tracks;
}

}  // namespace

TEST_CASE("la gerbe : quatre pistes à un quart de tour et un quart de cycle l'une de l'autre",
          "[render][ring_starburst]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectRingStarburstTracks(TestPattern());
  const std::array<std::int16_t, 4> azimuths = {270, 0, 90, 180};
  const std::array<float, 4> cycles = {2.5F, 5.0F, 7.5F, 10.0F};
  const std::array<std::int16_t, 4> tilts = {53, 60, 55, 50};
  for (std::size_t rank = 0; rank < tracks.size(); ++rank) {
    REQUIRE(tracks[rank].active);
    REQUIRE(tracks[rank].ticks == 0);
    REQUIRE(tracks[rank].alpha == 0);
    REQUIRE(tracks[rank].degrees == kRingStarburstFullSweep);
    REQUIRE(tracks[rank].azimuth_degrees == azimuths[rank]);
    REQUIRE(tracks[rank].radius_at_anchor == Approx(cycles[rank]));
    REQUIRE(tracks[rank].lift_key == Approx(cycles[rank]));
    REQUIRE(tracks[rank].tilt_degrees == tilts[rank]);
    REQUIRE(tracks[rank].value[0] == Approx(0.0F));
    REQUIRE(tracks[rank].value[20] == Approx(0.0F));
  }
}

TEST_CASE("la gerbe : le cercle grandit d'un dixième par tic et le ruban s'ouvre à partir de trois",
          "[render][ring_starburst]") {
  // La première piste part à deux et demi : quatre tics plus tard elle est à
  // deux virgule neuf, sans largeur ; au sixième tic elle dépasse trois.
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      Advance(SeedEffectRingStarburstTracks(TestPattern()), 4);
  CHECK(tracks[0].radius_at_anchor == Approx(2.9F));
  CHECK(tracks[0].value[0] == Approx(0.0F));
  // Pendant l'ouverture, l'alpha monte de deux par tic.
  CHECK(tracks[0].alpha == 8);
  tracks = Advance(tracks, 2);
  CHECK(tracks[0].radius_at_anchor == Approx(3.1F));
  // Le ruban est deux fois large comme la part du cycle parcourue.
  CHECK(tracks[0].value[0] == Approx(0.2F).margin(1e-5F));
  CHECK(tracks[0].value[20] == Approx(0.2F).margin(1e-5F));
  // Et la jupe se couche : quatre-vingt-dix degrés moins cinq par unité, tronqués.
  CHECK(tracks[0].tilt_degrees == 89);
  CHECK(tracks[0].alpha == 12);
}

TEST_CASE("la gerbe : passé la moitié du cycle, le ruban se referme et l'alpha retombe",
          "[render][ring_starburst]") {
  // La quatrième piste part à dix, sept unités dans son cycle : elle se referme.
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      Advance(SeedEffectRingStarburstTracks(TestPattern()), 1);
  CHECK(tracks[3].radius_at_anchor == Approx(10.1F));
  // Dix moins sept virgule un, doublé.
  CHECK(tracks[3].value[0] == Approx(5.8F).margin(1e-4F));
  CHECK(tracks[3].tilt_degrees == 54);
  // Son alpha était nul : il ne descend pas sous zéro.
  CHECK(tracks[3].alpha == 0);
}

TEST_CASE(
    "la gerbe : à treize, le cercle repart de trois, et la piste s'éteint passé cinquante tics",
    "[render][ring_starburst]") {
  // La quatrième piste atteint treize au trentième tic : elle repart, encore vive.
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      Advance(SeedEffectRingStarburstTracks(TestPattern()), 30);
  CHECK(tracks[3].active);
  CHECK(tracks[3].radius_at_anchor == Approx(kRingStarburstInnerRadius));
  CHECK(tracks[3].alpha == 2);
  // La troisième, partie à sept et demi, y arrive au cinquante-cinquième tic :
  // trop tard, elle s'éteint.
  tracks = Advance(tracks, 25);
  CHECK(tracks[2].ticks == 55);
  CHECK(tracks[2].ticks > kRingStarburstDiesAfterTick);
  CHECK_FALSE(tracks[2].active);
  // Une piste éteinte ne bouge plus.
  const float frozen = tracks[2].radius_at_anchor;
  tracks = Advance(tracks, 5);
  CHECK(tracks[2].radius_at_anchor == Approx(frozen));
}

TEST_CASE("la gerbe ouverte rend des quads de la jupe", "[render][ring_starburst]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      Advance(SeedEffectRingStarburstTracks(TestPattern()), 20);
  const std::vector<EffectWorldQuad> skirt =
      ComposeEffectTrackSkirt(tracks[0], kDarkGreyVariant, glm::vec3{0.0F, 0.0F, 0.0F});
  CHECK_FALSE(skirt.empty());
}

TEST_CASE("le nœud semé porte la forme de la gerbe et sa variante sombre",
          "[render][ring_starburst]") {
  const ro::render::PooledEffectNode node =
      SowEffectRingStarburstNode(TestPattern(), glm::vec3{1.0F, 2.0F, 3.0F}, 200);
  CHECK(node.shape == ro::render::PooledNodeShape::kRingStarburst);
  CHECK(node.trail_variant == kDarkGreyVariant);
  CHECK(node.node.life_ticks == 200);
  CHECK(node.ground_position == glm::vec3(1.0F, 2.0F, 3.0F));
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == "test.tga");
}

TEST_CASE("la table des gerbes : deux nœuds pour la gerbe noire, aux tics un et deux",
          "[render][ring_starburst]") {
  const std::span<const EffectRingStarburstPattern> patterns =
      EffectRingStarburstPatternsFor(kBlackDevilEffect);
  REQUIRE(patterns.size() == 2);
  CHECK(patterns[0].sown_at_tick == 1);
  CHECK(patterns[1].sown_at_tick == 2);
  CHECK(patterns[0].first_cycle == Approx(2.5F));
  CHECK(patterns[1].first_cycle == Approx(2.7F));
  CHECK(patterns[0].first_azimuth_degrees == 270);
  CHECK(patterns[1].first_azimuth_degrees == 271);
  CHECK(patterns[1].seed_tilt_degrees == std::array<std::int16_t, 4>{52, 59, 54, 49});
  CHECK(EffectRingStarburstLastSowTick(kBlackDevilEffect) == 2);
  CHECK(EffectRingStarburstLastSowTick(1) == 0);
  CHECK(EffectRingStarburstPatternTable().size() == 2);
}

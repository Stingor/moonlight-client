// LES COURONNES DE MAGNUM BREAK, sous-type 73 : la table, l'horaire, le semis,
// le mouvement et la figure.

#include "ro/render/effect_break_ring.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_break_ring_seed.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"

using Catch::Approx;
using ro::render::AdvanceBreakRingTracks;
using ro::render::ComposeBreakRingQuads;
using ro::render::EffectBreakRingPattern;
using ro::render::EffectBreakRingPatternsFor;
using ro::render::EffectBreakRingPatternTable;
using ro::render::EffectNodeScheduleLastSowTick;
using ro::render::EffectNodeScheduleSowsAt;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kBreakRingSchedule;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::SeedBreakRingTracks;
using ro::render::SowBreakRingNode;

namespace {

using Tracks = std::array<EffectTrack, kEffectTracksPerNode>;

constexpr std::uint32_t kMagnumBreakEffectId = 339;

void Advance(Tracks& tracks, std::int32_t ticks) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceBreakRingTracks(tracks);
  }
}

}  // namespace

TEST_CASE("Couronnes : un site, l'effet 339 sur ring_yellow", "[render][effectnode][breakring]") {
  REQUIRE(EffectBreakRingPatternTable().size() == 1);
  const auto patterns = EffectBreakRingPatternsFor(kMagnumBreakEffectId);
  REQUIRE(patterns.size() == 1);
  CHECK(patterns.front().client_function == "Effect_BuildUpdate_MagnumBreakRings");
  CHECK(patterns.front().texture == "data\\texture\\effect\\ring_yellow.tga");
  CHECK(ro::render::KnowsProceduralEffect(kMagnumBreakEffectId));
}

TEST_CASE("Couronnes : dix nœuds, tous les trois tics du vingt-septième au cinquante-quatrième",
          "[render][effectnode][breakring]") {
  std::int32_t sowing = 0;
  for (std::int32_t tick = 0; tick <= 100; ++tick) {
    if (EffectNodeScheduleSowsAt(kBreakRingSchedule, tick)) {
      ++sowing;
      CHECK(tick % 3 == 0);
      CHECK(tick >= 27);
      CHECK(tick <= 54);
    }
  }
  CHECK(sowing == 10);
  CHECK(EffectNodeScheduleLastSowTick(kBreakRingSchedule) == 54);
}

TEST_CASE("Couronnes : la taille dépend du rang dans la salve", "[render][effectnode][breakring]") {
  // AU TIC TRENTE, rang cinq.
  const Tracks tracks = SeedBreakRingTracks(30);
  CHECK(tracks[0].active);
  CHECK(tracks[1].active);
  CHECK_FALSE(tracks[2].active);
  CHECK_FALSE(tracks[3].active);
  CHECK(tracks[0].degrees == 360);
  CHECK(tracks[0].lift_key == Approx(20.0F - (5.0F * 0.7F)));
  CHECK(tracks[0].radius_at_anchor == Approx((5.0F * 0.2F) + 5.0F));
  CHECK(tracks[0].azimuth_degrees == 30);
  CHECK(tracks[1].azimuth_degrees == 39);
  CHECK(tracks[0].tilt_degrees == 60);
  CHECK(tracks[1].tilt_degrees == 52);
  CHECK(tracks[0].style == 0);
  CHECK(tracks[1].style == 1);
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("Couronnes : le nœud porte le mot cinq, la variante cinq et la vie de l'effet",
          "[render][effectnode][breakring]") {
  const EffectBreakRingPattern& pattern = EffectBreakRingPatternsFor(kMagnumBreakEffectId)[0];
  const PooledEffectNode node = SowBreakRingNode(pattern, glm::vec3{1.0F, 2.0F, 3.0F}, 27, 100);
  CHECK(node.shape == PooledNodeShape::kBreakRings);
  REQUIRE(node.node.render_word.has_value());
  CHECK(*node.node.render_word == 5U);
  CHECK(node.trail_variant == 5);
  CHECK(node.node.life_ticks == 100);
  CHECK(node.tracks[0].azimuth_degrees == 12);
}

TEST_CASE("Couronnes : dix puis cinq par tic jusqu'au huitième, puis moins trente et la mort",
          "[render][effectnode][breakring]") {
  Tracks tracks = SeedBreakRingTracks(27);
  Advance(tracks, 8);
  CHECK(tracks[0].alpha == 80);
  CHECK(tracks[1].alpha == 40);
  Advance(tracks, 1);
  CHECK(tracks[0].alpha == 50);
  CHECK(tracks[1].alpha == 10);
  Advance(tracks, 1);
  CHECK(tracks[0].alpha == 20);
  CHECK_FALSE(tracks[1].active);
  Advance(tracks, 1);
  CHECK_FALSE(tracks[0].active);
}

TEST_CASE("Couronnes : l'inclinaison négative revient à zéro, l'azimut se replie",
          "[render][effectnode][breakring]") {
  Tracks tracks = SeedBreakRingTracks(27);
  tracks[0].tilt_degrees = -4;
  tracks[0].azimuth_degrees = 365;
  Advance(tracks, 1);
  CHECK(tracks[0].tilt_degrees == 0);
  CHECK(tracks[0].azimuth_degrees == 5);
}

TEST_CASE("Couronnes : vingt segments, le bord intérieur relevé de trois",
          "[render][effectnode][breakring]") {
  Tracks tracks = SeedBreakRingTracks(27);
  tracks[0].alpha = 60;
  tracks[0].tilt_degrees = 0;
  const glm::vec3 anchor{10.0F, 20.0F, 30.0F};
  std::vector<EffectWorldQuad> quads;
  ComposeBreakRingQuads(tracks[0], 5, anchor, quads);
  REQUIRE(quads.size() == 20);
  const float radius = tracks[0].radius_at_anchor;
  const float key = tracks[0].lift_key;
  for (const EffectWorldQuad& quad : quads) {
    CHECK(quad.corners[0].y == Approx(anchor.y + 3.0F));
    // À PLAT : la clé éloigne le bord extérieur, sans le lever.
    CHECK(glm::length(glm::vec2{quad.corners[3].x - anchor.x, quad.corners[3].z - anchor.z}) ==
          Approx(radius + key));
    CHECK(quad.corners[3].y == Approx(anchor.y + 3.0F));
    CHECK(quad.tint[3] == 60);
  }
  // ⚠ LE DERNIER SEGMENT REFERME LE CERCLE sur l'azimut de départ.
  CHECK(quads.back().corners[1].x == Approx(quads.front().corners[0].x));
  CHECK(quads.back().corners[1].z == Approx(quads.front().corners[0].z));

  // DRESSÉE À QUATRE-VINGT-DIX DEGRÉS, la clé lève le bord extérieur au lieu de
  // l'éloigner.
  tracks[0].tilt_degrees = 90;
  quads.clear();
  ComposeBreakRingQuads(tracks[0], 5, anchor, quads);
  REQUIRE_FALSE(quads.empty());
  CHECK(quads[0].corners[3].y == Approx(anchor.y + key + 3.0F));

  // ⚠ ÉTEINTE OU INVISIBLE, RIEN.
  tracks[0].alpha = 0;
  quads.clear();
  ComposeBreakRingQuads(tracks[0], 5, anchor, quads);
  CHECK(quads.empty());
}

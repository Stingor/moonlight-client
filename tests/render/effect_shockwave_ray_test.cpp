// LES RAIS DE L'ONDE DE CHOC, sous-type 54 : la table, le semis, le mouvement et
// la figure.

#include "ro/render/effect_shockwave_ray.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_shockwave_ray_seed.h"

using Catch::Approx;
using ro::render::AdvanceShockwaveRayTracks;
using ro::render::ComposeShockwaveRayQuads;
using ro::render::EffectParticleBlend;
using ro::render::EffectShockwaveRayPattern;
using ro::render::EffectShockwaveRayPatternsFor;
using ro::render::EffectShockwaveRayPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectOwnerTintAbsent;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::SeedShockwaveRayTracks;
using ro::render::SowShockwaveRayNode;

namespace {

using Tracks = std::array<EffectTrack, kEffectTracksPerNode>;

constexpr std::uint32_t kShockwaveEffectId = 300;

void Advance(Tracks& tracks, std::int32_t ticks) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceShockwaveRayTracks(tracks, 0, 0);
  }
}

}  // namespace

TEST_CASE("Onde de choc : deux appels pour l'effet 300, aux angles trente et soixante",
          "[render][effectnode][shockwave]") {
  REQUIRE(EffectShockwaveRayPatternTable().size() == 2);
  const auto patterns = EffectShockwaveRayPatternsFor(kShockwaveEffectId);
  REQUIRE(patterns.size() == 2);
  CHECK(patterns[0].client_function == "Effect_Build_Shockwave");
  CHECK(patterns[0].texture == "data\\texture\\effect\\shockwave_c.bmp");
  CHECK(patterns[0].base_degrees == 30);
  CHECK(patterns[1].base_degrees == 60);
  CHECK(ro::render::KnowsProceduralEffect(kShockwaveEffectId));
  CHECK(EffectShockwaveRayPatternsFor(301).empty());
}

TEST_CASE("Onde de choc : quatre pistes allumées, un quart de tour d'écart, rayon cent",
          "[render][effectnode][shockwave]") {
  const Tracks tracks = SeedShockwaveRayTracks(30);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].ticks == 0);
    CHECK(tracks[rang].alpha == 0);
    CHECK(tracks[rang].radius_at_anchor == 100.0F);
    CHECK(tracks[rang].tilt_degrees == static_cast<std::int16_t>(30 + (90 * rang)));
  }
}

TEST_CASE("Onde de choc : le nœud porte le mot cinq, l'image et la vie de l'effet",
          "[render][effectnode][shockwave]") {
  const EffectShockwaveRayPattern& pattern = EffectShockwaveRayPatternsFor(kShockwaveEffectId)[1];
  const PooledEffectNode node = SowShockwaveRayNode(pattern, glm::vec3{4.0F, 5.0F, 6.0F}, 300);
  CHECK(node.shape == PooledNodeShape::kShockwaveRays);
  REQUIRE(node.node.render_word.has_value());
  CHECK(*node.node.render_word == 5U);
  CHECK(node.node.life_ticks == 300);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == "data\\texture\\effect\\shockwave_c.bmp");
  CHECK(node.ground_position == glm::vec3{4.0F, 5.0F, 6.0F});
  CHECK(node.tracks[0].tilt_degrees == 60);
}

TEST_CASE("Onde de choc : dix-huit par tic jusqu'au dixième, palier, puis moins quinze",
          "[render][effectnode][shockwave]") {
  Tracks tracks = SeedShockwaveRayTracks(30);
  Advance(tracks, 10);
  CHECK(tracks[0].alpha == 180);
  Advance(tracks, 10);
  CHECK(tracks[0].alpha == 180);
  Advance(tracks, 1);
  CHECK(tracks[0].alpha == 165);
  // ⚠ À ZÉRO, LA PISTE RESTE ALLUMÉE : elle ne meurt qu'avec le nœud.
  Advance(tracks, 20);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].active);
}

TEST_CASE("Onde de choc : les angles de panneau se rangent tels quels, même éteints",
          "[render][effectnode][shockwave]") {
  Tracks tracks;
  AdvanceShockwaveRayTracks(tracks, 123, 45);
  CHECK(tracks[0].style == 123);
  CHECK(tracks[1].style == 45);
  CHECK(tracks[0].ticks == 0);
}

TEST_CASE("Onde de choc : rien tant que l'alpha n'est pas positif",
          "[render][effectnode][shockwave]") {
  const Tracks tracks = SeedShockwaveRayTracks(30);
  std::vector<EffectWorldQuad> quads;
  ComposeShockwaveRayQuads(tracks[0], 0, 0, glm::vec3{0.0F}, quads);
  CHECK(quads.empty());
}

TEST_CASE("Onde de choc : un triangle étroit et blanc, puis un large et bleu pâle",
          "[render][effectnode][shockwave]") {
  Tracks tracks = SeedShockwaveRayTracks(0);
  tracks[0].alpha = 101;
  const glm::vec3 origin{10.0F, 20.0F, 30.0F};
  std::vector<EffectWorldQuad> quads;
  ComposeShockwaveRayQuads(tracks[0], 0, 0, origin, quads);
  REQUIRE(quads.size() == 2);

  // LA MOITIÉ DE L'ALPHA, TRONQUÉE ; l'alpha entier pour le second.
  CHECK(quads[0].tint == std::array<std::uint8_t, 4>{255, 255, 255, 50});
  CHECK(quads[1].tint == std::array<std::uint8_t, 4>{130, 130, 255, 101});
  const auto addition = EffectParticleBlend(0, kEffectOwnerTintAbsent);
  CHECK(quads[0].blend_destination == addition.destination);
  CHECK(quads[1].blend_destination == addition.destination);

  // LE CENTRE, deux fois ; les deux autres coins au rayon.
  for (const EffectWorldQuad& quad : quads) {
    CHECK(quad.corners[0] == quad.corners[3]);
    CHECK(glm::length(quad.corners[1] - quad.corners[0]) == Approx(100.0F));
    CHECK(glm::length(quad.corners[2] - quad.corners[0]) == Approx(100.0F));
  }
  // LE LARGE S'OUVRE PLUS QUE L'ÉTROIT : cinq degrés de part et d'autre contre
  // trois.
  const float narrow = glm::length(quads[0].corners[2] - quads[0].corners[1]);
  const float large = glm::length(quads[1].corners[2] - quads[1].corners[1]);
  CHECK(narrow == Approx(2.0F * 100.0F * std::sin(glm::radians(3.0F))).margin(1e-3));
  CHECK(large == Approx(2.0F * 100.0F * std::sin(glm::radians(5.0F))).margin(1e-3));
}

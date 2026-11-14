#include "ro/render/effect_spiral_stack.h"

#include <algorithm>
#include <cmath>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_spiral_stack_seed.h"

using Catch::Approx;
using ro::render::ComposeSpiralStackQuads;
using ro::render::EffectNodePool;
using ro::render::EffectSpiralStackPattern;
using ro::render::EffectSpiralStackPatternsFor;
using ro::render::EffectSpiralStackPatternTable;
using ro::render::EffectSpiralStackStylesFor;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kMarkOfSpiralSteps;
using ro::render::kMarkOfSpiralStyle;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectSpiralStackTracks;
using ro::render::SowEffectSpiralStackNode;

namespace {

constexpr glm::vec3 kAtTheOrigin{0.0F, 0.0F, 0.0F};

[[nodiscard]] float QuadArea(const EffectWorldQuad& quad) {
  const glm::vec3 ab = quad.corners[1] - quad.corners[0];
  const glm::vec3 ac = quad.corners[2] - quad.corners[0];
  const glm::vec3 ad = quad.corners[3] - quad.corners[0];
  return (glm::length(glm::cross(ab, ac)) + glm::length(glm::cross(ac, ad))) * 0.5F;
}

// La piste vive d'un style, avancée par le mouvement — elle naît invisible.
[[nodiscard]] EffectTrack AdvancedTrack(std::uint8_t style, std::int32_t ticks) {
  const EffectSpiralStackPattern pattern{0, "essai", "image.tga", style};
  std::uint32_t seed = 999U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectSpiralStackTracks(pattern, seed);
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    ro::render::AdvanceSpiralStackTracks(tracks, seed);
  }
  return tracks.front();
}

}  // namespace

TEST_CASE("la pile naît invisible et une seule piste est vive", "[render][spiral_stack]") {
  const EffectSpiralStackPattern& pattern = EffectSpiralStackPatternsFor(435).front();
  std::uint32_t seed = 1U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpiralStackTracks(pattern, seed);
  CHECK(tracks.front().active);
  CHECK(tracks.front().alpha == 0);
  for (std::size_t rang = 1; rang < kEffectTracksPerNode; ++rang) {
    CHECK_FALSE(tracks[rang].active);
  }
  // ⚠ ALPHA NUL, DONC AUCUN QUAD : c'est le mouvement qui allume.
  CHECK(ComposeSpiralStackQuads(tracks.front(), kAtTheOrigin).empty());
}

TEST_CASE("la pile compte un quad par étage", "[render][spiral_stack]") {
  const EffectTrack track = AdvancedTrack(0, 30);
  const std::vector<EffectWorldQuad> quads = ComposeSpiralStackQuads(track, kAtTheOrigin);
  REQUIRE(quads.size() == track.mark[kMarkOfSpiralSteps]);
  for (const EffectWorldQuad& quad : quads) {
    CHECK(QuadArea(quad) > 0.0F);
  }
}

TEST_CASE("la pile MONTE, TOURNE et RÉTRÉCIT", "[render][spiral_stack]") {
  const EffectTrack track = AdvancedTrack(0, 30);
  const std::vector<EffectWorldQuad> quads = ComposeSpiralStackQuads(track, kAtTheOrigin);
  REQUIRE(quads.size() >= 3);

  // ⚠ LE SOMMET EST EN HAUT, AU-DESSUS DU SOL : le premier étage est le plus
  // haut, et les suivants redescendent vers l'ancre. Le client l'écrit dans son
  // repère qui descend ; recopier ses ordonnées posait la pile sous le sol.
  CHECK(quads.front().corners[3].y > 0.0F);
  CHECK(quads[1].corners[3].y < quads[0].corners[3].y);
  for (const EffectWorldQuad& quad : quads) {
    for (const glm::vec3& corner : quad.corners) {
      CHECK(corner.y > 0.0F);
    }
  }

  // Chaque étage est plus étroit que le précédent, tant que le pas reste positif.
  const auto radius = [](const EffectWorldQuad& quad) {
    return std::hypot(quad.corners[3].x, quad.corners[3].z);
  };
  CHECK(radius(quads[1]) < radius(quads[0]));
  CHECK(radius(quads[2]) < radius(quads[1]));

  // Et il a tourné : le premier coin n'est pas au même azimut.
  CHECK(quads[1].corners[3].x != Approx(quads[0].corners[3].x));

  // ⚠ LE CARRÉ EST GAUCHI : deux coins un cran plus haut que les deux autres.
  CHECK(quads.front().corners[0].y == Approx(quads.front().corners[3].y + 1.0F));
  CHECK(quads.front().corners[1].y == Approx(quads.front().corners[2].y + 1.0F));
}

TEST_CASE("le style décide de la teinte, du mélange et du nombre de couches",
          "[render][spiral_stack]") {
  SECTION("deux styles ADDITIONNENT, et rien dans leur numéro ne le dit") {
    CHECK(EffectSpiralStackStylesFor(0).front().blend_mode == 0);
    CHECK(EffectSpiralStackStylesFor(10).front().blend_mode == 0);
    for (std::uint8_t style = 1; style <= 8; ++style) {
      INFO("style " << static_cast<int>(style));
      CHECK(EffectSpiralStackStylesFor(style).front().blend_mode == 1);
    }
  }
  SECTION("un style peint DEUX couches, et lui seul") {
    CHECK(EffectSpiralStackStylesFor(11).size() == 2);
    for (std::uint8_t style = 0; style <= 10; ++style) {
      INFO("style " << static_cast<int>(style));
      CHECK(EffectSpiralStackStylesFor(style).size() == 1);
    }
    // Sa première couche est un voile au tiers de l'alpha, la seconde un rouge
    // additif par-dessus.
    CHECK(EffectSpiralStackStylesFor(11)[0].alpha_divisor == 3);
    CHECK(EffectSpiralStackStylesFor(11)[1].blend_mode == 0);
  }
  SECTION("un style dilue son alpha par dix") {
    CHECK(EffectSpiralStackStylesFor(8).front().alpha_divisor == 10);
  }
  SECTION("un style inconnu NE DESSINE PAS") {
    // ⚠ C'EST UNE RÉPONSE, PAS UN OUBLI : le `switch` du client retombe sur un
    // `default` muet. Peindre un carré blanc montrerait ce qu'il cache.
    CHECK_FALSE(EffectSpiralStackStylesFor(99).front().draws);
    EffectTrack track = AdvancedTrack(0, 30);
    track.mark[kMarkOfSpiralStyle] = 99;
    CHECK(ComposeSpiralStackQuads(track, kAtTheOrigin).empty());
  }
}

TEST_CASE("le style dix a son propre gabarit", "[render][spiral_stack]") {
  // ⚠ L'OCTET NE FAIT PAS QUE CHOISIR UNE COULEUR : celui-ci refait la
  // silhouette. Quatorze étages au lieu de dix, et AUCUN rétrécissement — il
  // monte en colonne là où les autres se resserrent.
  const EffectTrack column = AdvancedTrack(10, 30);
  const EffectTrack cone = AdvancedTrack(0, 30);
  CHECK(column.mark[kMarkOfSpiralSteps] == 14);
  CHECK(cone.mark[kMarkOfSpiralSteps] == 10);
  CHECK(column.radius_at_anchor == Approx(0.0F));

  const std::vector<EffectWorldQuad> quads = ComposeSpiralStackQuads(column, kAtTheOrigin);
  REQUIRE(quads.size() == 14);
  const auto radius = [](const EffectWorldQuad& quad) {
    return std::hypot(quad.corners[3].x, quad.corners[3].z);
  };
  CHECK(radius(quads.back()) == Approx(radius(quads.front())).margin(0.01F));
}

TEST_CASE("le tourbillon tourne d'un pas TIRÉ", "[render][spiral_stack]") {
  // ⚠ TROIS À CINQ DEGRÉS PAR TIC, au hasard. Un pas fixe donnerait un
  // carrousel ; c'est ce tremblement qui fait le tourbillon.
  const EffectSpiralStackPattern& pattern = EffectSpiralStackPatternsFor(435).front();
  std::uint32_t seed = 4U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectSpiralStackTracks(pattern, seed);
  std::set<std::int32_t> step;
  std::int16_t before = tracks.front().azimuth_degrees;
  for (int tick = 0; tick < 40; ++tick) {
    ro::render::AdvanceSpiralStackTracks(tracks, seed);
    const std::int32_t offset = (tracks.front().azimuth_degrees - before + 360) % 360;
    step.insert(offset);
    before = tracks.front().azimuth_degrees;
  }
  CHECK(step.size() > 1);
  for (const std::int32_t offset : step) {
    CHECK(offset >= 3);
    CHECK(offset <= 5);
  }
}

TEST_CASE("l'alpha monte, tient un palier, puis retombe — sauf pour un style",
          "[render][spiral_stack]") {
  // La montée s'arrête au quarantième tic.
  CHECK(AdvancedTrack(0, 40).alpha == 40);
  // ⚠ LE PALIER : de quarante à soixante, rien ne bouge. Le supprimer ferait
  // pâlir le tourbillon dès qu'il a fini de paraître.
  CHECK(AdvancedTrack(0, 55).alpha == 40);
  CHECK(AdvancedTrack(0, 80).alpha < 40);
  // Le style deux ne pâlit JAMAIS de lui-même.
  CHECK(AdvancedTrack(2, 200).alpha == 40);
  // Et le style dix attend cent vingt tics.
  CHECK(AdvancedTrack(10, 100).alpha == 40);
  CHECK(AdvancedTrack(10, 160).alpha < 40);
}

TEST_CASE("les sept piles du client sont en table", "[render][spiral_stack]") {
  const std::span<const EffectSpiralStackPattern> table = EffectSpiralStackPatternTable();
  CHECK(table.size() == 7);
  CHECK(std::ranges::is_sorted(table, {}, &EffectSpiralStackPattern::effect_id));

  std::set<std::uint8_t> styles;
  for (const EffectSpiralStackPattern& pattern : table) {
    styles.insert(pattern.style);
    INFO("effet " << pattern.effect_id);
    CHECK_FALSE(pattern.texture.empty());
    CHECK(EffectSpiralStackPatternsFor(pattern.effect_id).size() == 1);
    // ⚠ AUCUN DES SEPT NE TOMBE SUR LE `default` MUET, et c'est ce qui rend
    // cette famille entièrement visible.
    CHECK(EffectSpiralStackStylesFor(pattern.style).front().draws);

    const EffectTrack track = AdvancedTrack(pattern.style, 30);
    const std::vector<EffectWorldQuad> quads = ComposeSpiralStackQuads(track, kAtTheOrigin);
    REQUIRE_FALSE(quads.empty());
    float area = 0.0F;
    for (const EffectWorldQuad& quad : quads) {
      area += QuadArea(quad);
    }
    CHECK(area > 0.0F);
  }
  CHECK(styles.size() == table.size());
  CHECK(EffectSpiralStackPatternsFor(1).empty());
}

TEST_CASE("le nœud semé rejoint la troupe", "[render][spiral_stack]") {
  const EffectSpiralStackPattern& pattern = EffectSpiralStackPatternsFor(464).front();
  std::uint32_t seed = 3U;
  ro::render::PooledEffectNode node = SowEffectSpiralStackNode(
      pattern, glm::vec3{2.0F, 0.0F, 8.0F}, ro::render::kEffectNodeDefaultLifeTicks, seed);
  CHECK(node.shape == PooledNodeShape::kSpiralStack);
  CHECK(node.resource_paths.size() == 1);

  EffectNodePool pool;
  pool.Add(node);
  for (int tick = 0; tick < 25; ++tick) {
    pool.Advance(16);
  }
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes().front().tracks[0].alpha == 25);
  // Et le style onze peint bien deux couches par étage.
  const std::vector<EffectWorldQuad> quads =
      ComposeSpiralStackQuads(pool.nodes().front().tracks[0], kAtTheOrigin);
  CHECK(quads.size() == 2 * pool.nodes().front().tracks[0].mark[kMarkOfSpiralSteps]);
}

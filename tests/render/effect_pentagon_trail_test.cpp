#include "ro/render/effect_pentagon_trail.h"

#include <algorithm>
#include <cmath>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pentagon_trail_seed.h"

using Catch::Approx;
using ro::render::ComposePentagonTrailQuads;
using ro::render::EffectNodePool;
using ro::render::EffectPentagonTrailPattern;
using ro::render::EffectPentagonTrailPatternsFor;
using ro::render::EffectPentagonTrailPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kMarkOfTrailShade;
using ro::render::kPentagonTrailBlue;
using ro::render::kPentagonTrailBlueColor;
using ro::render::kPentagonTrailPaleColor;
using ro::render::kPentagonTrailSides;
using ro::render::kTailSlotOfFarCenterY;
using ro::render::kTailSlotOfNearCenterY;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectPentagonTrailTracks;
using ro::render::SowEffectPentagonTrailNode;

namespace {

constexpr glm::vec3 kAuSol{100.0F, 0.0F, 200.0F};

[[nodiscard]] float QuadArea(const EffectWorldQuad& quad) {
  const glm::vec3 ab = quad.corners[1] - quad.corners[0];
  const glm::vec3 ac = quad.corners[2] - quad.corners[0];
  const glm::vec3 ad = quad.corners[3] - quad.corners[0];
  return (glm::length(glm::cross(ab, ac)) + glm::length(glm::cross(ac, ad))) * 0.5F;
}

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Layers(std::uint8_t octet) {
  const EffectPentagonTrailPattern pattern{0, "essai", "image.tga", octet};
  return SeedEffectPentagonTrailTracks(pattern, kAuSol);
}

}  // namespace

TEST_CASE("la traînée pentagonale a CINQ côtés, pas six", "[render][pentagon_trail]") {
  // ⚠ SIX TOURS, CINQ CÔTÉS. Le client boucle six fois, mais son pas vaut
  // soixante-douze degrés : le sixième sommet retombe sur le premier. Compter
  // les tours de boucle donnerait six quads, dont un d'aire nulle.
  const EffectTrack track = Layers(0).front();
  const std::vector<EffectWorldQuad> quads = ComposePentagonTrailQuads(track);
  REQUIRE(quads.size() == kPentagonTrailSides);
  for (const EffectWorldQuad& quad : quads) {
    CHECK(QuadArea(quad) > 0.0F);
  }
  // Et la figure SE REFERME : le dernier côté revient au premier sommet.
  CHECK(quads.back().corners[1].x == Approx(quads.front().corners[0].x).margin(0.001F));
  CHECK(quads.back().corners[1].z == Approx(quads.front().corners[0].z).margin(0.001F));
}

TEST_CASE("la traînée ne dessine rien sans piste vive", "[render][pentagon_trail]") {
  EffectTrack track = Layers(0).front();
  SECTION("éteinte") {
    track.active = false;
    CHECK(ComposePentagonTrailQuads(track).empty());
  }
  SECTION("alpha nul") {
    track.alpha = 0;
    CHECK(ComposePentagonTrailQuads(track).empty());
  }
}

TEST_CASE("le tube est tendu entre DEUX centres", "[render][pentagon_trail]") {
  const EffectTrack track = Layers(0).front();
  const std::vector<EffectWorldQuad> quads = ComposePentagonTrailQuads(track);
  REQUIRE_FALSE(quads.empty());

  // Les deux premiers coins sont sur le cercle proche, les deux autres sur le
  // lointain : un tube, pas un disque.
  const float near_point = track.tail[kTailSlotOfNearCenterY];
  const float far_point = track.tail[kTailSlotOfFarCenterY];
  CHECK(near_point != Approx(far_point));
  for (const EffectWorldQuad& quad : quads) {
    CHECK(quad.corners[0].y == Approx(near_point));
    CHECK(quad.corners[1].y == Approx(near_point));
    CHECK(quad.corners[2].y == Approx(far_point));
    CHECK(quad.corners[3].y == Approx(far_point));
  }
  // ⚠ ET LA TRAÎNÉE MONTE : le client retranche, et une soustraction élève dans
  // son repère, qui descend. Dans le nôtre, qui monte, le centre lointain est
  // donc PLUS HAUT ; recopier la soustraction l'enfonçait sous le sol.
  CHECK(far_point > near_point);
}

TEST_CASE("les coins sont ABSOLUS", "[render][pentagon_trail]") {
  // Le semis a recopié la position de l'effet dans les deux centres ; la figure
  // n'y ajoute rien. Les coins tournent donc autour du point où l'effet a été
  // posé, sans qu'aucune origine de nœud n'entre.
  const EffectTrack track = Layers(0).front();
  const std::vector<EffectWorldQuad> quads = ComposePentagonTrailQuads(track);
  REQUIRE_FALSE(quads.empty());
  float somme_x = 0.0F;
  for (const EffectWorldQuad& quad : quads) {
    somme_x += quad.corners[0].x;
  }
  // Les cinq sommets d'un pentagone régulier ont pour barycentre son centre.
  CHECK(somme_x / static_cast<float>(kPentagonTrailSides) == Approx(kAuSol.x).margin(0.01F));
}

TEST_CASE("la marque décide de la teinte, et il n'y en a que deux", "[render][pentagon_trail]") {
  EffectTrack track = Layers(0).front();
  track.mark[kMarkOfTrailShade] = kPentagonTrailBlue;
  const std::vector<EffectWorldQuad> bleus = ComposePentagonTrailQuads(track);
  REQUIRE_FALSE(bleus.empty());
  CHECK(bleus.front().tint[0] == kPentagonTrailBlueColor[0]);
  CHECK(bleus.front().tint[2] == kPentagonTrailBlueColor[2]);

  track.mark[kMarkOfTrailShade] = 7;
  const std::vector<EffectWorldQuad> pales = ComposePentagonTrailQuads(track);
  REQUIRE_FALSE(pales.empty());
  CHECK(pales.front().tint[1] == kPentagonTrailPaleColor[1]);

  // ⚠ ET LE MÉLANGE EST UNE ADDITION : le client pousse zéro au seizième
  // paramètre. Un mélange ordinaire rendrait ces nuages gris.
  CHECK(bleus.front().blend_source == 5);
  CHECK(bleus.front().blend_destination == 2);
}

TEST_CASE("les quatre couches ne se superposent pas", "[render][pentagon_trail]") {
  const std::array<EffectTrack, kEffectTracksPerNode> layers = Layers(0);
  std::set<std::int16_t> azimuts;
  std::set<float> radii;
  for (const EffectTrack& track : layers) {
    CHECK(track.active);
    azimuts.insert(track.azimuth_degrees);
    radii.insert(track.lift_key);
  }
  // Quatre azimuts distincts, et des rayons qui croissent.
  CHECK(azimuts.size() == kEffectTracksPerNode);
  CHECK(radii.size() > 1);
  CHECK(layers[1].lift_key > layers[0].lift_key);

  // ⚠ LA COUCHE DE TÊTE EST DÉJÀ LOIN : son compteur part à cent quatre-vingts,
  // et son rayon ne suit pas la suite des trois autres.
  CHECK(layers.back().ticks == 180);
  CHECK(layers[0].ticks == 0);
}

TEST_CASE("une piste sur deux respire, et l'autre tient l'échelle", "[render][pentagon_trail]") {
  const EffectPentagonTrailPattern& pattern = EffectPentagonTrailPatternsFor(307).front();
  ro::render::PooledEffectNode node =
      SowEffectPentagonTrailNode(pattern, kAuSol, ro::render::kEffectNodeDefaultLifeTicks);
  CHECK(node.shape == PooledNodeShape::kPentagonTrail);
  const std::array<float, kEffectTracksPerNode> start = {
      node.tracks[0].lift_key, node.tracks[1].lift_key, node.tracks[2].lift_key,
      node.tracks[3].lift_key};

  EffectNodePool pool;
  pool.Add(node);
  for (int tick = 0; tick < 90; ++tick) {
    pool.Advance(16);
  }
  REQUIRE(pool.size() == 1);
  const auto& after = pool.nodes().front().tracks;
  // ⚠ LES RANGS PAIRS NE BOUGENT PAS : c'est eux qui donnent l'échelle du tube.
  CHECK(after[0].lift_key == Approx(start[0]));
  CHECK(after[2].lift_key == Approx(start[2]));
  // Et les impairs respirent autour de leur valeur de repos.
  CHECK(after[1].lift_key != Approx(start[1]));
  CHECK(std::abs(after[1].lift_key - after[1].value[0]) <= Approx(0.5F).margin(0.001F));
}

TEST_CASE("les sept traînées du client sont en table", "[render][pentagon_trail]") {
  const std::span<const EffectPentagonTrailPattern> table = EffectPentagonTrailPatternTable();
  CHECK(table.size() == 7);
  CHECK(std::ranges::is_sorted(table, {}, &EffectPentagonTrailPattern::effect_id));

  std::set<std::uint8_t> octets;
  for (const EffectPentagonTrailPattern& pattern : table) {
    octets.insert(pattern.builder_argument);
    INFO("effet " << pattern.effect_id);
    CHECK_FALSE(pattern.texture.empty());
    CHECK(EffectPentagonTrailPatternsFor(pattern.effect_id).size() == 1);

    // ⚠ TOUTE COUCHE A DE L'AIRE dès le premier tic — sauf celle dont le semis
    // l'éteint volontairement, et le test le dit au lieu de l'ignorer.
    const std::array<EffectTrack, kEffectTracksPerNode> layers =
        SeedEffectPentagonTrailTracks(pattern, kAuSol);
    for (const EffectTrack& track : layers) {
      const std::vector<EffectWorldQuad> quads = ComposePentagonTrailQuads(track);
      if (track.alpha == 0) {
        CHECK(quads.empty());
        continue;
      }
      REQUIRE(quads.size() == kPentagonTrailSides);
      float area = 0.0F;
      for (const EffectWorldQuad& quad : quads) {
        area += QuadArea(quad);
      }
      CHECK(area > 0.0F);
    }
  }
  CHECK(octets.size() == table.size());
  CHECK(EffectPentagonTrailPatternsFor(1).empty());
}

TEST_CASE("l'octet onze naît invisible et le mouvement le lève", "[render][pentagon_trail]") {
  // ⚠ LE SEUL DES SEPT DANS CE CAS : son semis pose l'alpha à
  // zéro là où les six autres posent vingt-cinq, trente ou quarante.
  const std::array<EffectTrack, kEffectTracksPerNode> layers = Layers(11);
  CHECK(layers.front().alpha == 0);
  CHECK(ComposePentagonTrailQuads(layers.front()).empty());

  const EffectPentagonTrailPattern& pattern = EffectPentagonTrailPatternsFor(581).front();
  EffectNodePool pool;
  pool.Add(SowEffectPentagonTrailNode(pattern, kAuSol, ro::render::kEffectNodeDefaultLifeTicks));
  for (int tick = 0; tick < 20; ++tick) {
    pool.Advance(16);
  }
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes().front().tracks[0].alpha == 20);
  CHECK_FALSE(ComposePentagonTrailQuads(pool.nodes().front().tracks[0]).empty());
}

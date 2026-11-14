// LES FEUILLES QUI S'ÉTIRENT, sous-type 141 : la table, le semis, le mouvement et
// la figure.

#include "ro/render/effect_tapered_sheet.h"

#include <array>
#include <cstdint>
#include <optional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_tapered_sheet_seed.h"

using Catch::Approx;
using ro::render::AdvanceTaperedSheetTracks;
using ro::render::ComposeTaperedSheetQuad;
using ro::render::EffectParticleBlend;
using ro::render::EffectTaperedSheetPattern;
using ro::render::EffectTaperedSheetPatternsFor;
using ro::render::EffectTaperedSheetPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectOwnerTintAbsent;
using ro::render::kEffectTracksPerNode;
using ro::render::kTaperedSheetSlotOfHeight;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::SeedTaperedSheetTracks;
using ro::render::SowTaperedSheetNode;

namespace {

using Tracks = std::array<EffectTrack, kEffectTracksPerNode>;

constexpr std::uint32_t kFreezingStickEffectId = 811;
constexpr glm::vec3 kBearer{100.0F, 5.0F, -40.0F};

void Advance(Tracks& tracks, glm::vec3 node, std::int32_t ticks) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    AdvanceTaperedSheetTracks(tracks, node, 0, 0);
  }
}

float AreaOf(const EffectWorldQuad& quad) {
  return glm::length(
      glm::cross(quad.corners[1] - quad.corners[0], quad.corners[3] - quad.corners[0]));
}

}  // namespace

TEST_CASE("Feuilles : un site, l'effet 811, son image et son octet",
          "[render][effectnode][taperedsheet]") {
  REQUIRE(EffectTaperedSheetPatternTable().size() == 1);
  const auto patterns = EffectTaperedSheetPatternsFor(kFreezingStickEffectId);
  REQUIRE(patterns.size() == 1);
  CHECK(patterns.front().client_function == "Effect_InitStackedSprite4");
  CHECK(patterns.front().texture == "data\\texture\\effect\\freezing_stick.bmp");
  CHECK(patterns.front().octet == 0);
  CHECK(ro::render::KnowsProceduralEffect(kFreezingStickEffectId));
}

TEST_CASE("Feuilles : quatre pistes opaques, retardées de dix tics, empilées",
          "[render][effectnode][taperedsheet]") {
  const Tracks tracks = SeedTaperedSheetTracks(kBearer, 0);
  const std::array<float, 4> etages = {30.0F, 20.0F, 10.0F, 0.0F};
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].alpha == 255);
    CHECK(tracks[rang].ticks == -10 * static_cast<std::int32_t>(rang));
    CHECK(tracks[rang].radius_at_anchor == 20.0F);
    CHECK(tracks[rang].degrees == 215);
    CHECK(tracks[rang].azimuth_degrees == 0);
    // ⚠ LE CLIENT RETRANCHE L'ÉTAGE À SON ORDONNÉE, QUI DESCEND : chez nous il
    // s'ajoute.
    CHECK(tracks[rang].tail[kTaperedSheetSlotOfHeight] == Approx(kBearer.y + etages[rang]));
  }
}

TEST_CASE("Feuilles : le nœud porte le mot cinq, l'image et la vie de l'effet",
          "[render][effectnode][taperedsheet]") {
  const EffectTaperedSheetPattern& pattern =
      EffectTaperedSheetPatternsFor(kFreezingStickEffectId)[0];
  const PooledEffectNode node = SowTaperedSheetNode(pattern, kBearer, 200);
  CHECK(node.shape == PooledNodeShape::kTaperedSheets);
  REQUIRE(node.node.render_word.has_value());
  CHECK(*node.node.render_word == 5U);
  CHECK(node.node.life_ticks == 200);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == "data\\texture\\effect\\freezing_stick.bmp");
}

TEST_CASE("Feuilles : l'étirement gagne cinq par tic jusqu'à cent, puis l'alpha perd deux",
          "[render][effectnode][taperedsheet]") {
  Tracks tracks = SeedTaperedSheetTracks(kBearer, 0);
  Advance(tracks, kBearer, 1);
  CHECK(tracks[0].azimuth_degrees == 5);
  CHECK(tracks[0].alpha == 255);
  // ⚠ LA PISTE QUI ATTEND N'AVANCE QUE SON COMPTEUR.
  CHECK(tracks[1].ticks == -9);
  CHECK(tracks[1].azimuth_degrees == 0);
  Advance(tracks, kBearer, 19);
  CHECK(tracks[0].azimuth_degrees == 100);
  CHECK(tracks[0].alpha == 255);
  Advance(tracks, kBearer, 1);
  CHECK(tracks[0].azimuth_degrees == 100);
  CHECK(tracks[0].alpha == 253);
  // ⚠ À ZÉRO, LA PISTE RESTE ALLUMÉE.
  Advance(tracks, kBearer, 200);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].active);
}

TEST_CASE("Feuilles : l'ancre suit le nœud à plat et garde sa hauteur",
          "[render][effectnode][taperedsheet]") {
  Tracks tracks = SeedTaperedSheetTracks(kBearer, 0);
  const glm::vec3 elsewhere{7.0F, 99.0F, 8.0F};
  Advance(tracks, elsewhere, 1);
  CHECK(tracks[0].anchor == glm::vec3{7.0F, kBearer.y + 30.0F, 8.0F});
  CHECK(tracks[0].style == 0);
}

TEST_CASE("Feuilles : sans aire au départ, un rectangle de vingt sur quarante à cent",
          "[render][effectnode][taperedsheet]") {
  Tracks tracks = SeedTaperedSheetTracks(kBearer, 0);
  // LA PISTE QUI N'A PAS COMMENCÉ se peint, mais sans aire.
  const std::optional<EffectWorldQuad> nee = ComposeTaperedSheetQuad(tracks[0], 0, 0);
  REQUIRE(nee.has_value());
  CHECK(AreaOf(*nee) == Approx(0.0F).margin(1e-3));

  Advance(tracks, kBearer, 20);
  const std::optional<EffectWorldQuad> etiree = ComposeTaperedSheetQuad(tracks[0], 0, 0);
  REQUIRE(etiree.has_value());
  // LARGE DU RAYON, LONGUE DE DEUX RAYONS : le bord intérieur est passé de
  // l'autre côté du centre.
  CHECK(glm::length(etiree->corners[1] - etiree->corners[0]) == Approx(20.0F));
  CHECK(glm::length(etiree->corners[3] - etiree->corners[0]) == Approx(40.0F));
  CHECK(etiree->tint == std::array<std::uint8_t, 4>{100, 100, 255, 255});
  CHECK(etiree->blend_destination == EffectParticleBlend(0, kEffectOwnerTintAbsent).destination);
  // SON CENTRE EST L'ANCRE : les coins sont absolus.
  const glm::vec3 centre =
      (etiree->corners[0] + etiree->corners[1] + etiree->corners[2] + etiree->corners[3]) * 0.25F;
  CHECK(glm::length(centre - tracks[0].anchor) == Approx(0.0F).margin(1e-3));
}

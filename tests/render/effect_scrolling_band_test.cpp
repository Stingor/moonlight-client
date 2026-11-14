#include "ro/render/effect_scrolling_band.h"

#include <algorithm>
#include <cmath>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_scrolling_band_seed.h"

using Catch::Approx;
using ro::render::ComposeScrollingBandQuads;
using ro::render::EffectNodePool;
using ro::render::EffectScrollingBandBuilder;
using ro::render::EffectScrollingBandPattern;
using ro::render::EffectScrollingBandPatternsFor;
using ro::render::EffectScrollingBandPatternTable;
using ro::render::EffectScrollingBandStyleFor;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kMarkOfBandPlane;
using ro::render::kMarkOfBandSqueeze;
using ro::render::kMarkOfBandStyle;
using ro::render::kScrollingBandSegments;
using ro::render::kScrollingBandTextures;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectScrollingBandTracks;
using ro::render::SowEffectScrollingBandNode;

namespace {

constexpr glm::vec3 kAtTheOrigin{0.0F, 0.0F, 0.0F};
// Les deux couples de mélange de Direct3D que la voie A choisit.
constexpr std::int32_t kBlendSourceAlpha = 5;
constexpr std::int32_t kBlendOne = 2;
constexpr std::int32_t kBlendInverseSourceAlpha = 6;

// L'AIRE d'un quad, prise comme celle de ses deux triangles. Un quad rogné
// jusqu'à rien la rend nulle, et c'est ce qu'on veut savoir.
[[nodiscard]] float QuadArea(const EffectWorldQuad& quad) {
  const glm::vec3 ab = quad.corners[1] - quad.corners[0];
  const glm::vec3 ac = quad.corners[2] - quad.corners[0];
  const glm::vec3 ad = quad.corners[3] - quad.corners[0];
  return (glm::length(glm::cross(ab, ac)) + glm::length(glm::cross(ac, ad))) * 0.5F;
}

// Une piste de cascade ordinaire : quatre-vingts tics par cycle, style blanc.
[[nodiscard]] EffectTrack PlainTrack() {
  const EffectScrollingBandPattern pattern{
      0, "essai", {{"a", "b", "c"}}, EffectScrollingBandBuilder::kInitWaterfall, 0, 0};
  return SeedEffectScrollingBandTracks(pattern).front();
}

}  // namespace

TEST_CASE("la bande qui défile ne dessine rien sans piste vive", "[render][scrolling_band]") {
  EffectTrack track = PlainTrack();

  SECTION("piste éteinte") {
    track.active = false;
    CHECK(ComposeScrollingBandQuads(track, kAtTheOrigin).empty());
  }
  SECTION("alpha nul") {
    track.alpha = 0;
    CHECK(ComposeScrollingBandQuads(track, kAtTheOrigin).empty());
  }
  SECTION("alpha négatif") {
    track.alpha = -1;
    CHECK(ComposeScrollingBandQuads(track, kAtTheOrigin).empty());
  }
}

TEST_CASE("la cascade pend AU-DESSUS de son ancre, jusqu'au sol", "[render][scrolling_band]") {
  // Le client écrit la bande à des ordonnées négatives, de moins cent soixante
  // à zéro, dans son repère qui descend. Recopiées, elles l'enterraient.
  EffectTrack track = PlainTrack();
  track.ticks = 17;
  const std::vector<EffectWorldQuad> quads = ComposeScrollingBandQuads(track, kAtTheOrigin);
  REQUIRE_FALSE(quads.empty());
  float highest = 0.0F;
  for (const EffectWorldQuad& quad : quads) {
    for (const glm::vec3& corner : quad.corners) {
      CHECK(corner.y >= -0.001F);
      highest = std::max(highest, corner.y);
    }
  }
  CHECK(highest > 100.0F);
}

TEST_CASE("la bande compte cinq segments bout à bout", "[render][scrolling_band]") {
  EffectTrack track = PlainTrack();
  track.ticks = 17;
  const std::vector<EffectWorldQuad> quads = ComposeScrollingBandQuads(track, kAtTheOrigin);
  REQUIRE(quads.size() == kScrollingBandSegments);

  // ⚠ LES TROIS SEGMENTS DU MILIEU SE TOUCHENT EXACTEMENT : le haut de l'un est
  // le bas du suivant. Un écart ici ferait une couture visible à chaque tic.
  for (std::size_t rang = 1; rang + 1 < kScrollingBandSegments; ++rang) {
    CHECK(quads[rang].corners[2].y == Approx(quads[rang - 1].corners[0].y));
  }
  // Et chacun porte de l'aire.
  for (const EffectWorldQuad& quad : quads) {
    CHECK(QuadArea(quad) > 0.0F);
  }
}

TEST_CASE("les deux bouts de la bande sont rognés, image comprise", "[render][scrolling_band]") {
  EffectTrack track = PlainTrack();
  // Un tic où la phase n'est ni au début ni à la fin de son cycle.
  track.ticks = 20;
  const std::vector<EffectWorldQuad> quads = ComposeScrollingBandQuads(track, kAtTheOrigin);
  REQUIRE(quads.size() == kScrollingBandSegments);

  const EffectWorldQuad& first = quads.front();
  const EffectWorldQuad& middle = quads[2];
  const EffectWorldQuad& last = quads.back();

  // Le premier et le dernier sont PLUS COURTS qu'un segment plein.
  const float full = std::abs(middle.corners[2].y - middle.corners[0].y);
  CHECK(std::abs(first.corners[2].y - first.corners[0].y) < full);
  CHECK(std::abs(last.corners[2].y - last.corners[0].y) < full);

  // ⚠ ET LEUR IMAGE EST COUPÉE DU MÊME PAS. C'est là toute la figure : rogner la
  // géométrie sans rogner l'image étirerait la texture au lieu de la faire
  // glisser. Le milieu, lui, porte l'image entière.
  CHECK(middle.texture_uv[0].y == Approx(1.0F));
  CHECK(middle.texture_uv[2].y == Approx(0.0F));
  CHECK(first.texture_uv[2].y > 0.0F);
  CHECK(last.texture_uv[0].y < 1.0F);
}

TEST_CASE("la bande glisse d'un cycle entier et se recolle", "[render][scrolling_band]") {
  EffectTrack track = PlainTrack();
  const auto cycle = static_cast<std::int32_t>(track.lift_key);
  REQUIRE(cycle > 1);

  track.ticks = 0;
  const std::vector<EffectWorldQuad> start = ComposeScrollingBandQuads(track, kAtTheOrigin);
  track.ticks = 1;
  const std::vector<EffectWorldQuad> after = ComposeScrollingBandQuads(track, kAtTheOrigin);
  track.ticks = cycle;
  const std::vector<EffectWorldQuad> loop = ComposeScrollingBandQuads(track, kAtTheOrigin);

  REQUIRE(start.size() == kScrollingBandSegments);
  REQUIRE(after.size() == kScrollingBandSegments);
  REQUIRE(loop.size() == kScrollingBandSegments);

  // Un tic plus tard, la bande a bougé.
  CHECK(after[2].corners[0].y != Approx(start[2].corners[0].y));
  // Un cycle plus tard, elle est revenue à sa place — c'est ce qui la rend sans
  // couture.
  CHECK(loop[2].corners[0].y == Approx(start[2].corners[0].y));
  // ⚠ MAIS SON IMAGE A AVANCÉ D'UN CRAN, et c'est cela qui empêche la boucle de
  // se voir : la géométrie se répète, l'image non.
  CHECK(loop[2].texture != start[2].texture);
}

TEST_CASE("chaque segment prend son image dans le jeu de trois", "[render][scrolling_band]") {
  EffectTrack track = PlainTrack();
  for (std::int32_t tick = 0; tick < 400; ++tick) {
    track.ticks = tick;
    for (const EffectWorldQuad& quad : ComposeScrollingBandQuads(track, kAtTheOrigin)) {
      CHECK(quad.texture < kScrollingBandTextures);
    }
  }
}

TEST_CASE("le style décide de la teinte et du mélange", "[render][scrolling_band]") {
  SECTION("le style ordinaire peint blanc et se mélange") {
    EffectTrack track = PlainTrack();
    track.mark[kMarkOfBandStyle] = 0;
    const std::vector<EffectWorldQuad> quads = ComposeScrollingBandQuads(track, kAtTheOrigin);
    REQUIRE_FALSE(quads.empty());
    CHECK(quads.front().tint[0] == 255);
    CHECK(quads.front().tint[1] == 255);
    CHECK(quads.front().tint[2] == 255);
    CHECK(quads.front().blend_source == kBlendSourceAlpha);
    CHECK(quads.front().blend_destination == kBlendInverseSourceAlpha);
  }
  SECTION("l'eau profonde peint bleu et S'AJOUTE") {
    EffectTrack track = PlainTrack();
    track.mark[kMarkOfBandStyle] = 1;
    const std::vector<EffectWorldQuad> quads = ComposeScrollingBandQuads(track, kAtTheOrigin);
    REQUIRE_FALSE(quads.empty());
    CHECK(quads.front().tint[0] == 55);
    CHECK(quads.front().tint[1] == 55);
    CHECK(quads.front().tint[2] == 255);
    // ⚠ L'ADDITION, ET NON LE MÉLANGE : c'est ce qui fait briller l'eau au lieu
    // de la rendre grise. Voir effect_tinted_quad.cpp, où la même erreur a été
    // payée sur les étincelles du soin.
    CHECK(quads.front().blend_source == kBlendSourceAlpha);
    CHECK(quads.front().blend_destination == kBlendOne);
  }
  SECTION("un style inconnu retombe sur l'ordinaire") {
    CHECK(EffectScrollingBandStyleFor(77).blend_mode == 1);
    CHECK_FALSE(EffectScrollingBandStyleFor(77).reversed);
  }
}

TEST_CASE("la marque de plan fait basculer la bande d'un quart de tour",
          "[render][scrolling_band]") {
  EffectTrack couchee = PlainTrack();
  couchee.ticks = 11;
  EffectTrack debout = couchee;
  debout.mark[kMarkOfBandPlane] = 1;

  const std::vector<EffectWorldQuad> plat = ComposeScrollingBandQuads(couchee, kAtTheOrigin);
  const std::vector<EffectWorldQuad> rotated = ComposeScrollingBandQuads(debout, kAtTheOrigin);
  REQUIRE(plat.size() == rotated.size());
  REQUIRE_FALSE(plat.empty());

  // La largeur passe d'un axe à l'autre, et la hauteur ne bouge pas.
  CHECK(plat.front().corners[0].x != Approx(plat.front().corners[1].x));
  CHECK(rotated.front().corners[0].x == Approx(rotated.front().corners[1].x));
  CHECK(rotated.front().corners[0].z != Approx(rotated.front().corners[1].z));
  CHECK(rotated.front().corners[0].y == Approx(plat.front().corners[0].y));
}

TEST_CASE("le tassement ne tasse qu'au-dessus de dix", "[render][scrolling_band]") {
  EffectTrack track = PlainTrack();
  track.ticks = 9;
  const float full = QuadArea(ComposeScrollingBandQuads(track, kAtTheOrigin)[2]);

  // Dix ne tasse rien — et c'est la valeur que l'un des deux bâtisseurs pose.
  track.mark[kMarkOfBandSqueeze] = 10;
  CHECK(QuadArea(ComposeScrollingBandQuads(track, kAtTheOrigin)[2]) == Approx(full));

  track.mark[kMarkOfBandSqueeze] = 20;
  CHECK(QuadArea(ComposeScrollingBandQuads(track, kAtTheOrigin)[2]) < full);
}

TEST_CASE("les coins sont relatifs à l'origine du nœud", "[render][scrolling_band]") {
  EffectTrack track = PlainTrack();
  track.ticks = 5;
  const glm::vec3 elsewhere{120.0F, -8.0F, 240.0F};
  const std::vector<EffectWorldQuad> here = ComposeScrollingBandQuads(track, kAtTheOrigin);
  const std::vector<EffectWorldQuad> over_there = ComposeScrollingBandQuads(track, elsewhere);
  REQUIRE(here.size() == over_there.size());
  REQUIRE_FALSE(here.empty());
  for (std::size_t rang = 0; rang < here.size(); ++rang) {
    for (std::size_t corner = 0; corner < 4; ++corner) {
      CHECK(over_there[rang].corners[corner].x ==
            Approx(here[rang].corners[corner].x + elsewhere.x));
      CHECK(over_there[rang].corners[corner].y ==
            Approx(here[rang].corners[corner].y + elsewhere.y));
      CHECK(over_there[rang].corners[corner].z ==
            Approx(here[rang].corners[corner].z + elsewhere.z));
    }
  }
}

TEST_CASE("les treize cascades du client sont en table", "[render][scrolling_band]") {
  const std::span<const EffectScrollingBandPattern> table = EffectScrollingBandPatternTable();
  CHECK(table.size() == 13);

  std::set<std::uint32_t> identifiants;
  for (const EffectScrollingBandPattern& pattern : table) {
    identifiants.insert(pattern.effect_id);
    // Trois images, toujours, et toutes nommées.
    CHECK(pattern.textures.size() == kScrollingBandTextures);
    for (const std::string_view image : pattern.textures) {
      CHECK_FALSE(image.empty());
    }
    CHECK_FALSE(pattern.client_function.empty());
  }
  CHECK(identifiants.size() == table.size());
  // La table est TRIÉE : `EffectScrollingBandPatternsFor` la lit par intervalle.
  CHECK(std::ranges::is_sorted(table, {}, &EffectScrollingBandPattern::effect_id));
  for (const std::uint32_t effect_id : identifiants) {
    CHECK(EffectScrollingBandPatternsFor(effect_id).size() == 1);
  }
  CHECK(EffectScrollingBandPatternsFor(1).empty());
}

TEST_CASE("toute cascade relevée montre quatre rideaux qui ont de l'aire",
          "[render][scrolling_band]") {
  for (const EffectScrollingBandPattern& pattern : EffectScrollingBandPatternTable()) {
    const std::array<EffectTrack, kEffectTracksPerNode> tracks =
        SeedEffectScrollingBandTracks(pattern);

    std::set<float> cycles;
    std::set<float> largeurs;
    std::set<float> profondeurs;
    for (const EffectTrack& track : tracks) {
      INFO("effet " << pattern.effect_id);
      CHECK(track.active);
      CHECK(track.alpha > 0);
      CHECK(track.radius_at_anchor > 0.0F);
      cycles.insert(track.lift_key);
      largeurs.insert(track.radius_at_anchor);
      profondeurs.insert(track.anchor.z);
    }
    // ⚠ LES QUATRE RIDEAUX NE SE SUPERPOSENT PAS : largeurs et profondeurs
    // distinctes, toujours. C'est cela qui fait une eau plutôt qu'un plan.
    INFO("effet " << pattern.effect_id);
    CHECK(largeurs.size() == kEffectTracksPerNode);
    CHECK(profondeurs.size() == kEffectTracksPerNode);

    // ⚠ ET LA BANDE A DE L'AIRE À CHAQUE TIC, pas seulement au premier. Un
    // compte cumulé passerait même si la figure ne paraissait qu'une fois.
    for (std::int32_t tick = 0; tick < 90; ++tick) {
      for (const EffectTrack& modele : tracks) {
        EffectTrack track = modele;
        track.ticks = tick;
        const std::vector<EffectWorldQuad> quads = ComposeScrollingBandQuads(track, kAtTheOrigin);
        INFO("effet " << pattern.effect_id << " au tic " << tick);
        REQUIRE_FALSE(quads.empty());
        float area = 0.0F;
        for (const EffectWorldQuad& quad : quads) {
          area += QuadArea(quad);
        }
        CHECK(area > 0.0F);
      }
    }
  }
}

TEST_CASE("la cascade figée garde toute son aire", "[render][scrolling_band]") {
  // ⚠ LE RECOUPEMENT DES DEUX MESURES : l'unique identifiant dont le cycle est
  // nul est aussi le seul dont le style renverse la phase. Le cycle nul met la
  // phase au bout de sa course, le renversement la ramène à zéro, et le quad
  // unique garde toute son aire. Un seul des deux et la bande disparaîtrait.
  const std::span<const EffectScrollingBandPattern> patterns = EffectScrollingBandPatternsFor(711);
  REQUIRE(patterns.size() == 1);
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectScrollingBandTracks(patterns.front());
  for (const EffectTrack& modele : tracks) {
    CHECK(modele.lift_key == Approx(0.0F));
    EffectTrack track = modele;
    for (std::int32_t tick = 0; tick < 40; ++tick) {
      track.ticks = tick;
      const std::vector<EffectWorldQuad> quads = ComposeScrollingBandQuads(track, kAtTheOrigin);
      REQUIRE(quads.size() == 1);
      CHECK(QuadArea(quads.front()) > 0.0F);
    }
  }
}

TEST_CASE("les deux bâtisseurs ne donnent pas la même cascade", "[render][scrolling_band]") {
  // 351 et 352 ne diffèrent que par le plan ; 557 et 559 que par la vitesse.
  const auto tracks = [](std::uint32_t effect_id) {
    return SeedEffectScrollingBandTracks(EffectScrollingBandPatternsFor(effect_id).front());
  };
  CHECK(tracks(349).front().radius_at_anchor != Approx(tracks(351).front().radius_at_anchor));
  CHECK(tracks(351).front().mark[kMarkOfBandPlane] != tracks(352).front().mark[kMarkOfBandPlane]);
  CHECK(tracks(557).front().lift_key != Approx(tracks(559).front().lift_key));
  // Les deux bâtisseurs n'échelonnent pas leurs rideaux du même pas.
  CHECK(tracks(349)[1].anchor.z != Approx(tracks(557)[1].anchor.z));
}

TEST_CASE("le nœud semé rejoint la troupe et son compteur avance", "[render][scrolling_band]") {
  const EffectScrollingBandPattern& pattern = EffectScrollingBandPatternsFor(557).front();
  ro::render::PooledEffectNode node =
      SowEffectScrollingBandNode(pattern, glm::vec3{4.0F, 0.0F, 9.0F}, 200);
  CHECK(node.shape == PooledNodeShape::kScrollingBand);
  CHECK(node.resource_paths.size() == kScrollingBandTextures);
  // ⚠ UNE PISTE ÉTEINTE N'AVANCE PAS : c'est la garde du mouvement du client.
  node.tracks[3].active = false;

  EffectNodePool pool;
  pool.Add(node);
  pool.Advance(16);
  pool.Advance(16);
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes().front().tracks[0].ticks == 2);
  CHECK(pool.nodes().front().tracks[3].ticks == 0);
}

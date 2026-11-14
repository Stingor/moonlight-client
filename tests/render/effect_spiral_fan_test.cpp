#include "ro/render/effect_spiral_fan.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"

using ro::render::ComposeSpiralFanQuads;
using ro::render::EffectSpiralFanStyle;
using ro::render::EffectSpiralFanStyleFor;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;

namespace {

constexpr std::size_t kSlotOfFirstBlade = 1;
constexpr std::size_t kSlotOfLastBlade = 10;
constexpr std::size_t kBladeCount = 10;
constexpr std::size_t kMarkOfStyle = 4;

// Les trois styles que le semis du sous-type 147 emploie.
constexpr std::uint8_t kStyleThatStays = 5;
constexpr std::uint8_t kStyleThatOpens = 6;
constexpr std::uint8_t kStyleThatOpensRed = 7;
// Deux styles qu'un autre sous-type apportera, et que le client traite à part.
constexpr std::uint8_t kStyleInBlack = 0;
constexpr std::uint8_t kStyleThatDoubles = 3;
// Un style qu'aucun `case` ne nomme.
constexpr std::uint8_t kStyleNobodyNames = 200;

constexpr float kRadius = 21.0F;

// Une piste vive dont les dix lames brillent toutes. La figure ne dessine que
// les lames dont l'éclat est strictement positif : les allumer toutes est ce qui
// permet de compter les quads.
EffectTrack OneLive(std::uint8_t style, float shard = 100.0F) {
  EffectTrack track;
  track.active = true;
  track.radius_at_anchor = kRadius;
  track.mark[kMarkOfStyle] = style;
  for (std::size_t lame = 0; lame < kBladeCount; ++lame) {
    track.value[kSlotOfFirstBlade + lame] = shard;
  }
  return track;
}

// La couleur d'un quad, et son alpha. Le quad les range ensemble, en quatre
// canaux ; les tests parlent de l'un ou de l'autre.
[[nodiscard]] std::array<std::uint8_t, 3> ColorOf(const EffectWorldQuad& quad) {
  return {quad.tint[0], quad.tint[1], quad.tint[2]};
}

[[nodiscard]] std::uint8_t Alpha(const EffectWorldQuad& quad) { return quad.tint[3]; }

// Le rayon apparent d'un quad, mesuré depuis son centre. Les quatre coins sont
// sur un même cercle, et c'est la seule grandeur que la figure fait varier d'une
// lame à l'autre.
[[nodiscard]] float OffsetFromCenter(const EffectWorldQuad& quad) {
  glm::vec3 centre{0.0F, 0.0F, 0.0F};
  for (const auto& vertex : quad.corners) {
    centre += vertex;
  }
  centre /= static_cast<float>(quad.corners.size());
  return glm::length(quad.corners[0] - centre);
}

}  // namespace

TEST_CASE("Éventail : redressée vers l'œil, une lame montre le haut de son image en haut",
          "[render][figure]") {
  // Une caméra au lacet nul et au tangage de quarante-cinq degrés : la lame se
  // couche de quarante-cinq degrés pour lui faire face. Un glyphe doit s'y lire
  // à l'endroit — la première ligne de l'image, v = 0, au-dessus de la dernière.
  constexpr std::int16_t kTurnFacingYawZero = 0;
  constexpr std::int16_t kTiltFacingPitch45 = 45;
  const std::vector<EffectWorldQuad> quads =
      ComposeSpiralFanQuads(OneLive(kStyleInBlack), kTurnFacingYawZero, kTiltFacingPitch45, {});
  REQUIRE_FALSE(quads.empty());
  for (const EffectWorldQuad& quad : quads) {
    float top_height = 0.0F;
    float bottom_height = 0.0F;
    for (std::size_t corner = 0; corner < quad.corners.size(); ++corner) {
      (quad.texture_uv[corner].y == 0.0F ? top_height : bottom_height) += quad.corners[corner].y;
    }
    CHECK(top_height > bottom_height);
  }
}

TEST_CASE("Éventail : une piste éteinte ne sort AUCUN quad", "[render][figure]") {
  EffectTrack track = OneLive(kStyleThatStays);
  track.active = false;
  CHECK(ComposeSpiralFanQuads(track, 0, 0, {}).empty());
}

TEST_CASE("Éventail : TOUT style dessine, même celui que le client ne nomme pas",
          "[render][figure]") {
  // ⚠ CETTE FIGURE N'A PAS DE CAS MUET, contrairement à l'arc et au carré
  // teinté. Lui en inventer un par symétrie ferait disparaître des lames.
  const EffectSpiralFanStyle unknown = EffectSpiralFanStyleFor(kStyleNobodyNames);
  CHECK(unknown.color == std::array<std::uint8_t, 3>{255, 255, 255});
  CHECK_FALSE(unknown.has_aura);
  CHECK_FALSE(ComposeSpiralFanQuads(OneLive(kStyleNobodyNames), 0, 0, {}).empty());
}

TEST_CASE("Éventail : DIX lames sortent, une par case", "[render][figure]") {
  // Le style le plus simple : une lame, un quad, pas d'aura.
  const std::vector<EffectWorldQuad> quads =
      ComposeSpiralFanQuads(OneLive(kStyleInBlack), 0, 0, {});
  CHECK(quads.size() == kBladeCount);
  for (const EffectWorldQuad& quad : quads) {
    CHECK(ColorOf(quad) == std::array<std::uint8_t, 3>{0, 0, 0});
  }
}

TEST_CASE("Éventail : une lame SANS ÉCLAT ne sort pas", "[render][figure]") {
  // ⚠ C'EST CE QUI FAIT L'OUVERTURE. Pendant la montée, les lames s'allument
  // l'une après l'autre ; les soumettre toutes dès le premier tic montrerait
  // l'éventail entier d'emblée.
  EffectTrack track = OneLive(kStyleInBlack, 0.0F);
  CHECK(ComposeSpiralFanQuads(track, 0, 0, {}).empty());
  track.value[kSlotOfLastBlade] = 80.0F;
  CHECK(ComposeSpiralFanQuads(track, 0, 0, {}).size() == 1);
}

TEST_CASE("Éventail : la lame la plus BRILLANTE est la plus SERRÉE", "[render][figure]") {
  // ⚠ C'EST LE SENS DU DÉGRADÉ. Le client lit ses cases à rebours du rayon :
  // inverser le parcours retournerait l'éventail, clair au bord et sombre au
  // centre.
  EffectTrack track = OneLive(kStyleInBlack, 10.0F);
  track.value[kSlotOfLastBlade] = 250.0F;
  const std::vector<EffectWorldQuad> quads = ComposeSpiralFanQuads(track, 0, 0, {});
  REQUIRE(quads.size() == kBladeCount);
  CHECK(Alpha(quads.front()) == 250);
  CHECK(Alpha(quads.back()) == 10);
  CHECK(OffsetFromCenter(quads.front()) < OffsetFromCenter(quads.back()));
}

TEST_CASE("Éventail : les lames s'écartent d'un PAS que le style choisit", "[render][figure]") {
  // Trois styles serrent l'éventail d'une unité par lame, tous les autres
  // l'étalent de trois. C'est la seule chose qui en règle l'envergure.
  const std::vector<EffectWorldQuad> serre =
      ComposeSpiralFanQuads(OneLive(kStyleThatStays), 0, 0, {});
  const std::vector<EffectWorldQuad> etale =
      ComposeSpiralFanQuads(OneLive(kStyleInBlack), 0, 0, {});
  REQUIRE(!serre.empty());
  REQUIRE(!etale.empty());
  CHECK(OffsetFromCenter(serre.back()) < OffsetFromCenter(etale.back()));
  CHECK(OffsetFromCenter(serre.front()) == Catch::Approx(OffsetFromCenter(etale.front())));
}

TEST_CASE("Éventail : trois styles portent une AURA, les autres non", "[render][figure]") {
  // L'aura double le nombre de quads : un halo additif par lame.
  CHECK(ComposeSpiralFanQuads(OneLive(kStyleThatStays), 0, 0, {}).size() == 2 * kBladeCount);
  CHECK(ComposeSpiralFanQuads(OneLive(kStyleInBlack), 0, 0, {}).size() == kBladeCount);

  // ⚠ ET ELLE COMMENCE UN STYLE AVANT L'OUVERTURE. Le style qui ne s'ouvre pas
  // en a une aussi, bleue ; l'oublier lui retirerait son halo en silence.
  CHECK(EffectSpiralFanStyleFor(kStyleThatStays).has_aura);
  CHECK(EffectSpiralFanStyleFor(kStyleThatStays).aura_color ==
        std::array<std::uint8_t, 3>{5, 5, 255});
  // Un seul des trois vire au rouge.
  CHECK(EffectSpiralFanStyleFor(kStyleThatOpensRed).aura_color ==
        std::array<std::uint8_t, 3>{255, 5, 5});
  CHECK(EffectSpiralFanStyleFor(kStyleThatOpens).aura_color ==
        std::array<std::uint8_t, 3>{5, 5, 255});
}

TEST_CASE("Éventail : l'aura est PLUS GRANDE que sa lame et vient APRÈS elle", "[render][figure]") {
  const std::vector<EffectWorldQuad> quads =
      ComposeSpiralFanQuads(OneLive(kStyleThatStays), 0, 0, {});
  REQUIRE(quads.size() >= 2);
  CHECK(OffsetFromCenter(quads[1]) > OffsetFromCenter(quads[0]));
  // ⚠ L'ORDRE COMPTE : ces quads se mélangent à la destination, et poser l'aura
  // avant sa lame change l'image.
  CHECK(ColorOf(quads[0]) == std::array<std::uint8_t, 3>{255, 255, 255});
  CHECK(ColorOf(quads[1]) == std::array<std::uint8_t, 3>{5, 5, 255});
}

TEST_CASE("Éventail : un seul style DOUBLE sa lame la plus serrée", "[render][figure]") {
  const std::vector<EffectWorldQuad> quads =
      ComposeSpiralFanQuads(OneLive(kStyleThatDoubles, 100.0F), 0, 0, {});
  // Onze quads pour dix lames : la première sort deux fois.
  CHECK(quads.size() == kBladeCount + 1);
  CHECK(Alpha(quads[0]) == 50);
  CHECK(Alpha(quads[1]) == 100);
  CHECK(OffsetFromCenter(quads[0]) == Catch::Approx(OffsetFromCenter(quads[1])));
}

TEST_CASE("Éventail : les deux styles qui s'ouvrent APLATISSENT leurs coins", "[render][figure]") {
  // ⚠ LES RÉGULARISER LES RENDRAIT INDISCERNABLES DES AUTRES. Leurs quatre coins
  // se resserrent de dix degrés vers l'horizontale.
  CHECK(EffectSpiralFanStyleFor(kStyleThatOpens).corner_degrees ==
        std::array<std::int32_t, 4>{35, 145, 215, 325});
  CHECK(EffectSpiralFanStyleFor(kStyleInBlack).corner_degrees ==
        std::array<std::int32_t, 4>{45, 135, 225, 315});
}

TEST_CASE("Éventail : la figure MONTE au-dessus de l'origine du nœud", "[render][figure]") {
  // ⚠ LE REPÈRE DU CLIENT DESCEND : sa soustraction élève. Recopier le signe
  // enfoncerait l'éventail de soixante unités sous le sol.
  const std::vector<EffectWorldQuad> quads =
      ComposeSpiralFanQuads(OneLive(kStyleInBlack), 0, 0, {0.0F, 0.0F, 0.0F});
  REQUIRE(!quads.empty());
  for (const glm::vec3& vertex : quads.front().corners) {
    CHECK(vertex.y > 0.0F);
  }
  CHECK(EffectSpiralFanStyleFor(kStyleInBlack).lift_above_anchor == Catch::Approx(30.0F));
  CHECK(EffectSpiralFanStyleFor(kStyleThatOpens).lift_above_anchor == Catch::Approx(40.0F));
}

TEST_CASE("Éventail : l'origine du nœud PORTE la figure", "[render][figure]") {
  // Les coins sont relatifs : un nœud posé ailleurs emporte tout son éventail.
  const glm::vec3 elsewhere{100.0F, 0.0F, -40.0F};
  const std::vector<EffectWorldQuad> here = ComposeSpiralFanQuads(OneLive(kStyleInBlack), 0, 0, {});
  const std::vector<EffectWorldQuad> la =
      ComposeSpiralFanQuads(OneLive(kStyleInBlack), 0, 0, elsewhere);
  REQUIRE(here.size() == la.size());
  REQUIRE(!here.empty());
  for (std::size_t corner = 0; corner < here.front().corners.size(); ++corner) {
    const glm::vec3 offset = la.front().corners[corner] - here.front().corners[corner];
    CHECK(offset.x == Catch::Approx(elsewhere.x));
    CHECK(offset.z == Catch::Approx(elsewhere.z));
  }
}

TEST_CASE("Éventail : la case ZÉRO pousse tout l'éventail de côté", "[render][figure]") {
  // ⚠ ET ELLE N'EST PAS LA PREMIÈRE LAME. La prendre pour telle décalerait le
  // dégradé d'un cran et perdrait la lame la plus brillante.
  EffectTrack nudged = OneLive(kStyleInBlack);
  nudged.value[0] = 12.0F;
  const std::vector<EffectWorldQuad> straight =
      ComposeSpiralFanQuads(OneLive(kStyleInBlack), 0, 0, {});
  const std::vector<EffectWorldQuad> shifted = ComposeSpiralFanQuads(nudged, 0, 0, {});
  REQUIRE(straight.size() == shifted.size());
  REQUIRE(!straight.empty());
  for (std::size_t corner = 0; corner < straight.front().corners.size(); ++corner) {
    CHECK(shifted.front().corners[corner].x - straight.front().corners[corner].x ==
          Catch::Approx(12.0F));
  }
}

#include "ro/render/effect_tinted_quad.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_track.h"

using ro::render::ComposeTintedRotatedQuad;
using ro::render::EffectTintedQuadStyle;
using ro::render::EffectTintedQuadStyleFor;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;

namespace {

// Les cases que cette figure lit, redites ici pour que le test parle de ce que
// le client lit et non d'un index nu.
constexpr std::size_t kSlotOfSpinDegrees = 8;
constexpr std::size_t kSlotOfNudgeKey = 9;

// Une marque qui dessine, additive, coins échangés.
constexpr std::uint8_t kOrdinaryMark = 13;
// L'une des trois seules qui gardent l'ordre des coins.
constexpr std::uint8_t kMarkThatKeepsCornerOrder = 15;
// Celle qui délègue à une sous-marque, et celle qui s'élève.
constexpr std::uint8_t kMarkThatDelegates = 9;
constexpr std::uint8_t kMarkThatRises = 25;
// Une marque que le client ne dessine pas.
constexpr std::uint8_t kSilentMark = 20;

constexpr std::uint8_t kNoSubMark = 0;

EffectTrack LiveTrack() {
  EffectTrack track;
  track.active = true;
  track.alpha = 200;
  track.radius_at_anchor = 10.0F;
  track.degrees = 0;
  track.anchor = {5.0F, 2.0F, -3.0F};
  return track;
}

float AreaOf(const EffectWorldQuad& quad) {
  return glm::length(
      glm::cross(quad.corners[1] - quad.corners[0], quad.corners[3] - quad.corners[0]));
}

}  // namespace

TEST_CASE("Carré teinté : une marque muette ne dessine RIEN", "[render][figure]") {
  // Six marques sur vingt-six retombent sur le `default` du client. Poser un
  // carré blanc à leur place montrerait ce qu'il cache.
  CHECK_FALSE(EffectTintedQuadStyleFor(kSilentMark, kNoSubMark).draws);
  CHECK_FALSE(ComposeTintedRotatedQuad(LiveTrack(), kSilentMark, kNoSubMark, 0, 0).has_value());
}

TEST_CASE("Carré teinté : la marque qui délègue ne dessine pas sans sa sous-marque",
          "[render][figure]") {
  // Zéro n'est pas une sous-marque : le client n'a pas de branche pour elle.
  CHECK_FALSE(EffectTintedQuadStyleFor(kMarkThatDelegates, kNoSubMark).draws);
  for (std::uint8_t sub = 1; sub <= 6; ++sub) {
    INFO("sous-marque " << static_cast<int>(sub));
    CHECK(EffectTintedQuadStyleFor(kMarkThatDelegates, sub).draws);
  }
}

TEST_CASE("Carré teinté : NOIR n'est pas éteint", "[render][figure]") {
  // La marque 16 peint vraiment du noir. La prendre pour une absence de teinte
  // rendrait son carré blanc, ce qui se verrait mais s'expliquerait mal.
  const EffectTintedQuadStyle noire = EffectTintedQuadStyleFor(16, kNoSubMark);
  REQUIRE(noire.draws);
  CHECK(noire.color == std::array<std::uint8_t, 3>{0, 0, 0});
}

TEST_CASE("Carré teinté : trois marques SEULEMENT gardent l'ordre des coins", "[render][figure]") {
  // ⚠ LA MARQUE UN EN EST, bien que ses voisines échangent : son appel empile les
  // coins comme la marque zéro. C'est la teinte du
  // projectile de Finger Offensive, dont l'image se poserait retournée sinon.
  CHECK_FALSE(EffectTintedQuadStyleFor(0, kNoSubMark).swaps_corners);
  CHECK_FALSE(EffectTintedQuadStyleFor(1, kNoSubMark).swaps_corners);
  CHECK(EffectTintedQuadStyleFor(2, kNoSubMark).swaps_corners);
  CHECK_FALSE(EffectTintedQuadStyleFor(kMarkThatKeepsCornerOrder, kNoSubMark).swaps_corners);
  std::size_t gardent = 0;
  std::size_t dessinent = 0;
  for (std::uint16_t mark = 0; mark <= 255; ++mark) {
    const EffectTintedQuadStyle style =
        EffectTintedQuadStyleFor(static_cast<std::uint8_t>(mark), kNoSubMark);
    if (!style.draws) {
      continue;
    }
    ++dessinent;
    if (!style.swaps_corners) {
      ++gardent;
    }
  }
  CHECK(gardent == 3);
  CHECK(dessinent > gardent);
}

TEST_CASE("Carré teinté : les quatre coins font un CARRÉ autour de l'ancre", "[render][figure]") {
  EffectTrack track = LiveTrack();
  const std::optional<EffectWorldQuad> quad =
      ComposeTintedRotatedQuad(track, kMarkThatKeepsCornerOrder, kNoSubMark, 0, 0);
  REQUIRE(quad.has_value());
  // Sans rotation, les quatre coins sont à distance égale de l'ancre — c'est ce
  // qui sépare ce dessinateur de l'arc, dont un côté est plus large.
  for (const glm::vec3& corner : quad->corners) {
    INFO("coin " << corner.x << ", " << corner.y << ", " << corner.z);
    CHECK(glm::length(corner - track.anchor) == Catch::Approx(track.radius_at_anchor));
  }
  CHECK(AreaOf(*quad) > 0.0F);
}

TEST_CASE("Carré teinté : le bas de l'image va aux deux premiers coins", "[render][figure]") {
  // Le client pose v = 1 sur A et B, v = 0 sur C et D. L'inverse retournait
  // toutes les images de la figure, dont celles des chansons.
  const std::optional<EffectWorldQuad> quad =
      ComposeTintedRotatedQuad(LiveTrack(), kMarkThatKeepsCornerOrder, kNoSubMark, 0, 0);
  REQUIRE(quad.has_value());
  CHECK(quad->texture_uv[0] == glm::vec2{0.0F, 1.0F});
  CHECK(quad->texture_uv[1] == glm::vec2{1.0F, 1.0F});
  CHECK(quad->texture_uv[2] == glm::vec2{1.0F, 0.0F});
  CHECK(quad->texture_uv[3] == glm::vec2{0.0F, 0.0F});
}

TEST_CASE("Carré teinté : un angle propre HORS DU TOUR fige le rayon", "[render][figure]") {
  // Le client remet la case à zéro au lieu d'en prendre le reste : une piste qui
  // a débordé cesse de respirer, elle ne repart pas au milieu du cycle.
  EffectTrack debordee = LiveTrack();
  debordee.value[kSlotOfSpinDegrees] = 450.0F;
  const std::optional<EffectWorldQuad> hors =
      ComposeTintedRotatedQuad(debordee, kMarkThatKeepsCornerOrder, kNoSubMark, 0, 0);
  REQUIRE(hors.has_value());

  EffectTrack au_repos = LiveTrack();
  au_repos.value[kSlotOfSpinDegrees] = 0.0F;
  const std::optional<EffectWorldQuad> repos =
      ComposeTintedRotatedQuad(au_repos, kMarkThatKeepsCornerOrder, kNoSubMark, 0, 0);
  REQUIRE(repos.has_value());
  CHECK(AreaOf(*hors) == Catch::Approx(AreaOf(*repos)));

  // Et un angle DANS le tour, lui, fait bien respirer : sans quoi le test
  // ci-dessus passerait pour un rayon qui ne bouge jamais.
  EffectTrack gonflee = LiveTrack();
  gonflee.value[kSlotOfSpinDegrees] = 90.0F;
  const std::optional<EffectWorldQuad> gonfle =
      ComposeTintedRotatedQuad(gonflee, kMarkThatKeepsCornerOrder, kNoSubMark, 0, 0);
  REQUIRE(gonfle.has_value());
  CHECK(AreaOf(*gonfle) > AreaOf(*repos));
}

TEST_CASE("Carré teinté : la marque qui s'élève MONTE dans notre repère", "[render][figure]") {
  const EffectTrack track = LiveTrack();
  const std::optional<EffectWorldQuad> plat =
      ComposeTintedRotatedQuad(track, kOrdinaryMark, kNoSubMark, 0, 0);
  const std::optional<EffectWorldQuad> top =
      ComposeTintedRotatedQuad(track, kMarkThatRises, kNoSubMark, 0, 0);
  REQUIRE(plat.has_value());
  REQUIRE(top.has_value());
  // Le client l'écrit en RETRANCHANT cent dix, dans un repère qui descend.
  // Recopier le signe au lieu du sens enfoncerait la figure.
  CHECK(top->corners[0].y > plat->corners[0].y);
}

TEST_CASE("Carré teinté : l'ancre de piste est la SEULE origine des coins", "[render][figure]") {
  // Le client pousse UN au dix-huitième paramètre de son traceur pour toutes
  // ses marques : l'origine du nœud n'entre jamais. Déplacer l'ancre déplace
  // donc le carré d'autant, quelle que soit la marque.
  for (const std::uint8_t mark : {kOrdinaryMark, kMarkThatKeepsCornerOrder}) {
    INFO("marque " << static_cast<int>(mark));
    EffectTrack here = LiveTrack();
    EffectTrack elsewhere = LiveTrack();
    elsewhere.anchor += glm::vec3{100.0F, 0.0F, 100.0F};
    const std::optional<EffectWorldQuad> quad_here =
        ComposeTintedRotatedQuad(here, mark, kNoSubMark, 0, 0);
    const std::optional<EffectWorldQuad> quad_elsewhere =
        ComposeTintedRotatedQuad(elsewhere, mark, kNoSubMark, 0, 0);
    REQUIRE(quad_here.has_value());
    REQUIRE(quad_elsewhere.has_value());
    CHECK(quad_elsewhere->corners[0].x == Catch::Approx(quad_here->corners[0].x + 100.0F));
    CHECK(quad_elsewhere->corners[0].z == Catch::Approx(quad_here->corners[0].z + 100.0F));
  }
}

TEST_CASE("Carré teinté : le mélange vient de la MARQUE, par le seizième paramètre",
          "[render][figure]") {
  // Le dessinateur passe zéro pour la marque un, un pour la marque zéro ; le
  // traceur en fait le facteur UN — une addition — ou le mélange
  // ordinaire. Un seul mélange pour toutes les marques ajouterait le noir de la
  // marque seize, ou éteindrait le vert du soin.
  constexpr std::int32_t kFacteurUn = 2;
  constexpr std::int32_t kPlainFactor = 6;
  const EffectTrack track = LiveTrack();
  REQUIRE_FALSE(EffectTintedQuadStyleFor(kOrdinaryMark, kNoSubMark).ordinary_blend);
  REQUIRE(EffectTintedQuadStyleFor(kMarkThatKeepsCornerOrder, kNoSubMark).ordinary_blend);
  const std::optional<EffectWorldQuad> ajoute =
      ComposeTintedRotatedQuad(track, kOrdinaryMark, kNoSubMark, 0, 0);
  const std::optional<EffectWorldQuad> blend =
      ComposeTintedRotatedQuad(track, kMarkThatKeepsCornerOrder, kNoSubMark, 0, 0);
  REQUIRE(ajoute.has_value());
  REQUIRE(blend.has_value());
  CHECK(ajoute->blend_destination == kFacteurUn);
  CHECK(blend->blend_destination == kPlainFactor);
  // La marque un, bleutée, s'ajoute ; la marque seize, noire, se mélange.
  CHECK_FALSE(EffectTintedQuadStyleFor(1, kNoSubMark).ordinary_blend);
  CHECK(EffectTintedQuadStyleFor(16, kNoSubMark).ordinary_blend);
}

TEST_CASE("Carré teinté : la clé de décalage pousse le carré de côté", "[render][figure]") {
  EffectTrack nudged = LiveTrack();
  nudged.value[kSlotOfNudgeKey] = 10.0F;
  EffectTrack centre = LiveTrack();
  centre.value[kSlotOfNudgeKey] = 9.0F;
  const std::optional<EffectWorldQuad> shifted =
      ComposeTintedRotatedQuad(nudged, kMarkThatKeepsCornerOrder, kNoSubMark, 0, 0);
  const std::optional<EffectWorldQuad> straight =
      ComposeTintedRotatedQuad(centre, kMarkThatKeepsCornerOrder, kNoSubMark, 0, 0);
  REQUIRE(shifted.has_value());
  REQUIRE(straight.has_value());
  // ⚠ UNE SEULE VALEUR DÉCLENCHE, pas un seuil : neuf ne pousse pas.
  CHECK(shifted->corners[0].x > straight->corners[0].x);
}

TEST_CASE("Carré teinté : une seule marque n'emploie que la MOITIÉ de l'alpha",
          "[render][figure]") {
  const EffectTrack track = LiveTrack();
  const std::optional<EffectWorldQuad> integer =
      ComposeTintedRotatedQuad(track, kOrdinaryMark, kNoSubMark, 0, 0);
  const std::optional<EffectWorldQuad> moitie =
      ComposeTintedRotatedQuad(track, 18, kNoSubMark, 0, 0);
  REQUIRE(integer.has_value());
  REQUIRE(moitie.has_value());
  // La teinte range l'alpha en quatrième : voir `EffectWorldQuad`.
  CHECK(moitie->tint[3] * 2 == integer->tint[3]);
}

TEST_CASE("Carré teinté : une piste éteinte ou transparente ne dessine rien", "[render][figure]") {
  EffectTrack faded = LiveTrack();
  faded.active = false;
  CHECK_FALSE(ComposeTintedRotatedQuad(faded, kOrdinaryMark, kNoSubMark, 0, 0).has_value());

  EffectTrack transparente = LiveTrack();
  transparente.alpha = 0;
  CHECK_FALSE(ComposeTintedRotatedQuad(transparente, kOrdinaryMark, kNoSubMark, 0, 0).has_value());
}

#include "ro/render/effect_grid_plane.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_dark_veil.h"
#include "ro/render/effect_grid_plane_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::AdvanceParticleFrameTracks;
using ro::render::ComposeGridPlaneQuads;
using ro::render::EffectGridPlanePattern;
using ro::render::EffectGridPlanePatternsFor;
using ro::render::EffectGridPlanePatternTable;
using ro::render::EffectGridPlaneTintFor;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kGridPlaneAnchorDrop;
using ro::render::kGridPlaneStep;
using ro::render::kMarkOfGridPlaneTint;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectGridPlaneTracks;
using ro::render::SowEffectGridPlaneNode;

namespace {

constexpr std::uint32_t kPoison = 335;
// LA MARQUE QUI PULSE, et la seule.
constexpr std::uint8_t kPulsingMark = 1;
constexpr glm::vec3 kAtOrigin{0.0F, 0.0F, 0.0F};

[[nodiscard]] EffectGridPlanePattern Pattern(std::uint8_t octet) {
  return {kPoison, "Effect_Build_PoisonWhiteEmitter48", "data\\texture\\test.bmp", octet};
}

}  // namespace

TEST_CASE("la nappe fait soixante carreaux jointifs", "[render][grid_plane]") {
  EffectTrack track = SeedEffectGridPlaneTracks(Pattern(0)).front();
  track.alpha = 255;
  const std::vector<EffectWorldQuad> nappe = ComposeGridPlaneQuads(track, 0, 0, kAtOrigin);
  // ⚠ DIX CARREAUX SUR SIX : les deux boucles ne vont pas aussi loin l'une que
  // l'autre, et la nappe est donc un RECTANGLE, non un carré.
  CHECK(nappe.size() == 60);

  // ⚠ JOINTIFS : le côté d'un carreau vaut le pas des boucles. Le deuxième
  // carreau commence exactement là où le premier finit, sans fente ni
  // recouvrement.
  const glm::vec3 first = nappe[0].corners[0];
  const glm::vec3 opposite_corner = nappe[0].corners[2];
  CHECK(std::abs(opposite_corner.x - first.x) == Approx(static_cast<float>(kGridPlaneStep)));
  CHECK(nappe[1].corners[0].z == Approx(opposite_corner.z));
}

TEST_CASE("la teinte a quatre valeurs, et deux marques donnent la meme", "[render][grid_plane]") {
  const std::array<std::uint8_t, 3> white{255, 255, 255};
  CHECK(EffectGridPlaneTintFor(0) == white);
  // ⚠ TROIS PEINT BLANC COMME ZÉRO, et ce n'est pas une échelle : le client teste
  // les deux séparément et leur donne la même couleur.
  CHECK(EffectGridPlaneTintFor(3) == white);
  CHECK(EffectGridPlaneTintFor(kPulsingMark) == std::array<std::uint8_t, 3>{255, 0, 0});
  CHECK(EffectGridPlaneTintFor(2) == std::array<std::uint8_t, 3>{0, 0, 255});
  CHECK(EffectGridPlaneTintFor(9) == std::array<std::uint8_t, 3>{0, 0, 255});
}

TEST_CASE("le semis n'allume qu'une piste, eteinte", "[render][grid_plane]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectGridPlaneTracks(Pattern(0));
  CHECK(tracks[0].active);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].radius_at_anchor == Approx(static_cast<float>(kGridPlaneStep)));
  CHECK(tracks[0].mark[kMarkOfGridPlaneTint] == 0);
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    CHECK_FALSE(tracks[rang].active);
  }
  // ⚠ ÉTEINTE, ELLE NE REND RIEN : c'est le cas au premier tic, et c'est le
  // mouvement qui la fait paraître.
  CHECK(ComposeGridPlaneQuads(tracks[0], 0, 0, kAtOrigin).empty());
}

TEST_CASE("l'octet du site est la marque, et deux lecteurs la prennent", "[render][grid_plane]") {
  CHECK(SeedEffectGridPlaneTracks(Pattern(kPulsingMark)).front().mark[kMarkOfGridPlaneTint] ==
        kPulsingMark);
  // LA FIGURE Y LIT UNE COULEUR...
  EffectTrack red = SeedEffectGridPlaneTracks(Pattern(kPulsingMark)).front();
  red.alpha = 255;
  const EffectWorldQuad carreau = ComposeGridPlaneQuads(red, 0, 0, kAtOrigin).front();
  CHECK(carreau.tint[0] == 255);
  CHECK(carreau.tint[2] == 0);
}

TEST_CASE("la marque qui pulse monte deux fois plus vite et RETOMBE", "[render][grid_plane]") {
  std::array<EffectTrack, kEffectTracksPerNode> slow = SeedEffectGridPlaneTracks(Pattern(0));
  std::array<EffectTrack, kEffectTracksPerNode> living =
      SeedEffectGridPlaneTracks(Pattern(kPulsingMark));
  for (std::int32_t tick = 0; tick < 9; ++tick) {
    AdvanceParticleFrameTracks(slow, 0, 0);
    AdvanceParticleFrameTracks(living, 0, 0);
  }
  CHECK(slow[0].alpha == 45);
  CHECK(living[0].alpha == 50);

  // ⚠ LE PLAFOND EST UNE BORNE DU TEST, NON DE LA VALEUR : la lente dépasse
  // quarante-cinq d'un pas avant de s'arrêter, et finit donc à cinquante.
  for (std::int32_t tick = 0; tick < 50; ++tick) {
    AdvanceParticleFrameTracks(slow, 0, 0);
  }
  CHECK(slow[0].alpha == 50);

  // ET LA VIVE RETOMBE une fois passé le soixante-cinquième tic, ce que la lente
  // ne fait jamais.
  for (std::int32_t tick = 0; tick < 60; ++tick) {
    AdvanceParticleFrameTracks(living, 0, 0);
  }
  CHECK(living[0].alpha < 50);
  for (std::int32_t tick = 0; tick < 200; ++tick) {
    AdvanceParticleFrameTracks(slow, 0, 0);
  }
  CHECK(slow[0].alpha == 50);
}

TEST_CASE("l'origine se reboucle a un carreau, et n'avance pas d'elle-meme",
          "[render][grid_plane]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = SeedEffectGridPlaneTracks(Pattern(0));
  AdvanceParticleFrameTracks(tracks, 0, 0);
  // ⚠ RIEN NE LA FAIT CROÎTRE : le client ne fait que la ramener en deçà d'un
  // carreau. Lui prêter un pas ferait glisser la nappe, ce qu'il ne fait pas.
  CHECK(tracks[0].lift_key == Approx(0.0F));

  tracks[0].lift_key = 42.0F;
  AdvanceParticleFrameTracks(tracks, 0, 0);
  CHECK(tracks[0].lift_key == Approx(12.0F));
  AdvanceParticleFrameTracks(tracks, 0, 0);
  CHECK(tracks[0].lift_key == Approx(12.0F));
}

TEST_CASE("les deux angles de camera se posent dans les pistes", "[render][grid_plane]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  REQUIRE_FALSE(tracks.front().active);
  // ⚠ ILS ARRIVENT DÉJÀ COMPLÉTÉS : la troupe passe `EffectBillboardTurnDegrees`
  // et `…TiltDegrees`, et le mouvement les range tels quels. Cet essai attendait
  // qu'il les complète une seconde fois — la figure recevait alors l'angle brut
  // de la caméra. Voir `effect_billboard_facing_test.cpp`.
  AdvanceParticleFrameTracks(tracks, 330, 45);
  CHECK(tracks[0].style == 330);
  CHECK(tracks[1].style == 45);
}

TEST_CASE("le noeud seme descend son ancre de neuf", "[render][grid_plane]") {
  const std::span<const EffectGridPlanePattern> table = EffectGridPlanePatternTable();
  REQUIRE_FALSE(table.empty());
  CHECK(EffectGridPlanePatternsFor(kPoison).size() == 1);

  const ro::render::PooledEffectNode node =
      SowEffectGridPlaneNode(Pattern(0), glm::vec3{1.0F, 2.0F, 3.0F}, 60);
  CHECK(node.shape == PooledNodeShape::kGridPlane);
  // ⚠ NEUF DE PLUS, ET C'EST UNE DESCENTE : le repère du client descend.
  CHECK(node.ground_position.y == Approx(2.0F + kGridPlaneAnchorDrop));
  CHECK(node.owner_position.y == Approx(node.ground_position.y));
  REQUIRE(node.resource_paths.size() == 1);
}

TEST_CASE("Nappe au sol : la brume du poison se peint par-dessus tout", "[render][grid_plane]") {
  // 🔴 LE BÂTISSEUR LÈVE LE BIT 0x200 : sans épreuve de profondeur, le voile ne
  // se fait plus couper par le sol ni le décor, et reste collé à l'image.
  const auto patterns = EffectGridPlanePatternsFor(335);
  REQUIRE_FALSE(patterns.empty());
  const ro::render::PooledEffectNode pooled =
      ro::render::SowEffectGridPlaneNode(patterns.front(), {0.0F, 0.0F, 0.0F}, 100);
  REQUIRE(pooled.node.render_word.has_value());
  CHECK(*pooled.node.render_word == 0x201U);
  CHECK_FALSE(ro::render::EffectLotIsDepthTested(pooled.node.render_word));
}

TEST_CASE("Nappe au sol : ce client peint la brume à l'écran, et elle le couvre tout entier",
          "[render][grid_plane]") {
  // 🔴 POSÉE DANS LE MONDE, ELLE SUIVAIT LE ZOOM et le sol la coupait : elle se
  // lisait comme une dalle couchée. À l'écran, ses carreaux gardent la taille du
  // zoom par défaut et se prolongent jusqu'aux bords.
  const auto patterns = EffectGridPlanePatternsFor(335);
  REQUIRE_FALSE(patterns.empty());
  EffectTrack track = ro::render::SeedEffectGridPlaneTracks(patterns.front())[0];
  track.alpha = 120;
  constexpr float kWidth = 2560.0F;
  constexpr float kHeight = 1080.0F;
  const glm::vec2 bearer{700.0F, 400.0F};
  const std::vector<ro::render::EffectScreenQuad> quads =
      ro::render::ComposeScreenGridPlaneQuads(track, bearer, kWidth, kHeight);
  REQUIRE_FALSE(quads.empty());
  glm::vec2 low{kWidth, kHeight};
  glm::vec2 high{0.0F, 0.0F};
  const float side = ro::render::DarkVeilScreenRadiusPixels(track.radius_at_anchor, kHeight);
  for (const ro::render::EffectScreenQuad& quad : quads) {
    CHECK(quad.corners[2].x - quad.corners[0].x == Approx(side));
    CHECK(quad.tint[3] == 120);
    low = glm::min(low, quad.corners[0]);
    high = glm::max(high, quad.corners[2]);
  }
  CHECK(low.x <= 0.0F);
  CHECK(low.y <= 0.0F);
  CHECK(high.x >= kWidth);
  CHECK(high.y >= kHeight);
  // Le carrelage se cale sur le porteur : une arête passe à cent cinquante
  // unités à sa gauche, comme le bord de la nappe du jeu.
  const float edge = bearer.x - 150.0F * side / track.radius_at_anchor;
  const bool aligned = std::ranges::any_of(quads, [edge](const ro::render::EffectScreenQuad& q) {
    return std::abs(q.corners[0].x - edge) < 0.01F;
  });
  CHECK(aligned);
  // ⚠ SANS ALPHA, RIEN : l'alpha naît nul et le mouvement le monte.
  track.alpha = 0;
  CHECK(ro::render::ComposeScreenGridPlaneQuads(track, bearer, kWidth, kHeight).empty());
}

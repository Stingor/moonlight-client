#include "ro/render/effect_frame_and_bar.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_frame_and_bar_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceFrameAndBarTracks;
using ro::render::ComposeFrameWallQuads;
using ro::render::ComposePerimeterBarQuads;
using ro::render::EffectFrameAndBarPattern;
using ro::render::EffectFrameAndBarPatternsFor;
using ro::render::EffectFrameAndBarPatternTable;
using ro::render::EffectFrameAndBarRecipe;
using ro::render::EffectFrameAndBarRecipeOf;
using ro::render::EffectPerimeterPointOnSide;
using ro::render::EffectPerimeterSideOf;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kBarBlue;
using ro::render::kBarGreen;
using ro::render::kEffectTracksPerNode;
using ro::render::kEffectWallFloorHeight;
using ro::render::kFrameAndBarBarAlpha;
using ro::render::kFrameAndBarFrameTracks;
using ro::render::kFrameAndBarSlotOfBarLength;
using ro::render::kFrameAndBarSlotOfHalfX;
using ro::render::kFrameAndBarSlotOfHalfZ;
using ro::render::kFrameAndBarSlotOfHeight;
using ro::render::kFrameAndBarSlotOfStyle;
using ro::render::kFrameAndBarWalls;
using ro::render::kFrameBlue;
using ro::render::kFrameGreen;
using ro::render::SeedEffectFrameAndBarTracks;
using ro::render::SowEffectFrameAndBarNode;

namespace {

constexpr float kHalfSide = 10.0F;
constexpr float kHeight = 3.0F;

[[nodiscard]] EffectTrack Frame(float style = 0.0F) {
  EffectTrack track;
  track.active = true;
  track.alpha = 65;
  track.value[kFrameAndBarSlotOfHalfX] = kHalfSide;
  track.value[kFrameAndBarSlotOfHalfZ] = kHalfSide;
  track.value[kFrameAndBarSlotOfHeight] = kHeight;
  track.value[kFrameAndBarSlotOfStyle] = style;
  return track;
}

[[nodiscard]] EffectTrack Barre(float start, float length, float style = 0.0F) {
  EffectTrack track = Frame(style);
  track.alpha = kFrameAndBarBarAlpha;
  track.radius_at_anchor = start;
  track.value[kFrameAndBarSlotOfBarLength] = length;
  return track;
}

[[nodiscard]] const EffectFrameAndBarPattern& PatternOf(std::uint32_t effect_id,
                                                        std::uint8_t octet) {
  const EffectFrameAndBarPattern* found = nullptr;
  for (const EffectFrameAndBarPattern& pattern : EffectFrameAndBarPatternsFor(effect_id)) {
    if (pattern.style_byte == octet) {
      found = &pattern;
    }
  }
  REQUIRE(found != nullptr);
  return *found;
}

}  // namespace

TEST_CASE("le cadre dresse quatre murs qui se referment", "[render][frame_and_bar]") {
  const std::vector<EffectWorldQuad> quads = ComposeFrameWallQuads(Frame(), {0.0F, 0.0F, 0.0F});
  REQUIRE(quads.size() == kFrameAndBarWalls);
  for (std::size_t wall = 0; wall < kFrameAndBarWalls; ++wall) {
    const EffectWorldQuad& current = quads[wall];
    const EffectWorldQuad& next = quads[(wall + 1) % kFrameAndBarWalls];
    // Le pied lointain d'un mur est le pied proche du suivant.
    CHECK(current.corners[1].x == Approx(next.corners[0].x));
    CHECK(current.corners[1].z == Approx(next.corners[0].z));
  }
}

TEST_CASE("le mur part UNE unite au-dessus du noeud", "[render][frame_and_bar]") {
  // ⚠ LE CLIENT ÉCRIT MOINS UN ET SON REPÈRE DESCEND : le pied est donc
  // au-dessus du nœud, pas au-dessous.
  const std::vector<EffectWorldQuad> quads = ComposeFrameWallQuads(Frame(), {2.0F, 5.0F, 3.0F});
  CHECK(quads.front().corners[0].y == Approx(5.0F + kEffectWallFloorHeight));
  CHECK(quads.front().corners[3].y == Approx(5.0F + kEffectWallFloorHeight + kHeight));
  // ⚠ ET LES COINS SONT RELATIFS : l'origine du nœud entre dans les trois axes.
  // La profondeur du coin, écrite par le client, se nie chez nous.
  CHECK(quads.front().corners[0].x == Approx(2.0F + kHalfSide));
  CHECK(quads.front().corners[0].z == Approx(3.0F - kHalfSide));
}

TEST_CASE("le cadre et la barre virent au bleu ensemble", "[render][frame_and_bar]") {
  const std::vector<EffectWorldQuad> vert = ComposeFrameWallQuads(Frame(0.0F), {});
  CHECK(vert.front().tint[0] == kFrameGreen[0]);
  CHECK(vert.front().tint[1] == kFrameGreen[1]);
  const std::vector<EffectWorldQuad> blue = ComposeFrameWallQuads(Frame(10.0F), {});
  CHECK(blue.front().tint[0] == kFrameBlue[0]);
  CHECK(blue.front().tint[2] == kFrameBlue[2]);

  const std::vector<EffectWorldQuad> barre_verte = ComposePerimeterBarQuads(Barre(0.0F, 5.0F), {});
  CHECK(barre_verte.front().tint[0] == kBarGreen[0]);
  const std::vector<EffectWorldQuad> barre_bleue =
      ComposePerimeterBarQuads(Barre(0.0F, 5.0F, 10.0F), {});
  CHECK(barre_bleue.front().tint[0] == kBarBlue[0]);
}

TEST_CASE("une piste eteinte ou muette ne rend rien", "[render][frame_and_bar]") {
  EffectTrack faded = Frame();
  faded.active = false;
  CHECK(ComposeFrameWallQuads(faded, {}).empty());
  CHECK(ComposePerimeterBarQuads(faded, {}).empty());
  EffectTrack silent = Frame();
  silent.alpha = 0;
  CHECK(ComposeFrameWallQuads(silent, {}).empty());
  CHECK(ComposePerimeterBarQuads(silent, {}).empty());
}

TEST_CASE("le tour du perimetre se recolle a chaque coin", "[render][frame_and_bar]") {
  const float cote = kHalfSide + kHalfSide;
  for (std::size_t face = 0; face < kFrameAndBarWalls; ++face) {
    const float at_corner = static_cast<float>(face + 1) * cote;
    const glm::vec2 fin = EffectPerimeterPointOnSide(kHalfSide, face, at_corner);
    const glm::vec2 begin =
        EffectPerimeterPointOnSide(kHalfSide, (face + 1) % kFrameAndBarWalls,
                                   face + 1 == kFrameAndBarWalls ? 0.0F : at_corner);
    CHECK(fin.x == Approx(begin.x));
    CHECK(fin.y == Approx(begin.y));
  }
  CHECK(EffectPerimeterSideOf(kHalfSide, 0.0F) == 0);
  CHECK(EffectPerimeterSideOf(kHalfSide, 25.0F) == 1);
  CHECK(EffectPerimeterSideOf(kHalfSide, 45.0F) == 2);
  CHECK(EffectPerimeterSideOf(kHalfSide, 65.0F) == 3);
}

TEST_CASE("la barre tient en un quad tant qu'elle reste sur son cote", "[render][frame_and_bar]") {
  const std::vector<EffectWorldQuad> quads = ComposePerimeterBarQuads(Barre(5.0F, 10.0F), {});
  REQUIRE(quads.size() == 1);
  // ⚠ LE TOUR SE PARCOURT DANS LE REPÈRE DU CLIENT : chaque profondeur qu'il
  // écrit se nie chez nous, sans quoi la barre courrait à rebours.
  CHECK(quads.front().corners[0].x == Approx(kHalfSide));
  CHECK(quads.front().corners[0].z == Approx(-(kHalfSide - 5.0F)));
  CHECK(quads.front().corners[1].z == Approx(-(kHalfSide - 15.0F)));
}

TEST_CASE("la barre se coupe au coin et repart de l'autre cote", "[render][frame_and_bar]") {
  const std::vector<EffectWorldQuad> quads = ComposePerimeterBarQuads(Barre(15.0F, 10.0F), {});
  REQUIRE(quads.size() == 2);
  // Le premier morceau s'arrête PILE au coin.
  CHECK(quads[0].corners[1].x == Approx(kHalfSide));
  CHECK(quads[0].corners[1].z == Approx(kHalfSide));
  // Le second en repart, et file sur le côté suivant.
  CHECK(quads[1].corners[0].x == Approx(kHalfSide));
  CHECK(quads[1].corners[0].z == Approx(kHalfSide));
  CHECK(quads[1].corners[1].x == Approx(5.0F));
  CHECK(quads[1].corners[1].z == Approx(kHalfSide));
}

TEST_CASE("le second morceau n'est PAS coupe au coin suivant", "[render][frame_and_bar]") {
  // ⚠ UNE BARRE PLUS LONGUE QU'UN CÔTÉ DÉPASSE EN LIGNE DROITE, et c'est le
  // client qui le fait : il ne replie qu'une fois. Ajouter une seconde coupure
  // le corrigerait au lieu de le porter.
  const std::vector<EffectWorldQuad> quads = ComposePerimeterBarQuads(Barre(15.0F, 30.0F), {});
  REQUIRE(quads.size() == 2);
  CHECK(quads[1].corners[1].x == Approx(-15.0F));
  CHECK(quads[1].corners[1].x < -kHalfSide);
}

TEST_CASE("la barre du dernier cote repasse par le depart du tour", "[render][frame_and_bar]") {
  // Départ à soixante-quinze sur un tour de quatre-vingts : la barre franchit
  // le dernier coin et revient sur le premier côté.
  const std::vector<EffectWorldQuad> quads = ComposePerimeterBarQuads(Barre(75.0F, 10.0F), {});
  REQUIRE(quads.size() == 2);
  CHECK(quads[1].corners[0].x == Approx(kHalfSide));
  CHECK(quads[1].corners[0].z == Approx(-kHalfSide));
  CHECK(quads[1].corners[1].x == Approx(kHalfSide));
  CHECK(quads[1].corners[1].z == Approx(-(kHalfSide - 5.0F)));
}

TEST_CASE("la hauteur monte en quart de sinus, tient, puis retombe", "[render][frame_and_bar]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Frame();
  tracks[0].lift_key = 4.0F;
  tracks[0].style = 200;
  tracks[0].ticks = 0;
  tracks[0].value[kFrameAndBarSlotOfHeight] = 0.0F;

  AdvanceFrameAndBarTracks(tracks, {}, {});
  const float un_degre = std::sin(std::numbers::pi_v<float> / 180.0F) * 4.0F;
  CHECK(tracks[0].value[kFrameAndBarSlotOfHeight] == Approx(un_degre));
  for (std::int32_t tick = 1; tick < 90; ++tick) {
    AdvanceFrameAndBarTracks(tracks, {}, {});
  }
  CHECK(tracks[0].ticks == 90);
  CHECK(tracks[0].value[kFrameAndBarSlotOfHeight] == Approx(4.0F));
  // Le plateau tient jusqu'à la durée.
  for (std::int32_t tick = 90; tick < 200; ++tick) {
    AdvanceFrameAndBarTracks(tracks, {}, {});
  }
  CHECK(tracks[0].value[kFrameAndBarSlotOfHeight] == Approx(4.0F));
  // ⚠ ET LA RETOMBÉE DU STYLE ZÉRO NE SE COMPTE PAS DEPUIS LA DURÉE : le
  // client la compte depuis un instant FIXE, 2 505, si bien qu'une durée plus
  // courte fait disparaître le mur d'un coup.
  AdvanceFrameAndBarTracks(tracks, {}, {});
  CHECK(tracks[0].ticks == 201);
  CHECK(tracks[0].value[kFrameAndBarSlotOfHeight] == Approx(4.0F));
}

TEST_CASE("seules les deux dernieres pistes font courir leur barre", "[render][frame_and_bar]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    tracks[rang] = Frame();
    tracks[rang].style = 500;
    tracks[rang].radius_at_anchor = 0.0F;
  }
  AdvanceFrameAndBarTracks(tracks, {}, {});
  AdvanceFrameAndBarTracks(tracks, {}, {});
  CHECK(tracks[0].radius_at_anchor == Approx(0.0F));
  CHECK(tracks[1].radius_at_anchor == Approx(0.0F));
  CHECK(tracks[2].radius_at_anchor == Approx(1.0F));
  CHECK(tracks[3].radius_at_anchor == Approx(1.0F));

  // Et le tour se replie sur huit demi-côtés.
  tracks[2].radius_at_anchor = 79.8F;
  AdvanceFrameAndBarTracks(tracks, {}, {});
  CHECK(tracks[2].radius_at_anchor == Approx(0.3F));
}

TEST_CASE("le porteur qui bouge coupe la marque, mais pas n'importe laquelle",
          "[render][frame_and_bar]") {
  std::array<EffectTrack, kEffectTracksPerNode> surveillee;
  surveillee[0] = Frame(20.0F);
  surveillee[0].style = 2170;
  surveillee[0].ticks = 250;
  AdvanceFrameAndBarTracks(surveillee, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F});
  CHECK(surveillee[0].style == 251);

  // ⚠ AVANT LE DEUX-CENTIÈME TIC, le client ne regarde pas.
  std::array<EffectTrack, kEffectTracksPerNode> jeune;
  jeune[0] = Frame(20.0F);
  jeune[0].style = 2170;
  jeune[0].ticks = 100;
  AdvanceFrameAndBarTracks(jeune, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F});
  CHECK(jeune[0].style == 2170);

  // ⚠ ET LES STYLES SOUS VINGT NE SONT PAS SURVEILLÉS DU TOUT.
  std::array<EffectTrack, kEffectTracksPerNode> free_tracks;
  free_tracks[0] = Frame(10.0F);
  free_tracks[0].style = 2170;
  free_tracks[0].ticks = 250;
  AdvanceFrameAndBarTracks(free_tracks, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F});
  CHECK(free_tracks[0].style == 2170);
}

TEST_CASE("la table porte DEUX lignes par identifiant", "[render][frame_and_bar]") {
  const std::span<const EffectFrameAndBarPattern> table = EffectFrameAndBarPatternTable();
  CHECK(table.size() == 10);
  CHECK(std::ranges::is_sorted(table, {}, &EffectFrameAndBarPattern::effect_id));
  for (const std::uint32_t effect_id : {374U, 715U, 716U, 802U, 2401U}) {
    CHECK(EffectFrameAndBarPatternsFor(effect_id).size() == 2);
  }
  // ⚠ LES DEUX OCTETS SONT CONSÉCUTIFS, et c'est ce qui emboîte les deux nœuds.
  CHECK(PatternOf(374, 0).style_byte == 0);
  CHECK(PatternOf(374, 1).style_byte == 1);
  CHECK(PatternOf(715, 10).texture == PatternOf(716, 10).texture);
  CHECK(PatternOf(802, 20).style_byte == 20);
  CHECK(EffectFrameAndBarPatternsFor(1).empty());
}

TEST_CASE("l'octet du site porte sept nombres", "[render][frame_and_bar]") {
  const EffectFrameAndBarRecipe top = EffectFrameAndBarRecipeOf(0);
  const EffectFrameAndBarRecipe bottom = EffectFrameAndBarRecipeOf(1);
  CHECK(top.lift_floor == Approx(30.0F));
  CHECK(bottom.lift_floor == Approx(20.0F));
  CHECK(top.frame_alpha == 65);
  CHECK(bottom.frame_alpha == 32);
  CHECK(top.frame_half_side == Approx(12.5F));
  CHECK(bottom.frame_half_side == Approx(14.0F));
  CHECK(top.bar_half_side == Approx(13.3F));
  CHECK(bottom.bar_half_side == Approx(14.8F));
  CHECK(top.bar_start_floor == Approx(0.0F));
  CHECK(bottom.bar_start_floor == Approx(26.6F));

  // ⚠ SEULE LA DIZAINE TIRE SA DURÉE DE LA VIE DE L'EFFET.
  CHECK(EffectFrameAndBarRecipeOf(10).duration_from_life);
  CHECK(EffectFrameAndBarRecipeOf(11).duration_from_life);
  CHECK_FALSE(EffectFrameAndBarRecipeOf(0).duration_from_life);
  CHECK_FALSE(EffectFrameAndBarRecipeOf(20).duration_from_life);
  CHECK_FALSE(EffectFrameAndBarRecipeOf(21).duration_from_life);

  // ⚠ ET LA BARRE NE PARTAGE PAS LA DURÉE DU CADRE pour deux octets sur cinq.
  CHECK(EffectFrameAndBarRecipeOf(21).frame_duration == 2140);
  CHECK(EffectFrameAndBarRecipeOf(21).bar_duration == 2170);
  CHECK(EffectFrameAndBarRecipeOf(0).frame_duration == EffectFrameAndBarRecipeOf(0).bar_duration);
}

TEST_CASE("le semis pose deux pistes de cadre et deux de barre", "[render][frame_and_bar]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectFrameAndBarTracks(PatternOf(374, 0), 1000);
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.ticks == 0);
    CHECK(track.value[kFrameAndBarSlotOfStyle] == Approx(0.0F));
    CHECK(track.value[kFrameAndBarSlotOfHalfZ] == Approx(track.value[kFrameAndBarSlotOfHalfX]));
    // ⚠ LA HAUTEUR NAÎT NULLE : c'est le mouvement qui dresse les murs.
    CHECK(track.value[kFrameAndBarSlotOfHeight] == Approx(0.0F));
  }
  // Les deux cadres sont emboîtés, et le second est plus haut.
  CHECK(tracks[0].value[kFrameAndBarSlotOfHalfX] == Approx(12.5F));
  CHECK(tracks[1].value[kFrameAndBarSlotOfHalfX] == Approx(12.9F));
  CHECK(tracks[0].lift_key == Approx(30.0F));
  CHECK(tracks[1].lift_key == Approx(31.0F));
  CHECK(tracks[0].alpha == 65);
  // Les deux barres courent sur le MÊME carré, à des départs opposés.
  CHECK(tracks[2].value[kFrameAndBarSlotOfHalfX] == Approx(13.3F));
  CHECK(tracks[3].value[kFrameAndBarSlotOfHalfX] == Approx(13.3F));
  CHECK(tracks[2].radius_at_anchor == Approx(0.0F));
  CHECK(tracks[3].radius_at_anchor == Approx(53.2F));
  CHECK(tracks[2].alpha == kFrameAndBarBarAlpha);
  CHECK(tracks[2].value[kFrameAndBarSlotOfBarLength] == Approx(26.6F));
}

TEST_CASE("la duree de la dizaine vient de la vie de l'effet", "[render][frame_and_bar]") {
  const std::array<EffectTrack, kEffectTracksPerNode> brief =
      SeedEffectFrameAndBarTracks(PatternOf(715, 10), 1000);
  CHECK(brief[0].style == 910);
  CHECK(brief[2].style == 910);
  // ⚠ LE CLIENT BORNE LA VIE AVANT D'EN RETRANCHER SA MARGE.
  const std::array<EffectTrack, kEffectTracksPerNode> eternelle =
      SeedEffectFrameAndBarTracks(PatternOf(715, 10), 31000);
  CHECK(eternelle[0].style == 30000);
  // Et les autres octets gardent la durée écrite en dur.
  const std::array<EffectTrack, kEffectTracksPerNode> fixed =
      SeedEffectFrameAndBarTracks(PatternOf(374, 0), 1000);
  CHECK(fixed[0].style == 2415);
}

TEST_CASE("le noeud seme porte sa forme et son image", "[render][frame_and_bar]") {
  const ro::render::PooledEffectNode node =
      SowEffectFrameAndBarNode(PatternOf(802, 21), {4.0F, 0.0F, 6.0F}, 500);
  CHECK(node.shape == ro::render::PooledNodeShape::kFrameAndBar);
  CHECK(node.node.life_ticks == 500);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(PatternOf(802, 21).texture));
  CHECK(node.tracks[kFrameAndBarFrameTracks].value[kFrameAndBarSlotOfStyle] == Approx(21.0F));
}

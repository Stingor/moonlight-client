#include "ro/render/effect_sheathed_bolt.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_sheathed_bolt_seed.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceSheathedBoltTracks;
using ro::render::ComposeSheathedBoltQuads;
using ro::render::EffectSheathedBoltBuilder;
using ro::render::EffectSheathedBoltColor;
using ro::render::EffectSheathedBoltPattern;
using ro::render::EffectSheathedBoltPatternsFor;
using ro::render::EffectSheathedBoltPatternTable;
using ro::render::EffectSheathedBoltRadius;
using ro::render::EffectSheathedBoltRecipe;
using ro::render::EffectSheathedBoltRecipeOf;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kSheathedBoltBlue;
using ro::render::kSheathedBoltBlueStyle;
using ro::render::kSheathedBoltCoreDefault;
using ro::render::kSheathedBoltDim;
using ro::render::kSheathedBoltDimStyle;
using ro::render::kSheathedBoltPasses;
using ro::render::kSheathedBoltPink;
using ro::render::kSheathedBoltPinkStyle;
using ro::render::kSheathedBoltSheathDefault;
using ro::render::kSheathedBoltSheathGrowth;
using ro::render::kSheathedBoltSides;
using ro::render::kSheathedBoltSlotOfFarX;
using ro::render::kSheathedBoltSlotOfFarY;
using ro::render::kSheathedBoltSlotOfFarZ;
using ro::render::kSheathedBoltSlotOfLift;
using ro::render::kSheathedBoltSlotOfStyle;
using ro::render::kSheathedBoltStepDegrees;
using ro::render::kSheathedBoltUnnamedStyle;
using ro::render::kSheathedBoltWhite;
using ro::render::kSheathedBoltWhiteStyle;
using ro::render::NextEffectNodeDraw;
using ro::render::SeedEffectSheathedBoltTracks;
using ro::render::SowEffectSheathedBoltNode;

namespace {

constexpr float kCoreRadius = 0.5F;
constexpr float kHeight = 8.0F;

// UNE PISTE DÉJÀ DÉPLOYÉE : ancre à l'origine, bout lointain dix unités plus
// loin sur l'axe des x, ce qui correspond à un azimut de zéro.
[[nodiscard]] EffectTrack Deployed(float style = kSheathedBoltWhiteStyle) {
  EffectTrack track;
  track.active = true;
  track.alpha = 100;
  track.ticks = 3;
  track.azimuth_degrees = 0;
  track.lift_key = kCoreRadius;
  track.radius_at_anchor = 10.0F;
  track.anchor = {0.0F, 0.0F, 0.0F};
  track.value[kSheathedBoltSlotOfLift] = kHeight;
  track.value[kSheathedBoltSlotOfStyle] = style;
  track.tail[kSheathedBoltSlotOfFarX] = 10.0F;
  track.tail[kSheathedBoltSlotOfFarY] = 0.0F;
  track.tail[kSheathedBoltSlotOfFarZ] = 0.0F;
  return track;
}

[[nodiscard]] const EffectSheathedBoltPattern& PatternOf(std::uint32_t effect_id) {
  const std::span<const EffectSheathedBoltPattern> patterns =
      EffectSheathedBoltPatternsFor(effect_id);
  REQUIRE(patterns.size() == 1);
  return patterns.front();
}

// COMBIEN DE TIRAGES un générateur a consommés depuis une graine donnée.
[[nodiscard]] std::uint32_t AfterDraws(std::uint32_t seed, std::size_t count) {
  for (std::size_t step = 0; step < count; ++step) {
    static_cast<void>(NextEffectNodeDraw(seed));
  }
  return seed;
}

}  // namespace

TEST_CASE("le trait gaine rend cinq quads par passe", "[render][sheathed_bolt]") {
  const std::vector<EffectWorldQuad> quads = ComposeSheathedBoltQuads(Deployed());
  CHECK(quads.size() == kSheathedBoltPasses * kSheathedBoltSides);
}

TEST_CASE("une piste eteinte ou transparente ne rend rien", "[render][sheathed_bolt]") {
  EffectTrack faded = Deployed();
  faded.active = false;
  CHECK(ComposeSheathedBoltQuads(faded).empty());

  EffectTrack invisible = Deployed();
  invisible.alpha = 0;
  CHECK(ComposeSheathedBoltQuads(invisible).empty());
}

TEST_CASE("le pentagone se referme sur lui-meme", "[render][sheathed_bolt]") {
  const std::vector<EffectWorldQuad> quads = ComposeSheathedBoltQuads(Deployed());
  // ⚠ CINQ CÔTÉS POUR SIX SOMMETS : le sixième retombe sur le premier, et
  // c'est pourquoi la figure n'a pas de sixième quad d'aire nulle.
  const glm::vec3 first = quads.front().corners[0];
  const glm::vec3 last = quads[kSheathedBoltSides - 1].corners[1];
  CHECK(last.x == Approx(first.x).margin(1e-4));
  CHECK(last.y == Approx(first.y).margin(1e-4));
  CHECK(last.z == Approx(first.z).margin(1e-4));
}

TEST_CASE("le cercle est debout et perpendiculaire au trait", "[render][sheathed_bolt]") {
  // Azimut zéro : le plan du cercle prend le quart de tour, donc l'axe des z.
  // Aucun sommet ne s'écarte du plan en x, et le trait garde toute sa longueur.
  const std::vector<EffectWorldQuad> quads = ComposeSheathedBoltQuads(Deployed());
  for (const EffectWorldQuad& quad : quads) {
    CHECK(quad.corners[0].x == Approx(0.0F).margin(1e-4));
    CHECK(quad.corners[1].x == Approx(0.0F).margin(1e-4));
    CHECK(quad.corners[2].x == Approx(10.0F).margin(1e-4));
    CHECK(quad.corners[3].x == Approx(10.0F).margin(1e-4));
  }
}

TEST_CASE("le trait monte de la hauteur que la piste porte", "[render][sheathed_bolt]") {
  // Le premier sommet est à l'angle zéro : son sinus est nul, si bien que sa
  // hauteur est exactement celle du centre du cercle.
  const std::vector<EffectWorldQuad> quads = ComposeSheathedBoltQuads(Deployed());
  CHECK(quads.front().corners[0].y == Approx(kHeight));
  // Et le cercle s'étend de part et d'autre, du rayon de l'âme.
  float highest = quads.front().corners[0].y;
  float lowest = quads.front().corners[0].y;
  for (std::size_t cote = 0; cote < kSheathedBoltSides; ++cote) {
    highest = std::max(highest, quads[cote].corners[1].y);
    lowest = std::min(lowest, quads[cote].corners[1].y);
  }
  const float half_height = kCoreRadius * std::sin(static_cast<float>(kSheathedBoltStepDegrees) *
                                                   std::numbers::pi_v<float> / 180.0F);
  CHECK(highest == Approx(kHeight + half_height));
  CHECK(lowest == Approx(kHeight - half_height));
}

TEST_CASE("la gaine deborde l'ame, et double pour le style rose", "[render][sheathed_bolt]") {
  CHECK(EffectSheathedBoltRadius(kSheathedBoltWhiteStyle, 1.0F, false) == Approx(1.0F));
  CHECK(EffectSheathedBoltRadius(kSheathedBoltWhiteStyle, 1.0F, true) ==
        Approx(1.0F + kSheathedBoltSheathGrowth));
  // ⚠ LE STYLE ROSE EST LE SEUL À DOUBLER : lui donner le débordement commun
  // rétrécirait sa gaine de moitié dès que l'âme passe le quart d'unité.
  CHECK(EffectSheathedBoltRadius(kSheathedBoltPinkStyle, 1.0F, true) == Approx(2.0F));
}

TEST_CASE("les deux passes ne different de teinte que par defaut", "[render][sheathed_bolt]") {
  for (const float style : {kSheathedBoltWhiteStyle, kSheathedBoltPinkStyle, kSheathedBoltBlueStyle,
                            kSheathedBoltDimStyle}) {
    CHECK(EffectSheathedBoltColor(style, false) == EffectSheathedBoltColor(style, true));
  }
  CHECK(EffectSheathedBoltColor(kSheathedBoltWhiteStyle, false) == kSheathedBoltWhite);
  CHECK(EffectSheathedBoltColor(kSheathedBoltPinkStyle, false) == kSheathedBoltPink);
  CHECK(EffectSheathedBoltColor(kSheathedBoltBlueStyle, false) == kSheathedBoltBlue);
  CHECK(EffectSheathedBoltColor(kSheathedBoltDimStyle, false) == kSheathedBoltDim);
  // Un style inconnu, et lui seul, change de teinte d'une passe à l'autre.
  constexpr float kUnknownStyle = 1.0F;
  CHECK(EffectSheathedBoltColor(kUnknownStyle, false) == kSheathedBoltCoreDefault);
  CHECK(EffectSheathedBoltColor(kUnknownStyle, true) == kSheathedBoltSheathDefault);
}

TEST_CASE("l'ame se melange par l'alpha et la gaine s'ajoute", "[render][sheathed_bolt]") {
  const std::vector<EffectWorldQuad> quads = ComposeSheathedBoltQuads(Deployed());
  // Le facteur de destination de Direct3D : six pour le mélange ordinaire,
  // deux pour l'addition.
  CHECK(quads.front().blend_destination == 6);
  CHECK(quads.back().blend_destination == 2);
  // ⚠ ET L'IMAGE EST RETOURNÉE : le v vaut un aux deux premiers coins.
  CHECK(quads.front().texture_uv[0].y == Approx(1.0F));
  CHECK(quads.front().texture_uv[2].y == Approx(0.0F));
}

TEST_CASE("le style sombre monte vingt tics puis descend de deux", "[render][sheathed_bolt]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Deployed(kSheathedBoltDimStyle);
  tracks[0].alpha = 0;
  tracks[0].ticks = 0;
  for (std::int32_t tick = 0; tick < 20; ++tick) {
    AdvanceSheathedBoltTracks(tracks);
  }
  CHECK(tracks[0].ticks == 20);
  CHECK(tracks[0].alpha == 160);
  AdvanceSheathedBoltTracks(tracks);
  CHECK(tracks[0].alpha == 158);
}

TEST_CASE("le style blanc monte cinq tics puis descend d'un", "[render][sheathed_bolt]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Deployed(kSheathedBoltWhiteStyle);
  tracks[0].alpha = 0;
  tracks[0].ticks = 0;
  for (std::int32_t tick = 0; tick < 5; ++tick) {
    AdvanceSheathedBoltTracks(tracks);
  }
  CHECK(tracks[0].alpha == 10);
  AdvanceSheathedBoltTracks(tracks);
  CHECK(tracks[0].alpha == 9);
}

TEST_CASE("tout autre style monte de huit puis descend de quatre", "[render][sheathed_bolt]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Deployed(kSheathedBoltBlueStyle);
  tracks[0].alpha = 0;
  tracks[0].ticks = 0;
  for (std::int32_t tick = 0; tick < 5; ++tick) {
    AdvanceSheathedBoltTracks(tracks);
  }
  CHECK(tracks[0].alpha == 40);
  AdvanceSheathedBoltTracks(tracks);
  CHECK(tracks[0].alpha == 36);
  // Et l'alpha ne descend jamais sous zéro.
  for (std::int32_t tick = 0; tick < 20; ++tick) {
    AdvanceSheathedBoltTracks(tracks);
  }
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("un retard negatif fait attendre la piste", "[render][sheathed_bolt]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Deployed();
  tracks[0].alpha = 0;
  tracks[0].ticks = -3;
  tracks[0].tail[kSheathedBoltSlotOfFarX] = 0.0F;
  AdvanceSheathedBoltTracks(tracks);
  AdvanceSheathedBoltTracks(tracks);
  AdvanceSheathedBoltTracks(tracks);
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].tail[kSheathedBoltSlotOfFarX] == Approx(0.0F));
}

TEST_CASE("le mouvement pose le bout lointain, et il ne bouge plus", "[render][sheathed_bolt]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Deployed();
  tracks[0].ticks = 0;
  tracks[0].azimuth_degrees = 90;
  tracks[0].anchor = {2.0F, 5.0F, 3.0F};
  tracks[0].radius_at_anchor = 12.0F;
  tracks[0].tail = {};
  AdvanceSheathedBoltTracks(tracks);
  CHECK(tracks[0].tail[kSheathedBoltSlotOfFarX] == Approx(2.0F).margin(1e-4));
  CHECK(tracks[0].tail[kSheathedBoltSlotOfFarZ] == Approx(15.0F).margin(1e-4));
  // ⚠ LA HAUTEUR DU BOUT N'EST PAS ÉCRITE : le mouvement ne touche que deux
  // cases sur trois, et celle du milieu reste ce que le semis y a posé.
  CHECK(tracks[0].tail[kSheathedBoltSlotOfFarY] == Approx(0.0F));
  const float before = tracks[0].tail[kSheathedBoltSlotOfFarX];
  AdvanceSheathedBoltTracks(tracks);
  AdvanceSheathedBoltTracks(tracks);
  CHECK(tracks[0].tail[kSheathedBoltSlotOfFarX] == Approx(before));
}

TEST_CASE("la table porte les cinq identifiants du sous-type 69", "[render][sheathed_bolt]") {
  const std::span<const EffectSheathedBoltPattern> table = EffectSheathedBoltPatternTable();
  CHECK(table.size() == 5);
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id < table[rang].effect_id);
  }
  // ⚠ LE PREMIER NE VIENT PAS DU MÊME RÉPARTITEUR : 329 est un cas des
  // ÉMETTEURS, les quatre autres des effets. La famille se lit au sous-type, pas
  // à la plage d'où l'effet part.
  CHECK(PatternOf(329).builder == EffectSheathedBoltBuilder::kImpactYellow);
  CHECK(PatternOf(388).builder == EffectSheathedBoltBuilder::kAimedWhite);
  CHECK(PatternOf(393).builder == EffectSheathedBoltBuilder::kAimedPink);
  CHECK(PatternOf(892).builder == EffectSheathedBoltBuilder::kImpactBlue);
  CHECK(PatternOf(1030).builder == EffectSheathedBoltBuilder::kImpactDim);
  // ⚠ L'HORAIRE EST UNE BORNE, et les cinq ne l'ont pas la même : neuf nœuds
  // pour le rose, deux pour les trois impacts.
  CHECK(PatternOf(329).schedule.last_tick == 1);
  CHECK(PatternOf(388).schedule.last_tick == 3);
  CHECK(PatternOf(393).schedule.last_tick == 8);
  CHECK(PatternOf(892).schedule.last_tick == 1);
  CHECK(PatternOf(1030).schedule.last_tick == 1);
  CHECK(EffectSheathedBoltPatternsFor(1).empty());
}

TEST_CASE("le cinquieme trait est le bleu au style pres", "[render][sheathed_bolt]") {
  // ⚠ CE N'EST PAS UNE RESSEMBLANCE, C'EST UNE MESURE. Les deux bâtisseurs
  // écrivent les mêmes tirages dans le même ordre, et le style les sépare
  // seul. Si l'un des deux change, ce contrôle le dit.
  const EffectSheathedBoltRecipe& jaune =
      EffectSheathedBoltRecipeOf(EffectSheathedBoltBuilder::kImpactYellow);
  const EffectSheathedBoltRecipe& blue =
      EffectSheathedBoltRecipeOf(EffectSheathedBoltBuilder::kImpactBlue);
  CHECK(jaune.delay_floor == blue.delay_floor);
  CHECK(jaune.delay_spread == blue.delay_spread);
  CHECK(jaune.scatter.step == Approx(blue.scatter.step));
  CHECK(jaune.scatter.base == Approx(blue.scatter.base));
  CHECK(jaune.pullback.base == Approx(blue.pullback.base));
  CHECK(jaune.width.base == Approx(blue.width.base));
  CHECK(jaune.lift.base == Approx(blue.lift.base));
  CHECK(jaune.reach.base == Approx(blue.reach.base));
  CHECK(jaune.reach_from_target == blue.reach_from_target);
  CHECK(jaune.style == Approx(kSheathedBoltUnnamedStyle));
  CHECK(jaune.style != Approx(blue.style));

  // ET IL MONTRE CE QUE LA FIGURE GARDE POUR LES STYLES QU'ELLE NE NOMME PAS :
  // l'âme jaune et la gaine ivoire, les seules teintes que les deux passes ne
  // partagent pas.
  CHECK(EffectSheathedBoltColor(kSheathedBoltUnnamedStyle, false) == kSheathedBoltCoreDefault);
  CHECK(EffectSheathedBoltColor(kSheathedBoltUnnamedStyle, true) == kSheathedBoltSheathDefault);
}

TEST_CASE("le semis allume les quatre pistes et pose le style", "[render][sheathed_bolt]") {
  std::uint32_t seed = 7U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSheathedBoltTracks(PatternOf(388), {0.0F, 0.0F, 0.0F}, {30.0F, 0.0F, 0.0F}, seed);
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.alpha == 0);
    CHECK(track.value[kSheathedBoltSlotOfStyle] == Approx(kSheathedBoltWhiteStyle));
    CHECK(track.lift_key == Approx(0.05F));
    // Le retard va de moins quarante-cinq à moins soixante-quatre.
    CHECK(track.ticks <= -45);
    CHECK(track.ticks >= -64);
    // ⚠ LES DEUX BOUTS COÏNCIDENT À LA NAISSANCE.
    CHECK(track.tail[kSheathedBoltSlotOfFarX] == Approx(track.anchor.x));
    CHECK(track.tail[kSheathedBoltSlotOfFarY] == Approx(track.anchor.y));
    CHECK(track.tail[kSheathedBoltSlotOfFarZ] == Approx(track.anchor.z));
  }
}

TEST_CASE("la portee des traits vises se mesure sur la cible", "[render][sheathed_bolt]") {
  std::uint32_t near_point = 3U;
  std::uint32_t far_point = 3U;
  const std::array<EffectTrack, kEffectTracksPerNode> pres = SeedEffectSheathedBoltTracks(
      PatternOf(388), {0.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F}, near_point);
  const std::array<EffectTrack, kEffectTracksPerNode> elsewhere = SeedEffectSheathedBoltTracks(
      PatternOf(388), {0.0F, 0.0F, 0.0F}, {40.0F, 0.0F, 0.0F}, far_point);
  CHECK(elsewhere[0].radius_at_anchor - pres[0].radius_at_anchor == Approx(30.0F));

  // ⚠ ET CELLE DES TRAITS D'IMPACT NE S'Y MESURE PAS : elle est posée en dur,
  // et la cible ne change que leur azimut.
  std::uint32_t here = 3U;
  std::uint32_t over_there = 3U;
  const std::array<EffectTrack, kEffectTracksPerNode> court =
      SeedEffectSheathedBoltTracks(PatternOf(892), {0.0F, 0.0F, 0.0F}, {10.0F, 0.0F, 0.0F}, here);
  const std::array<EffectTrack, kEffectTracksPerNode> pareil = SeedEffectSheathedBoltTracks(
      PatternOf(892), {0.0F, 0.0F, 0.0F}, {40.0F, 0.0F, 0.0F}, over_there);
  CHECK(court[0].radius_at_anchor == Approx(pareil[0].radius_at_anchor));
  CHECK(court[0].radius_at_anchor >= 20.0F);
  CHECK(court[0].radius_at_anchor < 40.0F);
}

TEST_CASE("le recul tire l'ancre en arriere de la cible", "[render][sheathed_bolt]") {
  // Le dernier bâtisseur recule de CENT en dur, vers une cible plein est :
  // l'ancre part donc cent unités à l'ouest, à son écart de dispersion près.
  std::uint32_t seed = 11U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSheathedBoltTracks(PatternOf(1030), {0.0F, 0.0F, 0.0F}, {50.0F, 0.0F, 0.0F}, seed);
  // L'écart de dispersion va de moins quinze à plus quatorze virgule sept.
  for (const EffectTrack& track : tracks) {
    CHECK(track.anchor.x >= -115.0F);
    CHECK(track.anchor.x <= -85.0F);
    CHECK(track.azimuth_degrees == 0);
  }
}

TEST_CASE("un tirage absent ne fait pas avancer le generateur", "[render][sheathed_bolt]") {
  // ⚠ SEPT TIRAGES AU PLUS, ET TROIS BÂTISSEURS EN FONT MOINS. Le client
  // n'appelle pas `rand` là où il pose une constante, et en appeler un
  // décalerait toute la suite de sa séquence.
  const std::array<std::pair<std::uint32_t, std::size_t>, 5> attendus = {{
      {329, 6},
      {388, 6},
      {393, 7},
      {892, 6},
      {1030, 5},
  }};
  for (const auto& [effect_id, count] : attendus) {
    std::uint32_t seed = 123U;
    static_cast<void>(SeedEffectSheathedBoltTracks(PatternOf(effect_id), {0.0F, 0.0F, 0.0F},
                                                   {10.0F, 0.0F, 10.0F}, seed));
    CHECK(seed == AfterDraws(123U, count * kEffectTracksPerNode));
  }
}

TEST_CASE("le noeud seme porte sa forme et son image", "[render][sheathed_bolt]") {
  std::uint32_t seed = 5U;
  const ro::render::PooledEffectNode node =
      SowEffectSheathedBoltNode(PatternOf(892), {4.0F, 1.0F, 6.0F}, {0.0F, 0.0F, 20.0F}, 120, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kSheathedBolt);
  CHECK(node.node.life_ticks == 120);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(PatternOf(892).texture));
  CHECK(node.ground_position.y == Approx(1.0F));
  // La cible est plein nord : le cap du client vaut quatre-vingt-dix degrés.
  CHECK(node.tracks[0].azimuth_degrees == 90);
}

TEST_CASE("les cinq recettes sont celles des cinq batisseurs", "[render][sheathed_bolt]") {
  CHECK_FALSE(
      EffectSheathedBoltRecipeOf(EffectSheathedBoltBuilder::kImpactYellow).reach_from_target);
  CHECK(EffectSheathedBoltRecipeOf(EffectSheathedBoltBuilder::kAimedWhite).reach_from_target);
  CHECK(EffectSheathedBoltRecipeOf(EffectSheathedBoltBuilder::kAimedPink).reach_from_target);
  CHECK_FALSE(EffectSheathedBoltRecipeOf(EffectSheathedBoltBuilder::kImpactBlue).reach_from_target);
  CHECK_FALSE(EffectSheathedBoltRecipeOf(EffectSheathedBoltBuilder::kImpactDim).reach_from_target);
  // ⚠ LE SEUL BÂTISSEUR QUI TIRE LA LARGEUR DE SON ÂME est le rose ; les trois
  // autres la posent en dur.
  CHECK(EffectSheathedBoltRecipeOf(EffectSheathedBoltBuilder::kAimedPink).width.spread == 11);
  CHECK(EffectSheathedBoltRecipeOf(EffectSheathedBoltBuilder::kAimedWhite).width.spread == 0);
  // Et le seul qui pose son recul en dur est le sombre.
  CHECK(EffectSheathedBoltRecipeOf(EffectSheathedBoltBuilder::kImpactDim).pullback.spread == 0);
  CHECK(EffectSheathedBoltRecipeOf(EffectSheathedBoltBuilder::kImpactDim).pullback.base ==
        Approx(100.0F));
}

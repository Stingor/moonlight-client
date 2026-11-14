#include "ro/render/effect_ground_box.h"

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

#include "ro/render/effect_ground_box_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceGroundBoxTracks;
using ro::render::ComposeGroundBoxQuads;
using ro::render::EffectGroundBoxBlendMode;
using ro::render::EffectGroundBoxColor;
using ro::render::EffectGroundBoxPattern;
using ro::render::EffectGroundBoxPatternsFor;
using ro::render::EffectGroundBoxPatternTable;
using ro::render::EffectGroundBoxRecipeOf;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kEffectWallFloorHeight;
using ro::render::kGroundBoxAddedBlend;
using ro::render::kGroundBoxDiagonals;
using ro::render::kGroundBoxFull;
using ro::render::kGroundBoxFullVariant;
using ro::render::kGroundBoxOrdinaryBlend;
using ro::render::kGroundBoxPale;
using ro::render::kGroundBoxPaleVariant;
using ro::render::kGroundBoxPlain;
using ro::render::kGroundBoxPlainVariant;
using ro::render::kGroundBoxPulsingVariant;
using ro::render::kGroundBoxSlotOfBlueFalling;
using ro::render::kGroundBoxSlotOfBlueFloor;
using ro::render::kGroundBoxSlotOfBlueWave;
using ro::render::kGroundBoxSlotOfHalfX;
using ro::render::kGroundBoxSlotOfHalfZ;
using ro::render::kGroundBoxSlotOfHeight;
using ro::render::kGroundBoxSlotOfVariant;
using ro::render::kGroundBoxWalls;
using ro::render::SeedEffectGroundBoxTracks;
using ro::render::SowEffectGroundBoxNode;

namespace {

constexpr float kHalfSide = 4.0F;
constexpr float kHeight = 2.0F;

[[nodiscard]] EffectTrack Box(float variant = kGroundBoxPlainVariant) {
  EffectTrack track;
  track.active = true;
  track.alpha = 30;
  track.azimuth_degrees = 0;
  track.value[kGroundBoxSlotOfHalfX] = kHalfSide;
  track.value[kGroundBoxSlotOfHalfZ] = kHalfSide;
  track.value[kGroundBoxSlotOfHeight] = kHeight;
  track.value[kGroundBoxSlotOfVariant] = variant;
  return track;
}

[[nodiscard]] const EffectGroundBoxPattern& PatternOf(std::uint32_t effect_id) {
  const std::span<const EffectGroundBoxPattern> patterns = EffectGroundBoxPatternsFor(effect_id);
  REQUIRE(patterns.size() == 1);
  return patterns.front();
}

}  // namespace

TEST_CASE("la boite dresse quatre murs qui se referment", "[render][ground_box]") {
  const std::vector<EffectWorldQuad> quads = ComposeGroundBoxQuads(Box(), {0.0F, 0.0F, 0.0F});
  REQUIRE(quads.size() == kGroundBoxWalls);
  for (std::size_t wall = 0; wall < kGroundBoxWalls; ++wall) {
    const EffectWorldQuad& current = quads[wall];
    const EffectWorldQuad& next = quads[(wall + 1) % kGroundBoxWalls];
    CHECK(current.corners[1].x == Approx(next.corners[0].x).margin(1e-4));
    CHECK(current.corners[1].z == Approx(next.corners[0].z).margin(1e-4));
  }
  // Le plancher est UNE unité au-dessus du nœud, et la tête à la hauteur.
  CHECK(quads.front().corners[0].y == Approx(kEffectWallFloorHeight));
  CHECK(quads.front().corners[3].y == Approx(kEffectWallFloorHeight + kHeight));
}

TEST_CASE("l'azimut tourne les quatre coins", "[render][ground_box]") {
  // ⚠ AUCUN BÂTISSEUR N'EN POSE, mais le dessinateur le lit : à zéro la boîte
  // est alignée sur les axes, à quatre-vingt-dix elle a tourné d'un quart.
  // ⚠ LA ROTATION SE FAIT DANS LE REPÈRE DU CLIENT, dont la profondeur est
  // l'opposée de la nôtre : le premier coin, à profondeur positive chez lui,
  // est à profondeur négative ici.
  const std::vector<EffectWorldQuad> right = ComposeGroundBoxQuads(Box(), {});
  CHECK(right.front().corners[0].x == Approx(kHalfSide));
  CHECK(right.front().corners[0].z == Approx(-kHalfSide));

  EffectTrack tournee = Box();
  tournee.azimuth_degrees = 90;
  const std::vector<EffectWorldQuad> quads = ComposeGroundBoxQuads(tournee, {});
  CHECK(quads.front().corners[0].x == Approx(-kHalfSide).margin(1e-4));
  CHECK(quads.front().corners[0].z == Approx(-kHalfSide).margin(1e-4));
}

TEST_CASE("une piste eteinte ou muette ne rend rien", "[render][ground_box]") {
  EffectTrack faded = Box();
  faded.active = false;
  CHECK(ComposeGroundBoxQuads(faded, {}).empty());
  EffectTrack silent = Box();
  silent.alpha = 0;
  CHECK(ComposeGroundBoxQuads(silent, {}).empty());
}

TEST_CASE("la variante pleine ajoute deux diagonales a l'alpha double", "[render][ground_box]") {
  const std::vector<EffectWorldQuad> quads = ComposeGroundBoxQuads(Box(kGroundBoxFullVariant), {});
  REQUIRE(quads.size() == kGroundBoxWalls + kGroundBoxDiagonals);
  // ⚠ LES DEUX DERNIERS TRAVERSENT LA BOÎTE, et leur alpha est DOUBLÉ : c'est
  // ce qui rend le mur de brume plein au lieu d'un cadre vide.
  CHECK(quads[4].tint[3] == 60);
  CHECK(quads[0].tint[3] == 30);
  CHECK(quads[4].corners[0].x == Approx(kHalfSide));
  CHECK(quads[4].corners[1].x == Approx(-kHalfSide));

  // Et l'alpha doublé se borne.
  EffectTrack living = Box(kGroundBoxFullVariant);
  living.alpha = 200;
  CHECK(ComposeGroundBoxQuads(living, {})[4].tint[3] == 255);
}

TEST_CASE("chaque variante a sa teinte et son melange", "[render][ground_box]") {
  CHECK(EffectGroundBoxColor(Box(kGroundBoxPlainVariant)) == kGroundBoxPlain);
  CHECK(EffectGroundBoxColor(Box(kGroundBoxPaleVariant)) == kGroundBoxPale);
  CHECK(EffectGroundBoxColor(Box(kGroundBoxFullVariant)) == kGroundBoxFull);
  // ⚠ ET LA CINQUIÈME BRANCHE DU CLIENT LIT DES VALEURS NON INITIALISÉES :
  // nous rendons la variante nue plutôt qu'un hasard de pile.
  CHECK(EffectGroundBoxColor(Box(3.0F)) == kGroundBoxPlain);

  CHECK(EffectGroundBoxBlendMode(kGroundBoxPlainVariant) == kGroundBoxAddedBlend);
  CHECK(EffectGroundBoxBlendMode(kGroundBoxPaleVariant) == kGroundBoxOrdinaryBlend);
  CHECK(EffectGroundBoxBlendMode(kGroundBoxFullVariant) == kGroundBoxOrdinaryBlend);
}

TEST_CASE("le bleu de la variante qui pulse est une somme", "[render][ground_box]") {
  EffectTrack track = Box(kGroundBoxPulsingVariant);
  track.value[kGroundBoxSlotOfBlueFloor] = 30.0F;
  track.value[kGroundBoxSlotOfBlueWave] = 120.0F;
  const std::array<std::uint8_t, 3> tint = EffectGroundBoxColor(track);
  CHECK(tint[0] == 255);
  CHECK(tint[1] == 255);
  CHECK(tint[2] == 150);
}

TEST_CASE("la hauteur respire sans jamais tomber a zero", "[render][ground_box]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Box();
  tracks[0].lift_key = 10.0F;
  tracks[0].style = 1000;
  tracks[0].ticks = 200;  // au-delà du dépliage, en deçà de la durée
  // ⚠ L'ANGLE AVANCE AVANT QUE LA HAUTEUR NE SE CALCULE : à un angle de
  // deux cent soixante-neuf, le sinus vaut presque moins un, et la hauteur
  // tombe au creux — trois dixièmes de la clé, jamais zéro.
  tracks[0].degrees = 269;
  AdvanceGroundBoxTracks(tracks);
  CHECK(tracks[0].degrees == 270);
  CHECK(tracks[0].value[kGroundBoxSlotOfHeight] == Approx(3.0F).margin(1e-4));

  tracks[0].degrees = 89;
  AdvanceGroundBoxTracks(tracks);
  CHECK(tracks[0].value[kGroundBoxSlotOfHeight] == Approx(10.0F).margin(1e-4));

  // Et l'angle se replie sur le tour.
  tracks[0].degrees = 359;
  AdvanceGroundBoxTracks(tracks);
  CHECK(tracks[0].degrees == 0);
}

TEST_CASE("la boite se deplie sur les quatre-vingt-dix premiers tics", "[render][ground_box]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Box();
  tracks[0].lift_key = 10.0F;
  tracks[0].style = 1000;
  tracks[0].ticks = 0;
  tracks[0].degrees = 89;  // le sinus vaudra un au tic suivant
  AdvanceGroundBoxTracks(tracks);
  const float un_degre = std::sin(std::numbers::pi_v<float> / 180.0F);
  CHECK(tracks[0].ticks == 1);
  CHECK(tracks[0].value[kGroundBoxSlotOfHeight] == Approx(10.0F * un_degre).margin(1e-4));
}

TEST_CASE("l'alpha ne tombe que passe la duree", "[render][ground_box]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Box();
  tracks[0].lift_key = 10.0F;
  tracks[0].alpha = 30;
  tracks[0].style = 100;
  tracks[0].ticks = 50;
  AdvanceGroundBoxTracks(tracks);
  CHECK(tracks[0].alpha == 30);
  tracks[0].ticks = 100;
  AdvanceGroundBoxTracks(tracks);
  CHECK(tracks[0].alpha == 28);
  for (std::int32_t tick = 0; tick < 40; ++tick) {
    AdvanceGroundBoxTracks(tracks);
  }
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("le bleu monte, bute, et redescend", "[render][ground_box]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Box(kGroundBoxPulsingVariant);
  tracks[0].lift_key = 10.0F;
  tracks[0].style = 30000;
  tracks[0].value[kGroundBoxSlotOfBlueWave] = 199.0F;
  AdvanceGroundBoxTracks(tracks);
  CHECK(tracks[0].value[kGroundBoxSlotOfBlueWave] == Approx(200.0F));
  CHECK(tracks[0].value[kGroundBoxSlotOfBlueFalling] == Approx(0.0F));
  AdvanceGroundBoxTracks(tracks);
  // ⚠ LE CLIENT DÉPASSE D'UN PAS AVANT DE SE RETOURNER : il compare APRÈS
  // avoir ajouté, puis RAMÈNE la valeur au plafond.
  CHECK(tracks[0].value[kGroundBoxSlotOfBlueWave] == Approx(200.0F));
  CHECK(tracks[0].value[kGroundBoxSlotOfBlueFalling] == Approx(1.0F));
  AdvanceGroundBoxTracks(tracks);
  CHECK(tracks[0].value[kGroundBoxSlotOfBlueWave] == Approx(199.0F));

  // ⚠ ET LES AUTRES VARIANTES NE PULSENT PAS.
  std::array<EffectTrack, kEffectTracksPerNode> fixed;
  fixed[0] = Box(kGroundBoxPlainVariant);
  fixed[0].style = 30000;
  AdvanceGroundBoxTracks(fixed);
  CHECK(fixed[0].value[kGroundBoxSlotOfBlueWave] == Approx(0.0F));
}

TEST_CASE("la table porte les quatre identifiants du sous-type 64", "[render][ground_box]") {
  const std::span<const EffectGroundBoxPattern> table = EffectGroundBoxPatternTable();
  CHECK(table.size() == 4);
  CHECK(std::ranges::is_sorted(table, {}, &EffectGroundBoxPattern::effect_id));
  CHECK(PatternOf(318).style_byte == 0);
  CHECK(PatternOf(319).style_byte == 1);
  CHECK(PatternOf(405).style_byte == 2);
  CHECK(PatternOf(1088).style_byte == 4);
  CHECK(EffectGroundBoxPatternsFor(1).empty());
}

TEST_CASE("l'octet du site porte la hauteur, l'alpha, la duree et la taille",
          "[render][ground_box]") {
  CHECK(EffectGroundBoxRecipeOf(0).lift_key == Approx(50.0F));
  CHECK(EffectGroundBoxRecipeOf(0).half_side == Approx(5.0F));
  CHECK(EffectGroundBoxRecipeOf(0).alpha == 30);
  CHECK(EffectGroundBoxRecipeOf(0).duration == 864);
  CHECK(EffectGroundBoxRecipeOf(1).lift_key == Approx(16.0F));
  CHECK(EffectGroundBoxRecipeOf(1).half_side == Approx(2.5F));
  CHECK(EffectGroundBoxRecipeOf(1).alpha == 120);
  CHECK(EffectGroundBoxRecipeOf(2).lift_key == Approx(32.0F));
  CHECK(EffectGroundBoxRecipeOf(2).duration == 29999);
  // ⚠ L'OCTET TROIS NE POSE NI ALPHA NI DURÉE, et personne ne l'emploie.
  CHECK(EffectGroundBoxRecipeOf(3).alpha == 0);
  CHECK(EffectGroundBoxRecipeOf(3).lift_key == Approx(16.0F));
  // ⚠ ET L'OCTET QUATRE EST LE SEUL À TIRER SON BLEU.
  CHECK(EffectGroundBoxRecipeOf(4).draws_blue);
  CHECK_FALSE(EffectGroundBoxRecipeOf(0).draws_blue);
}

TEST_CASE("le semis n'allume qu'UNE piste sur quatre", "[render][ground_box]") {
  std::uint32_t seed = 9U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectGroundBoxTracks(PatternOf(318), seed);
  CHECK(tracks[0].active);
  CHECK_FALSE(tracks[1].active);
  CHECK_FALSE(tracks[2].active);
  CHECK_FALSE(tracks[3].active);
  CHECK(tracks[0].alpha == 30);
  CHECK(tracks[0].style == 864);
  CHECK(tracks[0].lift_key == Approx(50.0F));
  CHECK(tracks[0].azimuth_degrees == 0);
  CHECK(tracks[0].degrees >= 0);
  CHECK(tracks[0].degrees < 360);
  CHECK(tracks[0].value[kGroundBoxSlotOfHalfX] == Approx(5.0F));
  CHECK(tracks[0].value[kGroundBoxSlotOfHalfZ] == Approx(5.0F));
  CHECK(tracks[0].value[kGroundBoxSlotOfVariant] == Approx(0.0F));
  // ⚠ LA HAUTEUR NAÎT NULLE : c'est le mouvement qui déplie la boîte.
  CHECK(tracks[0].value[kGroundBoxSlotOfHeight] == Approx(0.0F));
}

TEST_CASE("un tirage absent ne fait pas avancer le generateur", "[render][ground_box]") {
  // ⚠ SEUL L'OCTET QUATRE TIRE DEUX FOIS. Tirer pour les autres décalerait
  // toute la suite de la séquence.
  std::uint32_t sans = 42U;
  static_cast<void>(SeedEffectGroundBoxTracks(PatternOf(318), sans));
  std::uint32_t with = 42U;
  static_cast<void>(SeedEffectGroundBoxTracks(PatternOf(1088), with));
  CHECK(sans != with);
  CHECK(SeedEffectGroundBoxTracks(PatternOf(1088), with)[0].value[kGroundBoxSlotOfBlueFloor] <
        50.0F);
}

TEST_CASE("le noeud seme porte sa forme et son image", "[render][ground_box]") {
  std::uint32_t seed = 3U;
  const ro::render::PooledEffectNode node =
      SowEffectGroundBoxNode(PatternOf(405), {2.0F, 0.0F, 7.0F}, 600, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kGroundBox);
  CHECK(node.node.life_ticks == 600);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(PatternOf(405).texture));
  CHECK(node.tracks[0].value[kGroundBoxSlotOfVariant] == Approx(kGroundBoxFullVariant));
}

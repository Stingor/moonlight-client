#include "ro/render/effect_anchored_skirt.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_anchored_skirt_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceAnchoredSkirtTracks;
using ro::render::ComposeAnchoredSkirtQuads;
using ro::render::EffectAnchoredSkirtDrift;
using ro::render::EffectAnchoredSkirtPattern;
using ro::render::EffectAnchoredSkirtPatternsFor;
using ro::render::EffectAnchoredSkirtPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kAnchoredSkirtDelayPerTrack;
using ro::render::kAnchoredSkirtFirstTiltStep;
using ro::render::kAnchoredSkirtInnerShare;
using ro::render::kAnchoredSkirtRadiusPerTrack;
using ro::render::kAnchoredSkirtSteps;
using ro::render::kAnchoredSkirtSweepDegrees;
using ro::render::kAnchoredSkirtTailOfDriftX;
using ro::render::kAnchoredSkirtTailOfDriftZ;
using ro::render::kAnchoredSkirtTailOfVariant;
using ro::render::kAnchoredSkirtTiltPerTrack;
using ro::render::kEffectTrackSlots;
using ro::render::kEffectTracksPerNode;
using ro::render::SeedEffectAnchoredSkirtTracks;
using ro::render::SowEffectAnchoredSkirtNode;

namespace {

[[nodiscard]] const EffectAnchoredSkirtPattern& PatternOf(std::uint32_t effect_id) {
  return EffectAnchoredSkirtPatternsFor(effect_id).front();
}

// UNE PISTE DÉJÀ OUVERTE : vingt et une longueurs pleines, compteur positif.
[[nodiscard]] EffectTrack OpenTrack() {
  EffectTrack track;
  track.active = true;
  track.alpha = 200;
  track.ticks = 20;
  track.degrees = kAnchoredSkirtSweepDegrees;
  track.azimuth_degrees = 0;
  track.tilt_degrees = 70;
  track.radius_at_anchor = 3.0F;
  track.lift_key = 30.0F;
  for (float& length : track.value) {
    length = 30.0F;
  }
  return track;
}

}  // namespace

TEST_CASE("le balayage de la gerbe ne tombe pas juste sur vingt", "[render][anchored_skirt]") {
  const std::vector<EffectWorldQuad> bands =
      ComposeAnchoredSkirtQuads(OpenTrack(), {0.0F, 0.0F}, 13, {0.0F, 0.0F, 0.0F});
  // ⚠ TROIS CENT QUINZE DEGRÉS PAR PAS DE QUINZE TIENNENT VINGT ET UNE BANDES,
  // pas vingt : le pas est le balayage divisé EN ENTIERS, et la boucle avance
  // tant qu'elle ne le dépasse pas. C'est le même quirk que le ruban qui
  // tourne, et les deux bâtisseurs posent exactement ce balayage-là.
  CHECK(bands.size() == static_cast<std::size_t>(kAnchoredSkirtSteps) + 1);
}

TEST_CASE("une piste eteinte, en retard ou sans image ne rend rien", "[render][anchored_skirt]") {
  EffectTrack faded = OpenTrack();
  faded.active = false;
  CHECK(ComposeAnchoredSkirtQuads(faded, {0.0F, 0.0F}, 13, {0.0F, 0.0F, 0.0F}).empty());
  // ⚠ LE COMPTEUR EST UNE GARDE : une piste en retard est allumée ET opaque, et
  // ne se dessine pas.
  EffectTrack delayed = OpenTrack();
  delayed.ticks = 0;
  CHECK(ComposeAnchoredSkirtQuads(delayed, {0.0F, 0.0F}, 13, {0.0F, 0.0F, 0.0F}).empty());
  CHECK(ComposeAnchoredSkirtQuads(OpenTrack(), {0.0F, 0.0F}, 0, {0.0F, 0.0F, 0.0F}).empty());
  EffectTrack silent = OpenTrack();
  silent.alpha = 0;
  CHECK(ComposeAnchoredSkirtQuads(silent, {0.0F, 0.0F}, 13, {0.0F, 0.0F, 0.0F}).empty());
}

TEST_CASE("le bord interieur est a hauteur NULLE, l'exterieur s'eleve",
          "[render][anchored_skirt]") {
  const std::vector<EffectWorldQuad> bands =
      ComposeAnchoredSkirtQuads(OpenTrack(), {0.0F, 0.0F}, 13, {0.0F, 0.0F, 0.0F});
  REQUIRE_FALSE(bands.empty());
  // Les coins zéro et un sont les deux bords intérieurs, les deux autres les
  // extérieurs.
  CHECK(bands.front().corners[0].y == Approx(0.0F));
  CHECK(bands.front().corners[1].y == Approx(0.0F));
  // ⚠ NOTRE REPÈRE MONTE : la jupe S'ÉLÈVE là où le client écrit un nombre
  // négatif.
  CHECK(bands.front().corners[2].y > 0.0F);
}

TEST_CASE("le bord interieur ne recoit que les trois quarts de la derive",
          "[render][anchored_skirt]") {
  const glm::vec2 derive{8.0F, 4.0F};
  const std::vector<EffectWorldQuad> sans =
      ComposeAnchoredSkirtQuads(OpenTrack(), {0.0F, 0.0F}, 13, {0.0F, 0.0F, 0.0F});
  const std::vector<EffectWorldQuad> with =
      ComposeAnchoredSkirtQuads(OpenTrack(), derive, 13, {0.0F, 0.0F, 0.0F});
  REQUIRE(sans.size() == with.size());
  // ⚠ TROIS QUARTS POUR L'INTÉRIEUR, ENTIER POUR L'EXTÉRIEUR. Leur donner la
  // même part refermerait la gerbe sur elle-même.
  CHECK(with.front().corners[1].x ==
        Approx(sans.front().corners[1].x - kAnchoredSkirtInnerShare * derive.x));
  CHECK(with.front().corners[2].x == Approx(sans.front().corners[2].x - derive.x));
  // ⚠ LA DÉRIVE S'ACCUMULE DANS LE REPÈRE DU CLIENT : sa profondeur se retourne
  // en passant au nôtre, et s'ajoute là où le client la retranche.
  CHECK(with.front().corners[2].z == Approx(sans.front().corners[2].z + derive.y));
}

TEST_CASE("la derive vient de la PREMIERE piste", "[render][anchored_skirt]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].tail[kAnchoredSkirtTailOfDriftX] = 5.0F;
  tracks[0].tail[kAnchoredSkirtTailOfDriftZ] = -3.0F;
  tracks[2].tail[kAnchoredSkirtTailOfDriftX] = 99.0F;
  const glm::vec2 derive = EffectAnchoredSkirtDrift(tracks);
  // ⚠ CELLE DE LA PREMIÈRE, quelle que soit la piste qu'on dessine : le client
  // lit un déplacement ABSOLU depuis le nœud.
  CHECK(derive.x == Approx(5.0F));
  CHECK(derive.y == Approx(-3.0F));
}

TEST_CASE("l'image defile avec le compteur", "[render][anchored_skirt]") {
  EffectTrack track = OpenTrack();
  track.ticks = 5;
  CHECK(ComposeAnchoredSkirtQuads(track, {0.0F, 0.0F}, 13, {0.0F, 0.0F, 0.0F}).front().texture ==
        5);
  track.ticks = 14;
  CHECK(ComposeAnchoredSkirtQuads(track, {0.0F, 0.0F}, 13, {0.0F, 0.0F, 0.0F}).front().texture ==
        1);
  // Une seule image : l'index reste à zéro.
  track.ticks = 7;
  CHECK(ComposeAnchoredSkirtQuads(track, {0.0F, 0.0F}, 1, {0.0F, 0.0F, 0.0F}).front().texture == 0);
}

TEST_CASE("le ruban se retrecit a ses deux bouts", "[render][anchored_skirt]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectAnchoredSkirtTracks(PatternOf(832), {1.0F, 0.0F, 0.0F});
  // Passé l'ouverture, les longueurs sont celles de la table de rétrécissement.
  tracks[0].ticks = 50;
  tracks[0].alpha = 200;
  AdvanceAnchoredSkirtTracks(tracks);
  const float full = tracks[0].lift_key;
  CHECK(tracks[0].value[0] == Approx(0.4F * full));
  CHECK(tracks[0].value[1] == Approx(0.5F * full));
  CHECK(tracks[0].value[5] == Approx(0.9F * full));
  // ⚠ LES NEUF DU MILIEU GARDENT LEUR PLEINE LONGUEUR.
  CHECK(tracks[0].value[10] == Approx(full));
  // Et la table est SYMÉTRIQUE.
  CHECK(tracks[0].value[kEffectTrackSlots - 1] == Approx(0.4F * full));
  CHECK(tracks[0].value[kEffectTrackSlots - 2] == Approx(0.5F * full));
}

TEST_CASE("la gerbe s'ouvre en quinze tics", "[render][anchored_skirt]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectAnchoredSkirtTracks(PatternOf(832), {1.0F, 0.0F, 0.0F});
  AdvanceAnchoredSkirtTracks(tracks);
  REQUIRE(tracks[0].ticks == 1);
  const float at_first = tracks[0].value[10];
  for (std::int32_t tick = 0; tick < 14; ++tick) {
    AdvanceAnchoredSkirtTracks(tracks);
  }
  REQUIRE(tracks[0].ticks == 15);
  CHECK(tracks[0].value[10] > at_first);
  // ⚠ AU QUINZIÈME TIC ELLE EST PLEINE : le sinus de quatre-vingt-dix.
  CHECK(tracks[0].value[10] == Approx(tracks[0].lift_key).margin(0.01F));
}

TEST_CASE("la gerbe recommence PLUS BAS au lieu de mourir", "[render][anchored_skirt]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectAnchoredSkirtTracks(PatternOf(832), {1.0F, 0.0F, 0.0F});
  const std::int16_t tilt = tracks[0].tilt_degrees;
  const float radius = tracks[0].radius_at_anchor;
  // Assez de tics pour monter puis retomber sous zéro.
  for (std::int32_t tick = 0; tick < 60; ++tick) {
    AdvanceAnchoredSkirtTracks(tracks);
  }
  // ⚠ ELLE EST TOUJOURS VIVE, et elle a perdu vingt degrés et quatre dixièmes.
  CHECK(tracks[0].active);
  CHECK(tracks[0].tilt_degrees < tilt);
  CHECK(tracks[0].radius_at_anchor < radius);
  // Et au bout de ses tours, elle s'éteint sur son plancher.
  for (std::int32_t tick = 0; tick < 400; ++tick) {
    AdvanceAnchoredSkirtTracks(tracks);
  }
  CHECK_FALSE(tracks[0].active);
  CHECK(tracks[0].tilt_degrees == 40);
}

TEST_CASE("la derive avance de quinze degres EN ARRIERE de l'azimut", "[render][anchored_skirt]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0].active = true;
  tracks[0].ticks = 0;
  tracks[0].azimuth_degrees = 15;
  tracks[0].degrees = kAnchoredSkirtSweepDegrees;
  AdvanceAnchoredSkirtTracks(tracks);
  // Azimut quinze moins quinze : cap zéro, donc plein est.
  CHECK(tracks[0].tail[kAnchoredSkirtTailOfDriftX] == Approx(1.0F));
  CHECK(tracks[0].tail[kAnchoredSkirtTailOfDriftZ] == Approx(0.0F).margin(0.001F));
}

TEST_CASE("le semis echelonne les quatre pistes", "[render][anchored_skirt]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectAnchoredSkirtTracks(PatternOf(832), {1.0F, 0.0F, 0.0F});
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].alpha == 0);
    CHECK(tracks[rang].degrees == kAnchoredSkirtSweepDegrees);
    CHECK(tracks[rang].ticks == -kAnchoredSkirtDelayPerTrack * static_cast<std::int32_t>(rang));
    CHECK(
        tracks[rang].tilt_degrees ==
        static_cast<std::int16_t>(kAnchoredSkirtTiltPerTrack *
                                  (kAnchoredSkirtFirstTiltStep - static_cast<std::int16_t>(rang))));
    CHECK(tracks[rang].radius_at_anchor ==
          Approx(PatternOf(832).first_radius -
                 static_cast<float>(rang) * kAnchoredSkirtRadiusPerTrack));
    CHECK(tracks[rang].lift_key == Approx(PatternOf(832).ribbon_length));
    CHECK(tracks[rang].tail[kAnchoredSkirtTailOfVariant] == Approx(PatternOf(832).trail_variant));
  }
  // ⚠ SOIXANTE-DIX DEGRÉS POUR LA PREMIÈRE.
  CHECK(tracks[0].tilt_degrees == 70);
}

TEST_CASE("le cap part PRESQUE a l'oppose de sa cible", "[render][anchored_skirt]") {
  const std::array<EffectTrack, kEffectTracksPerNode> vers_l_est =
      SeedEffectAnchoredSkirtTracks(PatternOf(832), {1.0F, 0.0F, 0.0F});
  // ⚠ CENT QUATRE-VINGT-QUINZE DEGRÉS DE PLUS QUE LA CIBLE, et c'est mesuré :
  // la gerbe jaillit vers l'arrière avant de dériver.
  CHECK(vers_l_est[0].azimuth_degrees == 195);
  const std::array<EffectTrack, kEffectTracksPerNode> without_target =
      SeedEffectAnchoredSkirtTracks(PatternOf(832), {0.0F, 0.0F, 0.0F});
  CHECK(without_target[0].azimuth_degrees == 195);
}

TEST_CASE("la table porte QUATRE lignes pour trois identifiants", "[render][anchored_skirt]") {
  const std::span<const EffectAnchoredSkirtPattern> table = EffectAnchoredSkirtPatternTable();
  REQUIRE(table.size() == 4);
  CHECK(table[0].effect_id == 832);
  CHECK(table[1].effect_id == 943);
  // ⚠ DEUX LIGNES POUR LE DERNIER : son site appelle le bâtisseur DEUX FOIS, sur
  // deux images différentes.
  CHECK(table[2].effect_id == 1678);
  CHECK(table[3].effect_id == 1678);
  CHECK(EffectAnchoredSkirtPatternsFor(1678).size() == 2);
  // TREIZE IMAGES POUR LES BULLES, UNE POUR LES ONDES.
  CHECK(table[0].textures.size() == 13);
  CHECK(table[2].textures.size() == 1);
  CHECK(table[0].trail_variant == Approx(0.0F));
  CHECK(table[1].trail_variant == Approx(7.0F));
  // Et le second bâtisseur sème trois fois.
  CHECK(table[2].schedule.last_tick == 40);
  CHECK(table[2].schedule.period_ticks == 20);
  CHECK(EffectAnchoredSkirtPatternsFor(1U).empty());
}

TEST_CASE("le noeud seme porte sa forme et TOUTES ses images", "[render][anchored_skirt]") {
  const ro::render::PooledEffectNode node =
      SowEffectAnchoredSkirtNode(PatternOf(832), {2.0F, 0.0F, 7.0F}, {1.0F, 0.0F, 0.0F}, 600);
  CHECK(node.shape == ro::render::PooledNodeShape::kAnchoredSkirt);
  CHECK(node.node.life_ticks == 600);
  // ⚠ TREIZE IMAGES DANS LE NŒUD, et c'est le nombre que la figure lit pour
  // faire défiler.
  CHECK(node.resource_paths.size() == 13);
  CHECK(node.resource_paths.front() == std::string(PatternOf(832).textures.front()));
}

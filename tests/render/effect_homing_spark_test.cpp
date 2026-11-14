#include "ro/render/effect_homing_spark.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_homing_spark_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceHomingSparkTracks;
using ro::render::ComposeHomingSparkQuads;
using ro::render::EffectHomingSparkPattern;
using ro::render::EffectHomingSparkPatternsFor;
using ro::render::EffectHomingSparkPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kHomingSparkApproachPhase;
using ro::render::kHomingSparkBladeHalfWidths;
using ro::render::kHomingSparkBladeRadius;
using ro::render::kHomingSparkBlades;
using ro::render::kHomingSparkDriftPhase;
using ro::render::kHomingSparkLift;
using ro::render::kHomingSparkMarkOfColor;
using ro::render::kHomingSparkMarkOfPhase;
using ro::render::kHomingSparkMarkOfSpinCount;
using ro::render::kHomingSparkMarks;
using ro::render::kHomingSparkSlotOfLift;
using ro::render::kHomingSparkSlotOfStep;
using ro::render::kHomingSparkSphereRadius;
using ro::render::kHomingSparkSpinPhase;
using ro::render::kHomingSparkStep;
using ro::render::kHomingSparkThinColors;
using ro::render::kHomingSparkWideColors;
using ro::render::SeedEffectHomingSparkTracks;
using ro::render::SowEffectHomingSparkNode;

namespace {

[[nodiscard]] EffectTrack Spark(std::uint8_t tint = 0) {
  EffectTrack track;
  track.active = true;
  track.alpha = 100;
  track.ticks = 5;
  track.tilt_degrees = 0;
  track.azimuth_degrees = 0;
  track.radius_at_anchor = 4.0F;
  track.anchor = {30.0F, 0.0F, 0.0F};
  track.value[kHomingSparkSlotOfLift] = 0.0F;
  track.value[kHomingSparkSlotOfStep] = kHomingSparkStep;
  track.mark[kHomingSparkMarkOfColor] = tint;
  return track;
}

[[nodiscard]] const EffectHomingSparkPattern& PatternOf(std::uint32_t effect_id) {
  const std::span<const EffectHomingSparkPattern> patterns =
      EffectHomingSparkPatternsFor(effect_id);
  REQUIRE(patterns.size() == 1);
  return patterns.front();
}

}  // namespace

TEST_CASE("une piste rend DEUX lames", "[render][homing_spark]") {
  const std::vector<EffectWorldQuad> quads = ComposeHomingSparkQuads(Spark(), {});
  CHECK(quads.size() == kHomingSparkBlades);
}

TEST_CASE("les deux lames sont centrees sur le MEME angle", "[render][homing_spark]") {
  // ⚠ ELLES NE SE CROISENT PAS : la large est plus ouverte, mais son milieu
  // est celui de la fine. Les croiser en ferait une étoile.
  const std::vector<EffectWorldQuad> quads = ComposeHomingSparkQuads(Spark(), {});
  const glm::vec3 thin_midpoint = (quads[0].corners[0] + quads[0].corners[1]) * 0.5F;
  const glm::vec3 wide_midpoint = (quads[1].corners[0] + quads[1].corners[1]) * 0.5F;
  // Les deux milieux sont sur le même rayon, la large un peu plus courte.
  CHECK(thin_midpoint.z == Approx(wide_midpoint.z).margin(1e-4));
  CHECK(thin_midpoint.x > wide_midpoint.x);
  // Et la large est bien plus ouverte.
  const float ouverture_fine = std::abs(quads[0].corners[1].z - quads[0].corners[0].z);
  const float ouverture_large = std::abs(quads[1].corners[1].z - quads[1].corners[0].z);
  CHECK(ouverture_large > ouverture_fine);
  CHECK(kHomingSparkBladeHalfWidths[1] > kHomingSparkBladeHalfWidths[0]);
}

TEST_CASE("chaque lame traverse le centre", "[render][homing_spark]") {
  // ⚠ CE N'EST PAS LE QUATRIÈME COIN QUI FAIT FACE AU PREMIER, MAIS LE
  // TROISIÈME. Les quatre angles sont -w, +w, 180-w et 180+w : c'est donc A
  // avec C, et B avec D, qui sont diamétralement opposés. Prendre A et D
  // ferait croire la lame tordue.
  const std::vector<EffectWorldQuad> quads = ComposeHomingSparkQuads(Spark(), {});
  const glm::vec3 first = quads[0].corners[0];
  const glm::vec3 oppose = quads[0].corners[2];
  CHECK(first.x - 30.0F == Approx(-(oppose.x - 30.0F)).margin(1e-3));
  CHECK(first.z == Approx(-oppose.z).margin(1e-3));
}

TEST_CASE("une piste eteinte, muette ou sans teinte ne rend rien", "[render][homing_spark]") {
  EffectTrack faded = Spark();
  faded.active = false;
  CHECK(ComposeHomingSparkQuads(faded, {}).empty());
  EffectTrack silent = Spark();
  silent.alpha = 0;
  CHECK(ComposeHomingSparkQuads(silent, {}).empty());
  // ⚠ AU-DELÀ DES QUATRE TEINTES, LE CLIENT NE DESSINE RIEN : son aiguillage
  // n'a pas de cas par défaut.
  EffectTrack inconnue = Spark(kHomingSparkMarks);
  CHECK(ComposeHomingSparkQuads(inconnue, {}).empty());
}

TEST_CASE("les deux lames d'une marque n'ont pas la meme teinte", "[render][homing_spark]") {
  for (std::uint8_t tint = 0; tint < kHomingSparkMarks; ++tint) {
    const std::vector<EffectWorldQuad> quads = ComposeHomingSparkQuads(Spark(tint), {});
    REQUIRE(quads.size() == kHomingSparkBlades);
    CHECK(quads[0].tint[0] == kHomingSparkThinColors[tint][0]);
    CHECK(quads[0].tint[1] == kHomingSparkThinColors[tint][1]);
    CHECK(quads[1].tint[0] == kHomingSparkWideColors[tint][0]);
    CHECK(quads[1].tint[2] == kHomingSparkWideColors[tint][2]);
  }
}

TEST_CASE("le decalage vertical s'ajoute a chaque coin", "[render][homing_spark]") {
  // ⚠ LE CLIENT L'ÉCRIT DEUX FOIS : une dans l'ancre au semis, une par coin au
  // dessin. Et notre repère monte là où le sien descend.
  EffectTrack track = Spark();
  track.value[kHomingSparkSlotOfLift] = -5.0F;
  const std::vector<EffectWorldQuad> quads = ComposeHomingSparkQuads(track, {});
  EffectTrack sans = Spark();
  const std::vector<EffectWorldQuad> nus = ComposeHomingSparkQuads(sans, {});
  CHECK(quads[0].corners[0].y - nus[0].corners[0].y == Approx(5.0F));
}

TEST_CASE("l'approche fonce en ligne droite vers le centre", "[render][homing_spark]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Spark();
  tracks[0].anchor = {30.0F, 0.0F, 0.0F};
  AdvanceHomingSparkTracks(tracks);
  CHECK(tracks[0].anchor.x == Approx(30.0F - kHomingSparkStep));
  // ⚠ ET LES ANGLES NE BOUGENT PAS TANT QU'ELLE APPROCHE.
  CHECK(tracks[0].tilt_degrees == 0);
  CHECK(tracks[0].mark[kHomingSparkMarkOfPhase] == kHomingSparkApproachPhase);
}

TEST_CASE("elle se met a tournoyer a DOUZE unites du centre", "[render][homing_spark]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Spark();
  tracks[0].anchor = {11.0F, 0.0F, 0.0F};
  // Passé le vingtième tic, la montée d'alpha ne masque plus la chute.
  tracks[0].ticks = 30;
  AdvanceHomingSparkTracks(tracks);
  CHECK(tracks[0].mark[kHomingSparkMarkOfPhase] == kHomingSparkSpinPhase);
  CHECK(tracks[0].mark[kHomingSparkMarkOfSpinCount] == 1);
  CHECK(tracks[0].tilt_degrees == 9);
  CHECK(tracks[0].alpha == 98);
  CHECK(tracks[0].radius_at_anchor == Approx(4.0F * 0.99F));
}

TEST_CASE("le tournoiement dure DIX tics puis cede a la derive", "[render][homing_spark]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Spark();
  tracks[0].anchor = {11.0F, 0.0F, 0.0F};
  for (std::int32_t tick = 0; tick < 9; ++tick) {
    AdvanceHomingSparkTracks(tracks);
  }
  CHECK(tracks[0].mark[kHomingSparkMarkOfPhase] == kHomingSparkSpinPhase);
  AdvanceHomingSparkTracks(tracks);
  CHECK(tracks[0].mark[kHomingSparkMarkOfSpinCount] == 10);
  CHECK(tracks[0].mark[kHomingSparkMarkOfPhase] == kHomingSparkDriftPhase);
  // ⚠ LE MÊME TIC TRAVERSE LES DEUX PHASES : le client retombe sur la dérive
  // dès que le compteur a débordé, sans attendre le tic suivant. L'azimut, que
  // le tournoiement ne touche pas, gagne donc son premier degré AU TIC MÊME où
  // la dérive prend la main.
  CHECK(tracks[0].azimuth_degrees == 1);
  // Et l'inclinaison a fait dix pas de neuf, moins le premier de la dérive.
  CHECK(tracks[0].tilt_degrees == 87);
}

TEST_CASE("la derive ne s'eteint qu'au tic trois cents", "[render][homing_spark]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Spark();
  tracks[0].mark[kHomingSparkMarkOfPhase] = kHomingSparkDriftPhase;
  tracks[0].alpha = 50;
  tracks[0].ticks = 100;
  AdvanceHomingSparkTracks(tracks);
  CHECK(tracks[0].alpha == 50);
  CHECK(tracks[0].tilt_degrees == 357);
  CHECK(tracks[0].azimuth_degrees == 1);

  tracks[0].ticks = 300;
  AdvanceHomingSparkTracks(tracks);
  CHECK(tracks[0].alpha == 49);
}

TEST_CASE("l'alpha monte de cinq pendant vingt tics", "[render][homing_spark]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Spark();
  tracks[0].alpha = 0;
  tracks[0].ticks = 0;
  tracks[0].anchor = {60.0F, 0.0F, 0.0F};
  for (std::int32_t tick = 0; tick < 19; ++tick) {
    AdvanceHomingSparkTracks(tracks);
  }
  CHECK(tracks[0].ticks == 19);
  CHECK(tracks[0].alpha == 95);
  AdvanceHomingSparkTracks(tracks);
  CHECK(tracks[0].alpha == 95);
}

TEST_CASE("un retard negatif fait attendre la piste", "[render][homing_spark]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = Spark();
  tracks[0].ticks = -2;
  tracks[0].alpha = 0;
  tracks[0].anchor = {30.0F, 0.0F, 0.0F};
  AdvanceHomingSparkTracks(tracks);
  AdvanceHomingSparkTracks(tracks);
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].anchor.x == Approx(30.0F));
}

TEST_CASE("la table porte les quatre identifiants du sous-type 159", "[render][homing_spark]") {
  const std::span<const EffectHomingSparkPattern> table = EffectHomingSparkPatternTable();
  CHECK(table.size() == 4);
  CHECK(std::ranges::is_sorted(table, {}, &EffectHomingSparkPattern::effect_id));
  CHECK(PatternOf(882).mark == 0);
  CHECK(PatternOf(908).mark == 1);
  CHECK(PatternOf(909).mark == 2);
  CHECK(PatternOf(910).mark == 3);
  // Les quatre partagent leur image.
  CHECK(PatternOf(882).texture == PatternOf(910).texture);
  CHECK(EffectHomingSparkPatternsFor(1).empty());
}

TEST_CASE("le semis pose les quatre pistes sur une sphere", "[render][homing_spark]") {
  std::uint32_t seed = 17U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectHomingSparkTracks(PatternOf(908), seed);
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.alpha == 0);
    CHECK(track.ticks <= 0);
    CHECK(track.ticks >= -100);
    CHECK(track.radius_at_anchor == Approx(kHomingSparkBladeRadius));
    CHECK(track.mark[kHomingSparkMarkOfPhase] == kHomingSparkApproachPhase);
    CHECK(track.mark[kHomingSparkMarkOfColor] == 1);
    CHECK(track.value[kHomingSparkSlotOfLift] == Approx(kHomingSparkLift));
    // ⚠ L'ANCRE EST UN POINT DE LA SPHÈRE, décalé du même décalage vertical :
    // sa distance au centre décalé vaut le rayon.
    const glm::vec3 recentre = {track.anchor.x, track.anchor.y - kHomingSparkLift, track.anchor.z};
    const float radius =
        std::sqrt(recentre.x * recentre.x + recentre.y * recentre.y + recentre.z * recentre.z);
    CHECK(radius == Approx(kHomingSparkSphereRadius).margin(1e-2));
  }
  // Les quatre ne partent pas du même point.
  CHECK(tracks[0].anchor != tracks[1].anchor);
}

TEST_CASE("le noeud seme porte sa forme et son image", "[render][homing_spark]") {
  std::uint32_t seed = 4U;
  const ro::render::PooledEffectNode node =
      SowEffectHomingSparkNode(PatternOf(909), {1.0F, 0.0F, 2.0F}, 400, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kHomingSpark);
  CHECK(node.node.life_ticks == 400);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(PatternOf(909).texture));
  CHECK(node.tracks[0].mark[kHomingSparkMarkOfColor] == 2);
}

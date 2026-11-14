#include "ro/render/effect_lightning_fan.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_lightning_fan_seed.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceLightningFanTracks;
using ro::render::ComposeLightningFanQuads;
using ro::render::EffectLightningFanJoint;
using ro::render::EffectLightningFanPattern;
using ro::render::EffectLightningFanPatternsFor;
using ro::render::EffectLightningFanPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kLightningFanAlphaPerSegment;
using ro::render::kLightningFanBlue;
using ro::render::kLightningFanFaces;
using ro::render::kLightningFanFirstDrawnSegment;
using ro::render::kLightningFanGradientPerSegment;
using ro::render::kLightningFanJitterFloor;
using ro::render::kLightningFanJitterSpread;
using ro::render::kLightningFanJitterStep;
using ro::render::kLightningFanLastDrawnSegment;
using ro::render::kLightningFanRandomColorSweep;
using ro::render::kLightningFanSegments;
using ro::render::kLightningFanTailOfBow;
using ro::render::kLightningFanTailOfSegments;
using ro::render::kLightningFanTiltPerTrack;
using ro::render::kLightningFanTubeRadius;
using ro::render::SeedEffectLightningFanTracks;
using ro::render::SowEffectLightningFanNode;

namespace {

constexpr std::array<std::uint32_t, 3> kFanEffectIds = {432, 781, 888};

[[nodiscard]] const EffectLightningFanPattern& PatternOf(std::uint32_t effect_id) {
  return EffectLightningFanPatternsFor(effect_id).front();
}

// UNE PISTE DÉJÀ ÉCRITE : l'historique rempli par un premier tic de mouvement.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> WrittenTrack(std::uint32_t effect_id) {
  std::uint32_t seed = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectLightningFanTracks(PatternOf(effect_id), {0.0F, 0.0F, 0.0F}, seed);
  std::uint32_t rng = 9U;
  AdvanceLightningFanTracks(tracks, rng);
  for (EffectTrack& track : tracks) {
    track.alpha = 200;
  }
  return tracks;
}

}  // namespace

TEST_CASE("l'eclair rend trois faces par segment dessine", "[render][lightning_fan]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTrack(432);
  std::uint32_t rng = 3U;
  const std::vector<EffectWorldQuad> faces = ComposeLightningFanQuads(tracks[0], rng);
  // ⚠ DU PREMIER AU DIX-HUITIÈME SEGMENT, bornes comprises : dix-huit segments,
  // trois faces chacun. Le segment zéro ne se dessine PAS, et c'est ce qui
  // détache l'éclair de son nœud.
  const std::size_t dessines = kLightningFanLastDrawnSegment - kLightningFanFirstDrawnSegment + 1;
  CHECK(faces.size() == dessines * kLightningFanFaces);
}

TEST_CASE("l'eclair reste sur un sol eleve, il ne passe pas en miroir dessous",
          "[render][lightning_fan]") {
  // L'ancre est deja dans notre repere ; seuls les joints, calcules comme le
  // client, changent de signe. Les nier ensemble renvoyait l'eclair d'un sol a
  // cent unites vers moins cent.
  constexpr float kSol = 100.0F;
  std::uint32_t seed = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectLightningFanTracks(PatternOf(781), {0.0F, kSol, 0.0F}, seed);
  std::uint32_t rng = 9U;
  AdvanceLightningFanTracks(tracks, rng);
  tracks[0].alpha = 200;
  std::uint32_t draw = 3U;
  const std::vector<EffectWorldQuad> faces = ComposeLightningFanQuads(tracks[0], draw);
  REQUIRE_FALSE(faces.empty());
  for (const EffectWorldQuad& face : faces) {
    for (const glm::vec3& corner : face.corners) {
      CHECK(corner.y > kSol - 10.0F);
    }
  }
}

TEST_CASE("une piste eteinte, muette ou en retard ne rend rien", "[render][lightning_fan]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTrack(432);
  std::uint32_t rng = 3U;
  EffectTrack faded = tracks[0];
  faded.active = false;
  CHECK(ComposeLightningFanQuads(faded, rng).empty());
  EffectTrack silent = tracks[0];
  silent.alpha = 0;
  CHECK(ComposeLightningFanQuads(silent, rng).empty());
  EffectTrack delayed = tracks[0];
  delayed.ticks = 0;
  CHECK(ComposeLightningFanQuads(delayed, rng).empty());
}

TEST_CASE("une piste qui ne dessine rien CONSOMME quand meme son tremblement",
          "[render][lightning_fan]") {
  // ⚠ LE CLIENT TIRE EN TÊTE DE SON DESSINATEUR, hors du test de piste vive.
  // Sauter ces tirages décalerait la suite du générateur pour les trois autres
  // pistes du nœud.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTrack(432);
  EffectTrack faded = tracks[0];
  faded.active = false;
  std::uint32_t before = 42U;
  static_cast<void>(ComposeLightningFanQuads(faded, before));
  std::uint32_t witness = 42U;
  for (std::size_t draw = 0; draw < 3U * static_cast<std::size_t>(kLightningFanSegments); ++draw) {
    static_cast<void>(ro::render::NextEffectNodeDraw(witness));
  }
  CHECK(before == witness);
}

TEST_CASE("la figure TIRE : deux images ne se ressemblent pas", "[render][lightning_fan]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTrack(432);
  std::uint32_t rng = 3U;
  const std::vector<EffectWorldQuad> first = ComposeLightningFanQuads(tracks[0], rng);
  const std::vector<EffectWorldQuad> second = ComposeLightningFanQuads(tracks[0], rng);
  REQUIRE(first.size() == second.size());
  REQUIRE_FALSE(first.empty());
  // ⚠ C'EST LA SEULE FIGURE DU LOT QUI TIRE, et c'est mesuré : le client
  // re-tire son tremblement à chaque image. L'état de la piste n'a pas bougé.
  bool differe = false;
  for (std::size_t rang = 0; rang < first.size() && !differe; ++rang) {
    differe = first[rang].corners[0] != second[rang].corners[0];
  }
  CHECK(differe);
}

TEST_CASE("le tremblement reste dans ses bornes", "[render][lightning_fan]") {
  EffectTrack track;
  track.tail[kLightningFanTailOfSegments] = kLightningFanSegments;
  track.radius_at_anchor = 10.0F;
  const float top = static_cast<float>(kLightningFanJitterSpread - 1) * kLightningFanJitterStep +
                    kLightningFanJitterFloor;
  // À phase nulle et bombement nul, le joint n'est que son tremblement — plus
  // l'abaissement de deux.
  const glm::vec3 bottom = EffectLightningFanJoint(
      track, 0, {kLightningFanJitterFloor, kLightningFanJitterFloor, kLightningFanJitterFloor});
  const glm::vec3 vertex = EffectLightningFanJoint(track, 0, {top, top, top});
  CHECK(bottom.x == Approx(kLightningFanJitterFloor));
  CHECK(vertex.x == Approx(top));
  CHECK(bottom.y == Approx(kLightningFanJitterFloor - 2.0F));
}

TEST_CASE("la phase donne la distance et le bombement", "[render][lightning_fan]") {
  EffectTrack track;
  track.tail[kLightningFanTailOfSegments] = kLightningFanSegments;
  track.tail[kLightningFanTailOfBow] = 0.1F;
  track.radius_at_anchor = 10.0F;
  track.tilt_degrees = 0;
  // ⚠ QUATRE-VINGT-DIX DEGRÉS FRANCHISSENT UN RAYON, et le bombement y est au
  // plus haut.
  track.value[0] = 90.0F;
  const glm::vec3 middle = EffectLightningFanJoint(track, 0, {0.0F, 0.0F, 0.0F});
  CHECK(middle.x == Approx(10.0F));
  // Le bombement du client est NÉGATIF, et son repère descend.
  CHECK(middle.y < -2.0F);
  track.value[0] = 0.0F;
  // ⚠ UNE PHASE NULLE NE PARCOURT RIEN : le client teste le signe avant de
  // diviser.
  CHECK(EffectLightningFanJoint(track, 0, {0.0F, 0.0F, 0.0F}).x == Approx(0.0F));
}

TEST_CASE("l'alpha tombe de cinq par segment", "[render][lightning_fan]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = WrittenTrack(432);
  tracks[0].alpha = 200;
  std::uint32_t rng = 3U;
  const std::vector<EffectWorldQuad> faces = ComposeLightningFanQuads(tracks[0], rng);
  REQUIRE(faces.size() >= 2 * kLightningFanFaces);
  // Le premier segment dessiné est le PREMIER, pas le zéroième.
  CHECK(faces.front().tint[3] ==
        200 - kLightningFanAlphaPerSegment * kLightningFanFirstDrawnSegment);
  CHECK(faces[kLightningFanFaces].tint[3] ==
        200 - kLightningFanAlphaPerSegment * (kLightningFanFirstDrawnSegment + 1));
}

TEST_CASE("le degrade monte, et le balayage TROIS tire au sort", "[render][lightning_fan]") {
  std::array<EffectTrack, kEffectTracksPerNode> degrade = WrittenTrack(432);
  std::uint32_t rng = 3U;
  const std::vector<EffectWorldQuad> faces = ComposeLightningFanQuads(degrade[0], rng);
  REQUIRE(faces.size() >= 2 * kLightningFanFaces);
  CHECK(faces.front().tint[2] == kLightningFanBlue);
  CHECK(faces.front().tint[0] == kLightningFanGradientPerSegment * kLightningFanFirstDrawnSegment);
  CHECK(faces.front().tint[0] == faces.front().tint[1]);
  // ⚠ LE BLEU NE BOUGE PAS : l'éclair passe du sombre au blanc, jamais au
  // jaune.
  CHECK(faces[kLightningFanFaces].tint[0] > faces.front().tint[0]);

  std::array<EffectTrack, kEffectTracksPerNode> au_sort = WrittenTrack(781);
  REQUIRE(au_sort[0].degrees == kLightningFanRandomColorSweep);
  const std::vector<EffectWorldQuad> tirees = ComposeLightningFanQuads(au_sort[0], rng);
  REQUIRE_FALSE(tirees.empty());
  CHECK(tirees.front().tint[2] == kLightningFanBlue);
  // ⚠ LE ROUGE RECULE DE CINQUANTE-SIX et reboucle par le haut : il est donc
  // CLAIR, jamais sombre. Le lire comme un petit nombre l'aurait noirci.
  CHECK(tirees.front().tint[0] >= 200);
}

TEST_CASE("l'historique s'ecrit AU PREMIER TIC, et d'un coup", "[render][lightning_fan]") {
  std::uint32_t seed = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectLightningFanTracks(PatternOf(432), {0.0F, 0.0F, 0.0F}, seed);
  for (const float phase : tracks[0].value) {
    CHECK(phase == Approx(0.0F));
  }
  std::uint32_t rng = 9U;
  AdvanceLightningFanTracks(tracks, rng);
  // ⚠ LA TÊTE EST AU PLAFOND ET LA QUEUE À NEUF : vingt et un tours d'un coup.
  // L'étaler sur vingt et un tics ferait pousser l'éclair comme un serpent.
  CHECK(tracks[0].value[0] == Approx(180.0F));
  CHECK(tracks[0].value[1] == Approx(180.0F));
  CHECK(tracks[0].value[2] == Approx(171.0F));
  CHECK(tracks[0].value[20] == Approx(9.0F));
  // ET RIEN NE BOUGE ENSUITE.
  const float at_first = tracks[0].value[10];
  for (std::int32_t tick = 0; tick < 10; ++tick) {
    AdvanceLightningFanTracks(tracks, rng);
  }
  CHECK(tracks[0].value[10] == Approx(at_first));
}

TEST_CASE("le rayon enfle, et c'est LUI qui eteint l'eclair", "[render][lightning_fan]") {
  std::uint32_t seed = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectLightningFanTracks(PatternOf(432), {0.0F, 0.0F, 0.0F}, seed);
  CHECK(tracks[0].radius_at_anchor == Approx(0.0F));
  std::uint32_t rng = 9U;
  for (std::int32_t tick = 0; tick < 20; ++tick) {
    AdvanceLightningFanTracks(tracks, rng);
  }
  CHECK(tracks[0].radius_at_anchor == Approx(8.0F));
  const std::int16_t at_peak = tracks[0].alpha;
  CHECK(at_peak > 0);
  // Passé quinze de rayon, l'alpha se met à tomber ; passé le vingt-cinquième
  // tic, plus rien ne le relève.
  for (std::int32_t tick = 0; tick < 200; ++tick) {
    AdvanceLightningFanTracks(tracks, rng);
  }
  CHECK(tracks[0].alpha == 0);
  // ⚠ ET LA PISTE RESTE VIVE : ce mouvement n'éteint jamais rien, c'est la vie
  // du nœud qui l'efface.
  CHECK(tracks[0].active);
}

TEST_CASE("le semis tire UNE fois par piste et ecarte les quatre", "[render][lightning_fan]") {
  std::uint32_t seed = 17U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectLightningFanTracks(PatternOf(781), {1.0F, 2.0F, 3.0F}, seed);
  std::uint32_t witness = 17U;
  for (std::size_t draw = 0; draw < kEffectTracksPerNode; ++draw) {
    static_cast<void>(ro::render::NextEffectNodeDraw(witness));
  }
  CHECK(seed == witness);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].alpha == 0);
    CHECK(tracks[rang].radius_at_anchor == Approx(0.0F));
    CHECK(tracks[rang].lift_key == Approx(kLightningFanTubeRadius));
    CHECK(tracks[rang].tail[kLightningFanTailOfSegments] == Approx(kLightningFanSegments));
    CHECK(tracks[rang].tilt_degrees ==
          static_cast<std::int16_t>(PatternOf(781).base_tilt_degrees +
                                    kLightningFanTiltPerTrack * static_cast<std::int16_t>(rang)));
    // ⚠ L'ANCRE DE `_F0` EST ÉLEVÉE DE CINQ, et les deux autres non.
    CHECK(tracks[rang].anchor.y == Approx(2.0F + 5.0F));
  }
}

TEST_CASE("la table porte SIX lignes pour trois identifiants", "[render][lightning_fan]") {
  const std::span<const EffectLightningFanPattern> table = EffectLightningFanPatternTable();
  REQUIRE(table.size() == 6);
  for (const std::uint32_t each_effect_id : kFanEffectIds) {
    const std::span<const EffectLightningFanPattern> own_entries =
        EffectLightningFanPatternsFor(each_effect_id);
    // ⚠ DEUX ÉVENTAILS PAR IDENTIFIANT : l'un vers l'avant, l'autre vers
    // l'arrière.
    REQUIRE(own_entries.size() == 2);
    CHECK(own_entries[0].base_tilt_degrees == 0);
    CHECK(own_entries[1].base_tilt_degrees == 180);
    for (const EffectLightningFanPattern& pattern : own_entries) {
      CHECK_FALSE(pattern.texture.empty());
      CHECK(pattern.schedule.last_tick == 3);
      CHECK(pattern.schedule.period_ticks == 1);
    }
  }
  CHECK(PatternOf(781).sweep == kLightningFanRandomColorSweep);
  CHECK(PatternOf(781).raises_anchor);
  CHECK(PatternOf(432).sweep == 0);
  CHECK_FALSE(PatternOf(432).raises_anchor);
  CHECK(EffectLightningFanPatternsFor(1U).empty());
}

TEST_CASE("le noeud seme porte sa forme et son image", "[render][lightning_fan]") {
  std::uint32_t seed = 3U;
  const ro::render::PooledEffectNode node =
      SowEffectLightningFanNode(PatternOf(888), {2.0F, 0.0F, 7.0F}, 600, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kLightningFan);
  CHECK(node.node.life_ticks == 600);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(PatternOf(888).texture));
}

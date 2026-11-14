#include "ro/render/effect_spin_ribbon.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_spin_ribbon_seed.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceRisingSpinRibbonTracks;
using ro::render::AdvanceSpinRibbonTracks;
using ro::render::ComposeSpinRibbonQuads;
using ro::render::EffectSpinRibbonPattern;
using ro::render::EffectSpinRibbonPatternsFor;
using ro::render::EffectSpinRibbonPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kRisingSpinRibbonAlpha;
using ro::render::kRisingSpinRibbonDelaySpread;
using ro::render::kRisingSpinRibbonHalfWidthFloor;
using ro::render::kRisingSpinRibbonHeight;
using ro::render::kRisingSpinRibbonRadius;
using ro::render::kSpinRibbonAlpha;
using ro::render::kSpinRibbonHalfWidthFloor;
using ro::render::kSpinRibbonHeight;
using ro::render::kSpinRibbonRadius;
using ro::render::kSpinRibbonSlotOfHeight;
using ro::render::kSpinRibbonSlotOfSpinSpeed;
using ro::render::kSpinRibbonSpinFloor;
using ro::render::kSpinRibbonSpinSpread;
using ro::render::kSpinRibbonSteps;
using ro::render::kSpinRibbonSweepFloor;
using ro::render::kSpinRibbonSweepSpread;
using ro::render::kSpinRibbonTiltSpacing;
using ro::render::SeedEffectSpinRibbonTracks;
using ro::render::SowEffectSpinRibbonNode;

namespace {

constexpr std::uint32_t kSpinEffectId = 346;
constexpr std::uint32_t kRisingEffectId = 408;

[[nodiscard]] const EffectSpinRibbonPattern& PatternOf(std::uint32_t effect_id) {
  return EffectSpinRibbonPatternsFor(effect_id).front();
}

[[nodiscard]] EffectTrack LiveTrack(std::int16_t sweep_degrees = 180) {
  EffectTrack track;
  track.active = true;
  track.alpha = 100;
  track.degrees = sweep_degrees;
  track.lift_key = 4.0F;
  track.radius_at_anchor = kSpinRibbonRadius;
  track.value[kSpinRibbonSlotOfHeight] = kSpinRibbonHeight;
  track.value[kSpinRibbonSlotOfSpinSpeed] = 3.0F;
  return track;
}

}  // namespace

TEST_CASE("le ruban rend vingt quads sur un balayage rond", "[render][spin_ribbon]") {
  const std::vector<EffectWorldQuad> bands =
      ComposeSpinRibbonQuads(LiveTrack(180), 0, {0.0F, 0.0F, 0.0F});
  CHECK(bands.size() == static_cast<std::size_t>(kSpinRibbonSteps));
}

TEST_CASE("un balayage qui ne tombe pas juste en donne DAVANTAGE", "[render][spin_ribbon]") {
  // ⚠ LE PAS EST LE BALAYAGE DIVISÉ EN ENTIERS : cent quatre-vingt-dix-neuf
  // degrés par pas de neuf tiennent vingt-deux quads, pas vingt. C'est mesuré,
  // et le borner à vingt effacerait les échantillons que le client peint.
  const std::vector<EffectWorldQuad> bands =
      ComposeSpinRibbonQuads(LiveTrack(199), 0, {0.0F, 0.0F, 0.0F});
  CHECK(bands.size() > static_cast<std::size_t>(kSpinRibbonSteps));
}

TEST_CASE("une piste eteinte, muette ou trop courte ne rend rien", "[render][spin_ribbon]") {
  EffectTrack faded = LiveTrack();
  faded.active = false;
  CHECK(ComposeSpinRibbonQuads(faded, 0, {0.0F, 0.0F, 0.0F}).empty());
  EffectTrack silent = LiveTrack();
  silent.alpha = 0;
  CHECK(ComposeSpinRibbonQuads(silent, 0, {0.0F, 0.0F, 0.0F}).empty());
  EffectTrack retournee = LiveTrack(-1);
  CHECK(ComposeSpinRibbonQuads(retournee, 0, {0.0F, 0.0F, 0.0F}).empty());
  // ⚠ CE DERNIER CAS N'EST PAS DANS LE CLIENT : un balayage de moins de vingt
  // degrés y donne un pas nul, et sa boucle ne s'arrête jamais. Les deux
  // bâtisseurs tirent au-dessus de cent quatre-vingts.
  CHECK(ComposeSpinRibbonQuads(LiveTrack(19), 0, {0.0F, 0.0F, 0.0F}).empty());
}

TEST_CASE("les deux bords sont SYMETRIQUES autour du cercle", "[render][spin_ribbon]") {
  EffectTrack track = LiveTrack();
  track.tilt_degrees = 0;
  track.azimuth_degrees = 0;
  const std::vector<EffectWorldQuad> bands = ComposeSpinRibbonQuads(track, 0, {0.0F, 0.0F, 0.0F});
  REQUIRE_FALSE(bands.empty());
  // ⚠ C'EST CE QUI LE SÉPARE DE LA JUPE, dont un seul bord s'écarte : ici le
  // milieu des deux bords est exactement sur le cercle.
  const glm::vec3 first = bands.front().corners[1];
  const glm::vec3 second = bands.front().corners[2];
  const glm::vec3 middle = (first + second) * 0.5F;
  const float radius = std::sqrt(middle.x * middle.x + middle.z * middle.z);
  CHECK(radius == Approx(kSpinRibbonRadius).margin(0.01F));
  CHECK(middle.y == Approx(kSpinRibbonHeight).margin(0.01F));
  // À INCLINAISON NULLE, LA LARGEUR EST VERTICALE.
  CHECK(std::abs(first.y - second.y) == Approx(2.0F * track.lift_key).margin(0.01F));
  CHECK(first.x == Approx(second.x).margin(0.01F));
}

TEST_CASE("le ruban retourne sa profondeur avec son ordonnee", "[render][spin_ribbon]") {
  // ⚠ LE CHANGEMENT DE REPÈRE EST UN DEMI-TOUR AUTOUR DE X : à l'azimut de
  // quatre-vingt-dix degrés, où le client pose une profondeur de +rayon, la
  // nôtre vaut −rayon. La garder positive ferait tourner le ruban à l'envers.
  EffectTrack track = LiveTrack();
  track.tilt_degrees = 0;
  track.azimuth_degrees = 90;
  const std::vector<EffectWorldQuad> bands = ComposeSpinRibbonQuads(track, 0, {0.0F, 0.0F, 0.0F});
  REQUIRE_FALSE(bands.empty());
  const glm::vec3 start = (bands.front().corners[0] + bands.front().corners[3]) * 0.5F;
  CHECK(start.z == Approx(-kSpinRibbonRadius).margin(0.01F));
  CHECK(start.x == Approx(0.0F).margin(0.01F));
}

TEST_CASE("l'ancre entre dans chaque coin", "[render][spin_ribbon]") {
  const std::vector<EffectWorldQuad> a = ComposeSpinRibbonQuads(LiveTrack(), 0, {0.0F, 0.0F, 0.0F});
  const std::vector<EffectWorldQuad> b =
      ComposeSpinRibbonQuads(LiveTrack(), 0, {5.0F, 1.0F, -2.0F});
  REQUIRE(a.size() == b.size());
  CHECK(b.front().corners[0].x == Approx(a.front().corners[0].x + 5.0F));
  CHECK(b.front().corners[0].y == Approx(a.front().corners[0].y + 1.0F));
}

TEST_CASE("chaque piste tourne a SA vitesse", "[render][spin_ribbon]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    tracks[rang] = LiveTrack();
    tracks[rang].azimuth_degrees = 0;
    tracks[rang].value[kSpinRibbonSlotOfSpinSpeed] = 3.0F + static_cast<float>(rang);
  }
  AdvanceSpinRibbonTracks(tracks);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].azimuth_degrees == static_cast<std::int16_t>(3 + rang));
    CHECK(tracks[rang].ticks == 1);
  }
  // ⚠ ET IL NE TOUCHE NI L'ALPHA NI LE RAYON : le sous-type 77 ne fait que
  // tourner.
  CHECK(tracks[0].alpha == 100);
  CHECK(tracks[0].radius_at_anchor == Approx(kSpinRibbonRadius));
}

TEST_CASE("l'azimut reste dans le tour", "[render][spin_ribbon]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = LiveTrack();
  tracks[0].azimuth_degrees = 358;
  tracks[0].value[kSpinRibbonSlotOfSpinSpeed] = 6.0F;
  AdvanceSpinRibbonTracks(tracks);
  CHECK(tracks[0].azimuth_degrees == 4);
}

TEST_CASE("le ruban qui monte attend son tour, puis vit", "[render][spin_ribbon]") {
  std::uint32_t seed = 5U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpinRibbonTracks(PatternOf(kRisingEffectId), seed);
  REQUIRE(tracks[0].alpha == kRisingSpinRibbonAlpha);
  const float start_radius = tracks[0].radius_at_anchor;
  std::uint32_t rng = 9U;
  // ⚠ TANT QUE LE COMPTEUR EST NÉGATIF, LA PISTE NE BOUGE NI NE TIRE.
  const std::int32_t delay = -tracks[0].ticks;
  for (std::int32_t tick = 0; tick < delay; ++tick) {
    AdvanceRisingSpinRibbonTracks(tracks, rng);
  }
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].alpha == kRisingSpinRibbonAlpha);
  CHECK(tracks[0].radius_at_anchor == Approx(start_radius));
  for (std::int32_t tick = 0; tick < 25; ++tick) {
    AdvanceRisingSpinRibbonTracks(tracks, rng);
  }
  CHECK(tracks[0].ticks == 25);
  CHECK(tracks[0].alpha == 50);
  CHECK(tracks[0].radius_at_anchor > start_radius);
  // LE PLATEAU, du vingt-sixième au soixante-dixième tic.
  for (std::int32_t tick = 0; tick < 45; ++tick) {
    AdvanceRisingSpinRibbonTracks(tracks, rng);
  }
  CHECK(tracks[0].ticks == 70);
  CHECK(tracks[0].alpha == 50);
  AdvanceRisingSpinRibbonTracks(tracks, rng);
  CHECK(tracks[0].alpha == 48);
}

TEST_CASE("l'inclinaison du ruban qui monte avance d'un degre ou de deux",
          "[render][spin_ribbon]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  tracks[0] = LiveTrack();
  tracks[0].tilt_degrees = 0;
  tracks[0].ticks = 0;
  std::uint32_t rng = 3U;
  std::int16_t before = tracks[0].tilt_degrees;
  for (std::int32_t tick = 0; tick < 20; ++tick) {
    AdvanceRisingSpinRibbonTracks(tracks, rng);
    const std::int16_t step = static_cast<std::int16_t>(tracks[0].tilt_degrees - before);
    CHECK(step >= 1);
    CHECK(step <= 2);
    before = tracks[0].tilt_degrees;
  }
}

TEST_CASE("le semis ecarte les quatre pistes de quarante-cinq degres", "[render][spin_ribbon]") {
  std::uint32_t seed = 11U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpinRibbonTracks(PatternOf(kSpinEffectId), seed);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].active);
    CHECK(tracks[rang].tilt_degrees ==
          static_cast<std::int16_t>(kSpinRibbonTiltSpacing * static_cast<std::int16_t>(rang)));
    CHECK(tracks[rang].alpha == kSpinRibbonAlpha);
    CHECK(tracks[rang].ticks == 0);
    CHECK(tracks[rang].radius_at_anchor == Approx(kSpinRibbonRadius));
    CHECK(tracks[rang].value[kSpinRibbonSlotOfHeight] == Approx(kSpinRibbonHeight));
    CHECK(tracks[rang].degrees >= kSpinRibbonSweepFloor);
    CHECK(tracks[rang].degrees < kSpinRibbonSweepFloor + kSpinRibbonSweepSpread);
    CHECK(tracks[rang].lift_key >= kSpinRibbonHalfWidthFloor);
    CHECK(tracks[rang].value[kSpinRibbonSlotOfSpinSpeed] >= kSpinRibbonSpinFloor);
    CHECK(tracks[rang].value[kSpinRibbonSlotOfSpinSpeed] <
          kSpinRibbonSpinFloor + static_cast<float>(kSpinRibbonSpinSpread));
  }
}

TEST_CASE("le ruban qui monte naît invisible, petit et en retard", "[render][spin_ribbon]") {
  std::uint32_t seed = 11U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpinRibbonTracks(PatternOf(kRisingEffectId), seed);
  for (const EffectTrack& track : tracks) {
    CHECK(track.alpha == kRisingSpinRibbonAlpha);
    CHECK(track.radius_at_anchor == Approx(kRisingSpinRibbonRadius));
    CHECK(track.value[kSpinRibbonSlotOfHeight] == Approx(kRisingSpinRibbonHeight));
    CHECK(track.lift_key >= kRisingSpinRibbonHalfWidthFloor);
    CHECK(track.ticks <= 0);
    CHECK(track.ticks > -kRisingSpinRibbonDelaySpread);
  }
}

TEST_CASE("le semis du 91 tire UNE fois de plus par piste", "[render][spin_ribbon]") {
  // ⚠ LE RETARD EST LE PREMIER TIRAGE DU 91, et le 77 n'en fait pas : quatre
  // tirages par piste d'un côté, cinq de l'autre.
  std::uint32_t sans = 42U;
  static_cast<void>(SeedEffectSpinRibbonTracks(PatternOf(kSpinEffectId), sans));
  std::uint32_t witness = 42U;
  for (std::size_t draw = 0; draw < 4 * kEffectTracksPerNode; ++draw) {
    static_cast<void>(ro::render::NextEffectNodeDraw(witness));
  }
  CHECK(sans == witness);
  std::uint32_t with = 42U;
  static_cast<void>(SeedEffectSpinRibbonTracks(PatternOf(kRisingEffectId), with));
  for (std::size_t draw = 0; draw < kEffectTracksPerNode; ++draw) {
    static_cast<void>(ro::render::NextEffectNodeDraw(witness));
  }
  CHECK(with == witness);
}

TEST_CASE("la table porte les trois identifiants des sous-types 77 et 91",
          "[render][spin_ribbon]") {
  const std::span<const EffectSpinRibbonPattern> table = EffectSpinRibbonPatternTable();
  REQUIRE(table.size() == 3);
  CHECK(table[0].effect_id == 346);
  CHECK(table[1].effect_id == 394);
  CHECK(table[2].effect_id == 408);
  CHECK_FALSE(table[0].rising);
  CHECK_FALSE(table[1].rising);
  CHECK(table[2].rising);
  for (const EffectSpinRibbonPattern& pattern : table) {
    CHECK(pattern.base_tilt_degrees == 0);
    CHECK_FALSE(pattern.texture.empty());
  }
  CHECK(EffectSpinRibbonPatternsFor(1U).empty());
}

TEST_CASE("le noeud seme porte la forme de SON sous-type", "[render][spin_ribbon]") {
  std::uint32_t seed = 3U;
  const ro::render::PooledEffectNode rotated =
      SowEffectSpinRibbonNode(PatternOf(kSpinEffectId), {2.0F, 0.0F, 7.0F}, 600, seed);
  CHECK(rotated.shape == ro::render::PooledNodeShape::kSpinRibbon);
  CHECK(rotated.node.life_ticks == 600);
  REQUIRE(rotated.resource_paths.size() == 1);
  CHECK(rotated.resource_paths.front() == std::string(PatternOf(kSpinEffectId).texture));
  const ro::render::PooledEffectNode rise =
      SowEffectSpinRibbonNode(PatternOf(kRisingEffectId), {2.0F, 0.0F, 7.0F}, 600, seed);
  CHECK(rise.shape == ro::render::PooledNodeShape::kRisingSpinRibbon);
}

#include "ro/render/effect_expanding_ring.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <span>
#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_expanding_ring_seed.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_track_figures.h"
#include "ro/render/effect_track_motion.h"

#include "support/effect_bearers.h"

using Catch::Approx;
using ro::render::AdvanceExpandingRingTracks;
using ro::render::AdvanceGravityWaveTracks;
using ro::render::ComposeEffectTrackRibbon;
using ro::render::EffectExpandingRingPattern;
using ro::render::EffectExpandingRingPatternsFor;
using ro::render::EffectExpandingRingPatternTable;
using ro::render::EffectNodePool;
using ro::render::EffectNodeScheduleLastSowTick;
using ro::render::EffectNodeScheduleSowsAt;
using ro::render::EffectRibbonTurnOf;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTickMs;
using ro::render::kEffectTrackSlots;
using ro::render::kEffectTracksPerNode;
using ro::render::kExpandingRingAlphaCeiling;
using ro::render::kExpandingRingRenderWord;
using ro::render::kExpandingRingSchedule;
using ro::render::kExpandingRingTailOfBaseRadius;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectExpandingRingTracks;
using ro::render::SowEffectExpandingRingNode;

namespace {

using Tracks = std::array<EffectTrack, kEffectTracksPerNode>;

// L'UNIQUE IDENTIFIANT du sous-type 169, EF_BOTTOM_DE2.
constexpr std::uint32_t kBottomDe2EffectId = 939;

// Le facteur de destination INVSRCALPHA de Direct3D : la variante zéro du ruban.
constexpr std::int32_t kInvSourceAlpha = 6;

// Une vie assez longue pour qu'aucun essai ne la voie finir.
constexpr std::int32_t kLongLife = 29999;

[[nodiscard]] float SinDegrees(std::int32_t degrees) {
  return std::sin(static_cast<float>(degrees) * std::numbers::pi_v<float> / 180.0F);
}

// UNE PISTE CONNUE : allumée, clé 15, base 4, rayon 4, inclinaison 80.
[[nodiscard]] EffectTrack KnownTrack() {
  EffectTrack track;
  track.active = true;
  track.degrees = 360;
  track.lift_key = 15.0F;
  track.tilt_degrees = 80;
  track.radius_at_anchor = 4.0F;
  track.tail[kExpandingRingTailOfBaseRadius] = 4.0F;
  track.alpha = 50;
  return track;
}

}  // namespace

TEST_CASE("Anneau qui s'élargit : un seul identifiant, son image et sa variante",
          "[render][effectnode]") {
  const std::span<const EffectExpandingRingPattern> table = EffectExpandingRingPatternTable();
  REQUIRE(table.size() == 1);
  const std::span<const EffectExpandingRingPattern> patterns =
      EffectExpandingRingPatternsFor(kBottomDe2EffectId);
  REQUIRE(patterns.size() == 1);
  CHECK(patterns.front().texture == "data\\texture\\effect\\ring_blue.tga");
  CHECK(patterns.front().client_function == "Effect_Build_QuadRingExpand");
  // L'OCTET ZÉRO du site : blanc, en mélange ordinaire.
  CHECK(patterns.front().trail_variant == 0);
  CHECK(EffectExpandingRingPatternsFor(kBottomDe2EffectId + 1).empty());
}

TEST_CASE("Anneau qui s'élargit : quatre semis, aux tics zéro à trois", "[render][effectnode]") {
  // ⚠ LE BÂTISSEUR SÈME TANT QUE L'ÂGE NE DÉPASSE PAS TROIS, à chaque appel.
  for (std::int32_t tick = 0; tick <= 3; ++tick) {
    CHECK(EffectNodeScheduleSowsAt(kExpandingRingSchedule, tick));
  }
  CHECK_FALSE(EffectNodeScheduleSowsAt(kExpandingRingSchedule, 4));
  CHECK(EffectNodeScheduleLastSowTick(kExpandingRingSchedule) == 3);
}

TEST_CASE("Anneau qui s'élargit : le semis pose ses quatre pistes et l'âge entre dans le rayon",
          "[render][effectnode]") {
  constexpr std::array<float, kEffectTracksPerNode> kCles = {15.0F, 16.0F, 17.0F, 18.0F};
  constexpr std::array<float, kEffectTracksPerNode> kBases = {0.5F, 1.0F, 1.5F, 2.0F};
  for (std::int32_t age = 0; age <= 3; ++age) {
    std::uint32_t generator = 7U + static_cast<std::uint32_t>(age);
    const Tracks tracks = SeedEffectExpandingRingTracks(age, generator);
    for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
      const EffectTrack& track = tracks[rang];
      CHECK(track.active);
      CHECK(track.ticks == 0);
      CHECK(track.degrees == 360);
      CHECK(track.tilt_degrees == 80);
      CHECK(track.lift_key == Approx(kCles[rang]));
      CHECK(track.alpha >= 0);
      CHECK(track.alpha < 100);
      CHECK(track.azimuth_degrees >= 0);
      CHECK(track.azimuth_degrees < 360);
      // LA BASE : deux unités par tic d'âge, plus celle de la piste.
      const float base = (2.0F * static_cast<float>(age)) + kBases[rang];
      CHECK(track.tail[kExpandingRingTailOfBaseRadius] == Approx(base));
      // ⚠ L'ÉCART ALTERNE : retranché aux pistes paires, ajouté aux impaires,
      // et jamais plus de deux unités.
      const float offset = track.radius_at_anchor - base;
      if (rang % 2 == 0) {
        CHECK(offset <= 1e-4F);
        CHECK(offset >= -2.0F - 1e-4F);
      } else {
        CHECK(offset >= -1e-4F);
        CHECK(offset <= 2.0F + 1e-4F);
      }
      // En dixièmes entiers.
      const float dixiemes = std::abs(offset) * 10.0F;
      CHECK(dixiemes == Approx(std::round(dixiemes)).margin(1e-3F));
      for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
        CHECK(track.value[segment] == 0.0F);
        CHECK(track.mark[segment] == 0);
      }
    }
  }
}

TEST_CASE("Anneau qui s'élargit : le rayon croît de quatre centièmes, pas de sept",
          "[render][effectnode]") {
  // ⚠ C'EST CE QUI LE SÉPARE DU CORPS COMMUN, qu'on aurait pu croire le sien :
  // `AdvanceGravityWaveTracks` fait croître de sept centièmes.
  Tracks tracks{};
  tracks[0] = KnownTrack();
  Tracks commun = tracks;
  AdvanceExpandingRingTracks(tracks);
  AdvanceGravityWaveTracks(commun, kLongLife);
  CHECK(tracks[0].radius_at_anchor == Approx(4.04F));
  CHECK(commun[0].radius_at_anchor != Approx(tracks[0].radius_at_anchor));
  CHECK(tracks[0].ticks == 1);
  CHECK(tracks[0].tilt_degrees == 79);
  // Sous le seuil : l'alpha monte de dix.
  CHECK(tracks[0].alpha == 60);
}

TEST_CASE("Anneau qui s'élargit : l'alpha plafonne à deux cents", "[render][effectnode]") {
  Tracks tracks{};
  tracks[0] = KnownTrack();
  tracks[0].alpha = 195;
  AdvanceExpandingRingTracks(tracks);
  CHECK(tracks[0].alpha == kExpandingRingAlphaCeiling);
}

TEST_CASE("Anneau qui s'élargit : au-delà de sa base plus un, il s'efface de deux puis renaît",
          "[render][effectnode]") {
  Tracks tracks{};
  tracks[0] = KnownTrack();
  // Le seuil est PAR PISTE : base 4, donc 5 — non les 4 du corps commun.
  tracks[0].radius_at_anchor = 4.97F;
  tracks[0].alpha = 100;
  AdvanceExpandingRingTracks(tracks);
  CHECK(tracks[0].radius_at_anchor == Approx(5.01F));
  CHECK(tracks[0].alpha == 98);

  tracks[0].alpha = 2;
  tracks[0].tilt_degrees = 40;
  AdvanceExpandingRingTracks(tracks);
  // LA RENAISSANCE : rayon remis à la base, inclinaison soixante-quatorze.
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].radius_at_anchor == Approx(4.0F));
  CHECK(tracks[0].tilt_degrees == 74);
}

TEST_CASE("Anneau qui s'élargit : sous la base plus un, il ne s'efface pas même loin du centre",
          "[render][effectnode]") {
  // Une base de huit : le corps commun s'effacerait passé quatre, celui-ci non.
  Tracks tracks{};
  tracks[0] = KnownTrack();
  tracks[0].tail[kExpandingRingTailOfBaseRadius] = 8.0F;
  tracks[0].radius_at_anchor = 8.0F;
  AdvanceExpandingRingTracks(tracks);
  CHECK(tracks[0].alpha == 60);
}

TEST_CASE("Anneau qui s'élargit : le plancher d'inclinaison éteint l'alpha",
          "[render][effectnode]") {
  Tracks tracks{};
  tracks[0] = KnownTrack();
  tracks[0].tilt_degrees = 10;
  tracks[0].alpha = 150;
  AdvanceExpandingRingTracks(tracks);
  CHECK(tracks[0].tilt_degrees == 10);
  // Éteint par le plancher, PUIS rallumé de dix par la branche du rayon.
  CHECK(tracks[0].alpha == 10);
}

TEST_CASE("Anneau qui s'élargit : l'onde bat autour de la clé, deux formules de phase",
          "[render][effectnode]") {
  Tracks tracks{};
  for (EffectTrack& track : tracks) {
    track = KnownTrack();
  }
  tracks[3].lift_key = 18.0F;
  AdvanceExpandingRingTracks(tracks);
  // Piste 0 : phase = tic (1). Piste 3 : phase = (tic + 3·45)·2 = 272.
  for (std::size_t segment = 0; segment < kEffectTrackSlots; ++segment) {
    const auto le_long = static_cast<std::int32_t>(segment) * 9;
    const float expected0 = 15.0F + (15.0F * 0.3F * SinDegrees(le_long) * SinDegrees(1));
    const float expected3 = 18.0F + (18.0F * 0.3F * SinDegrees(le_long) * SinDegrees(272));
    CHECK(tracks[0].value[segment] == Approx(expected0).margin(1e-4F));
    CHECK(tracks[3].value[segment] == Approx(expected3).margin(1e-4F));
  }
}

TEST_CASE("Anneau qui s'élargit : une piste éteinte ne bouge pas", "[render][effectnode]") {
  Tracks tracks{};
  tracks[0] = KnownTrack();
  tracks[0].active = false;
  AdvanceExpandingRingTracks(tracks);
  CHECK(tracks[0].ticks == 0);
  CHECK(tracks[0].radius_at_anchor == Approx(4.0F));
}

TEST_CASE("Anneau qui s'élargit : le nœud semé porte sa forme, son mot de rendu et sa vie",
          "[render][effectnode]") {
  const EffectExpandingRingPattern& pattern =
      EffectExpandingRingPatternsFor(kBottomDe2EffectId).front();
  std::uint32_t generator = 3U;
  const glm::vec3 bearer{10.0F, 0.0F, -4.0F};
  const PooledEffectNode node =
      SowEffectExpandingRingNode(pattern, bearer, 2, kLongLife - 2, generator);
  CHECK(node.shape == PooledNodeShape::kExpandingRing);
  REQUIRE(node.node.render_word.has_value());
  CHECK(*node.node.render_word == kExpandingRingRenderWord);
  CHECK(node.node.life_ticks == kLongLife - 2);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(pattern.texture));
  CHECK(node.trail_variant == 0);
  CHECK(node.ground_position.x == Approx(10.0F));
  CHECK(node.owner_position.z == Approx(-4.0F));
  // Âge deux : la première piste part de 4,5.
  CHECK(node.tracks[0].tail[kExpandingRingTailOfBaseRadius] == Approx(4.5F));
}

TEST_CASE("Anneau qui s'élargit : le ruban peint dix quads blancs par piste allumée",
          "[render][effectnode]") {
  Tracks tracks{};
  for (EffectTrack& track : tracks) {
    track = KnownTrack();
  }
  AdvanceExpandingRingTracks(tracks);
  const std::vector<EffectWorldQuad> quads = ComposeEffectTrackRibbon(
      tracks[0], 0, glm::vec3{0.0F, 0.0F, 0.0F}, EffectRibbonTurnOf(tracks));
  REQUIRE(quads.size() == 10);
  for (const EffectWorldQuad& quad : quads) {
    CHECK(quad.tint[0] == 255);
    CHECK(quad.tint[1] == 255);
    CHECK(quad.tint[2] == 255);
    CHECK(quad.tint[3] == 60);
    CHECK(quad.blend_destination == kInvSourceAlpha);
  }
}

TEST_CASE("Anneau qui s'élargit : il suit son porteur qui marche", "[render][effectnode]") {
  EffectNodePool pool;
  const glm::vec3 bearer{0.0F, 0.0F, 0.0F};
  const EffectExpandingRingPattern& pattern =
      EffectExpandingRingPatternsFor(kBottomDe2EffectId).front();
  std::uint32_t generator = 5U;
  pool.AnchorNextTo(11U, bearer);
  pool.Add(SowEffectExpandingRingNode(pattern, bearer, 0, kLongLife, generator));
  pool.AnchorNextTo(0U);

  const glm::vec3 farther = bearer + glm::vec3{30.0F, 0.0F, -10.0F};
  ro::test_support::MoveBearer(pool, 11U, farther);
  pool.Advance(kEffectTickMs);
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes()[0].ground_position.x == Approx(30.0F).margin(0.001F));
  CHECK(pool.nodes()[0].ground_position.z == Approx(-10.0F).margin(0.001F));
}

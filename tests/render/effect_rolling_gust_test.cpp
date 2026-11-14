#include "ro/render/effect_rolling_gust.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_ground_gust.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_rolling_gust_seed.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceRollingGustTracks;
using ro::render::ComposeRollingGustQuad;
using ro::render::EffectRollingGustPattern;
using ro::render::EffectRollingGustPatternsFor;
using ro::render::EffectRollingGustPatternTable;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kGroundGustSlotOfSpeed;
using ro::render::kRollingGustColor;
using ro::render::kRollingGustCrossStep;
using ro::render::kRollingGustDelayTicks;
using ro::render::kRollingGustLiftAboveNode;
using ro::render::kRollingGustLiftSpread;
using ro::render::kRollingGustRadius;
using ro::render::kRollingGustSowingTicks;
using ro::render::kRollingGustStartSpeed;
using ro::render::RollingGustCrossOffset;
using ro::render::SeedEffectRollingGustTracks;
using ro::render::SowEffectRollingGustNode;

namespace {

using Tracks = std::array<EffectTrack, kEffectTracksPerNode>;

// L'IDENTIFIANT UNIQUE du sous-type 126 : Kamaitachi.
constexpr std::uint32_t kKamaitachiEffectId = 624;

// Le facteur de destination de l'addition, dans la numérotation de Direct3D.
constexpr std::int32_t kBlendFactorAdd = 2;

[[nodiscard]] EffectTrack LiveTrack() {
  EffectTrack track;
  track.active = true;
  track.alpha = 60;
  track.radius_at_anchor = kRollingGustRadius;
  track.anchor = {10.0F, 3.0F, -4.0F};
  return track;
}

}  // namespace

TEST_CASE("la roue est DEBOUT dans le plan de son cap", "[render][rolling_gust]") {
  EffectTrack track = LiveTrack();
  track.azimuth_degrees = 0;
  track.tilt_degrees = 0;
  const std::optional<EffectWorldQuad> wheel = ComposeRollingGustQuad(track);
  REQUIRE(wheel.has_value());
  // CAP ZÉRO : le plan de la roue est celui de x et de la hauteur, et aucun
  // coin ne sort de la profondeur de l'ancre.
  for (const glm::vec3& corner : wheel->corners) {
    CHECK(corner.z == Approx(track.anchor.z));
  }
  // ⚠ LE PREMIER COIN À UN QUART DE TOUR : tout en hauteur, rien le long du
  // cap. Et il est EN DESSOUS de l'ancre — le client l'ajoute à une ordonnée
  // qui descend.
  CHECK(wheel->corners[0].x == Approx(track.anchor.x).margin(0.001F));
  CHECK(wheel->corners[0].y == Approx(track.anchor.y - kRollingGustRadius));
  // Le deuxième, un demi-tour : à l'arrière du cap, à la hauteur de l'ancre.
  CHECK(wheel->corners[1].x == Approx(track.anchor.x - kRollingGustRadius));
  CHECK(wheel->corners[1].y == Approx(track.anchor.y).margin(0.001F));
}

TEST_CASE("le cap tourne le plan de la roue", "[render][rolling_gust]") {
  EffectTrack track = LiveTrack();
  constexpr std::int16_t kQuartDeTour = 90;
  track.azimuth_degrees = kQuartDeTour;
  const std::optional<EffectWorldQuad> wheel = ComposeRollingGustQuad(track);
  REQUIRE(wheel.has_value());
  for (const glm::vec3& corner : wheel->corners) {
    CHECK(corner.x == Approx(track.anchor.x).margin(0.001F));
  }
  CHECK(wheel->corners[1].z == Approx(track.anchor.z - kRollingGustRadius));
}

TEST_CASE("la roue est d'un vert ajoute, et son image retournee", "[render][rolling_gust]") {
  const std::optional<EffectWorldQuad> wheel = ComposeRollingGustQuad(LiveTrack());
  REQUIRE(wheel.has_value());
  CHECK(wheel->tint[0] == kRollingGustColor[0]);
  CHECK(wheel->tint[1] == kRollingGustColor[1]);
  CHECK(wheel->tint[2] == kRollingGustColor[2]);
  CHECK(wheel->tint[3] == 60);
  CHECK(wheel->blend_destination == kBlendFactorAdd);
  CHECK(wheel->texture_uv[0].y == Approx(1.0F));
  CHECK(wheel->texture_uv[2].y == Approx(0.0F));
}

TEST_CASE("une roue eteinte ou muette ne rend rien", "[render][rolling_gust]") {
  EffectTrack faded = LiveTrack();
  faded.active = false;
  CHECK_FALSE(ComposeRollingGustQuad(faded).has_value());
  EffectTrack silent = LiveTrack();
  silent.alpha = 0;
  CHECK_FALSE(ComposeRollingGustQuad(silent).has_value());
  // ⚠ SA MARQUE DE TEINTE N'Y FAIT RIEN : le dessinateur ne la lit pas.
  EffectTrack marquee = LiveTrack();
  marquee.mark[ro::render::kGroundGustMarkOfColor] = 7;
  CHECK(ComposeRollingGustQuad(marquee).has_value());
}

TEST_CASE("l'alpha de la roue monte de six jusqu'a soixante, puis tombe de trois",
          "[render][rolling_gust]") {
  std::uint32_t seed = 1U;
  Tracks tracks = SeedEffectRollingGustTracks({0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, 0, seed);
  // Trois tics d'attente, le tic un sans alpha, puis la montée.
  for (std::int32_t tick = 0; tick < kRollingGustDelayTicks + 2; ++tick) {
    AdvanceRollingGustTracks(tracks);
  }
  REQUIRE(tracks[0].ticks == 2);
  CHECK(tracks[0].alpha == 6);
  for (std::int32_t tick = 0; tick < 8; ++tick) {
    AdvanceRollingGustTracks(tracks);
  }
  REQUIRE(tracks[0].ticks == 10);
  CHECK(tracks[0].alpha == 54);
  AdvanceRollingGustTracks(tracks);
  CHECK(tracks[0].alpha == 51);
  // La vitesse enfle comme celle de la bourrasque.
  CHECK(tracks[0].value[kGroundGustSlotOfSpeed] > kRollingGustStartSpeed);
}

TEST_CASE("les cinq noeuds naissent sur une croix", "[render][rolling_gust]") {
  CHECK(RollingGustCrossOffset(0) == glm::vec3{0.0F, 0.0F, 0.0F});
  CHECK(RollingGustCrossOffset(1) == glm::vec3{kRollingGustCrossStep, 0.0F, 0.0F});
  CHECK(RollingGustCrossOffset(2) == glm::vec3{-kRollingGustCrossStep, 0.0F, 0.0F});
  // ⚠ LA PROFONDEUR DU CLIENT EST LA NÔTRE RETOURNÉE.
  CHECK(RollingGustCrossOffset(3) == glm::vec3{0.0F, 0.0F, -kRollingGustCrossStep});
  CHECK(RollingGustCrossOffset(4) == glm::vec3{0.0F, 0.0F, kRollingGustCrossStep});
}

TEST_CASE("le semis retarde chaque noeud de son tic, et tire deux fois par piste",
          "[render][rolling_gust]") {
  std::uint32_t seed = 9U;
  std::uint32_t witness = 9U;
  constexpr std::int32_t kThirdTick = 2;
  const Tracks tracks =
      SeedEffectRollingGustTracks({1.0F, 2.0F, 3.0F}, {0.0F, 0.0F, 1.0F}, kThirdTick, seed);
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    const EffectTrack& track = tracks[rang];
    CHECK(track.active);
    CHECK(track.alpha == 0);
    CHECK(track.ticks == static_cast<std::int32_t>(rang) - kRollingGustDelayTicks - kThirdTick);
    CHECK(track.radius_at_anchor == Approx(kRollingGustRadius));
    CHECK(track.value[kGroundGustSlotOfSpeed] == Approx(kRollingGustStartSpeed));
    CHECK(track.tilt_degrees == static_cast<std::int16_t>(rang));
    const auto descent = static_cast<float>(ro::render::NextEffectNodeDraw(witness) %
                                            static_cast<std::uint32_t>(kRollingGustLiftSpread));
    static_cast<void>(ro::render::NextEffectNodeDraw(witness));
    CHECK(track.anchor.y == Approx(2.0F + kRollingGustLiftAboveNode - descent));
    CHECK(track.anchor.x == Approx(1.0F - kRollingGustCrossStep));
    // Le cap vient de la cible, et PAS du tirage : vers +z, quatre-vingt-dix degrés.
    CHECK(track.azimuth_degrees == 90);
  }
  CHECK(seed == witness);
}

TEST_CASE("la table porte l'unique identifiant du sous-type 126", "[render][rolling_gust]") {
  const std::span<const EffectRollingGustPattern> table = EffectRollingGustPatternTable();
  REQUIRE(table.size() == 1);
  CHECK(table.front().effect_id == kKamaitachiEffectId);
  CHECK(table.front().client_function == "Effect_Build_ScatterCross7eFromParam");
  CHECK(EffectRollingGustPatternsFor(kKamaitachiEffectId).size() == 1);
  CHECK(EffectRollingGustPatternsFor(1U).empty());
}

TEST_CASE("le noeud seme vit d'autant plus longtemps qu'il nait tard", "[render][rolling_gust]") {
  std::uint32_t seed = 3U;
  const EffectRollingGustPattern& pattern = EffectRollingGustPatternTable().front();
  const ro::render::PooledEffectNode node = SowEffectRollingGustNode(
      pattern, {2.0F, 0.0F, 7.0F}, {1.0F, 0.0F, 0.0F}, kRollingGustSowingTicks - 1, 200, seed);
  CHECK(node.shape == ro::render::PooledNodeShape::kRollingGust);
  CHECK(node.node.life_ticks == 200 + kRollingGustSowingTicks - 1);
  REQUIRE(node.resource_paths.size() == 1);
  CHECK(node.resource_paths.front() == std::string(pattern.texture));
}

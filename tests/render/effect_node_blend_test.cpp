#include "ro/render/effect_node_blend.h"

#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_frame_beat.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/tick_blend.h"

#include "support/effect_bearers.h"

using Catch::Approx;
using ro::render::EffectNodeDrawnBetweenTicks;
using ro::render::EffectNodeDrawnPosition;
using ro::render::EffectNodeDrawnYawDegrees;
using ro::render::EffectNodeKinematics;
using ro::render::EffectNodePool;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;

namespace {

// Le pas d'un sprite lancé le long de l'abscisse, en unités par tic.
constexpr float kStepUnits = 2.0F;
// Une demi-image de seize millisecondes au-delà d'un tic entier.
constexpr std::uint32_t kTickAndAHalfMs = 24;
constexpr std::uint32_t kAnchorId = 7;

// Un sprite lancé depuis l'origine le long de l'abscisse.
PooledEffectNode Launched(PooledNodeShape shape = PooledNodeShape::kSprite) {
  PooledEffectNode pooled = ro::render::PooledNodeAt(shape, glm::vec3{0.0F, 0.0F, 0.0F});
  pooled.launch_direction = glm::vec3{1.0F, 0.0F, 0.0F};
  pooled.node.step_length = kStepUnits;
  return pooled;
}

// Une avance d'un tic et demi : un tic battu, une demi-image entamée.
void AdvanceTickAndAHalf(EffectNodePool& pool) {
  const std::int32_t ticks = pool.TakeDueTicks(kTickAndAHalfMs);
  REQUIRE(ticks == 1);
  pool.Pass(false);
  pool.FinishFrame();
}

}  // namespace

TEST_CASE("Nœud entre deux tics : un nœud né depuis le dernier tic se peint où il est",
          "[render][effectnode][blend]") {
  EffectNodePool pool;
  pool.Add(Launched());
  const PooledEffectNode& born = pool.nodes().front();
  CHECK_FALSE(born.previous_tick.has_value());
  CHECK(EffectNodeDrawnPosition(born, 0.5F) == born.ground_position);
}

TEST_CASE("Nœud entre deux tics : à mi-pas, un sprite lancé se peint au milieu de ses deux tics",
          "[render][effectnode][blend]") {
  EffectNodePool pool;
  pool.Add(Launched());
  AdvanceTickAndAHalf(pool);
  const float weight = pool.TickWeight(true);
  CHECK(weight == Approx(0.5F));

  const PooledEffectNode& flying = pool.nodes().front();
  REQUIRE(flying.previous_tick.has_value());
  CHECK(flying.previous_tick->ground_position.x == 0.0F);
  CHECK(flying.ground_position.x == Approx(kStepUnits));
  CHECK(EffectNodeDrawnPosition(flying, weight).x == Approx(kStepUnits / 2.0F));
  CHECK(EffectNodeDrawnPosition(flying, 0.0F) == flying.previous_tick->ground_position);
}

TEST_CASE("Nœud entre deux tics : sans l'option, l'état courant au bit près",
          "[render][effectnode][blend]") {
  EffectNodePool pool;
  pool.Add(Launched());
  AdvanceTickAndAHalf(pool);
  const float weight = pool.TickWeight(false);
  CHECK(weight == ro::render::kWholeTickWeight);

  const PooledEffectNode& flying = pool.nodes().front();
  CHECK(EffectNodeDrawnPosition(flying, weight) == flying.ground_position);
  CHECK(EffectNodeDrawnYawDegrees(flying, weight) == flying.node_yaw_degrees);
  const ro::render::EffectNode drawn = EffectNodeDrawnBetweenTicks(flying, weight);
  CHECK(drawn.alpha == flying.node.alpha);
  CHECK(drawn.sprite_scale == flying.node.sprite_scale);
  CHECK(drawn.spin_degrees == flying.node.spin_degrees);
  CHECK(drawn.color == flying.node.color);
}

TEST_CASE("Nœud entre deux tics : une traînée reste au tic, sa tête avec elle",
          "[render][effectnode][blend]") {
  EffectNodePool pool;
  PooledEffectNode trailed = Launched();
  ro::render::SeedEffectTrailHistory(trailed.trail, 3, trailed.node, trailed.ground_position);
  pool.Add(trailed);
  AdvanceTickAndAHalf(pool);
  const PooledEffectNode& flying = pool.nodes().front();
  CHECK_FALSE(flying.previous_tick.has_value());
  CHECK(EffectNodeDrawnPosition(flying, pool.TickWeight(true)) == flying.ground_position);
}

TEST_CASE("Nœud entre deux tics : un nœud qui bat à chaque image ne glisse pas",
          "[render][effectnode][blend]") {
  EffectNodePool pool;
  pool.SowFor(ro::render::kIncreaseAgilityEffect);
  pool.Add(Launched());
  AdvanceTickAndAHalf(pool);
  REQUIRE_FALSE(pool.nodes().empty());
  CHECK_FALSE(pool.nodes().front().previous_tick.has_value());
}

TEST_CASE("Nœud entre deux tics : le pas du porteur emporte aussi l'état du tic précédent",
          "[render][effectnode][blend]") {
  EffectNodePool pool;
  pool.AnchorNextTo(kAnchorId, glm::vec3{0.0F, 0.0F, 0.0F});
  pool.Add(Launched());
  AdvanceTickAndAHalf(pool);
  // Le porteur a marché de dix unités depuis le tic : la figure le suit en bloc,
  // et seul son propre pas glisse.
  constexpr float kBearerStepUnits = 10.0F;
  ro::test_support::MoveBearer(pool, kAnchorId, glm::vec3{kBearerStepUnits, 0.0F, 0.0F});
  const PooledEffectNode& carried = pool.nodes().front();
  CHECK(EffectNodeDrawnPosition(carried, 0.5F).x == Approx(kBearerStepUnits + kStepUnits / 2.0F));
}

TEST_CASE(
    "Nœud entre deux tics : le cap glisse par le plus court chemin, l'échelle et l'alpha "
    "à mi-chemin",
    "[render][effectnode][blend]") {
  PooledEffectNode pooled = Launched(PooledNodeShape::kOrientedQuad);
  pooled.node_yaw_degrees = 10.0F;
  pooled.node.alpha = 200.0F;
  pooled.node.sprite_scale = 3.0F;
  pooled.node.half_width.half_size = 8.0F;
  EffectNodeKinematics previous = ro::render::EffectNodeKinematicsOf(pooled);
  previous.yaw_degrees = 350.0F;
  previous.alpha = 100.0F;
  previous.sprite_scale = 1.0F;
  previous.half_width = 4.0F;
  pooled.previous_tick = previous;

  CHECK(EffectNodeDrawnYawDegrees(pooled, 0.5F) == Approx(360.0F));
  const ro::render::EffectNode drawn = EffectNodeDrawnBetweenTicks(pooled, 0.5F);
  CHECK(drawn.alpha == Approx(150.0F));
  CHECK(drawn.sprite_scale == Approx(2.0F));
  CHECK(drawn.half_width.half_size == Approx(6.0F));
  // Ce qui est discret ne glisse pas.
  CHECK(drawn.frame == pooled.node.frame);
  CHECK(drawn.action == pooled.node.action);
}

#include "ro/render/effect_node_blend.h"

#include <cstddef>

#include "ro/render/effect_node_pool.h"
#include "ro/render/tick_blend.h"

namespace ro::render {

bool EffectNodeBlendsBetweenTicks(const PooledEffectNode& pooled) noexcept {
  switch (pooled.shape) {
    case PooledNodeShape::kSprite:
    case PooledNodeShape::kGlowSprite:
      return pooled.trail.empty();
    case PooledNodeShape::kRingCylinder:
    case PooledNodeShape::kCrossedPlanes:
    case PooledNodeShape::kOrientedQuad:
      return true;
    default:
      return false;
  }
}

EffectNodeKinematics EffectNodeKinematicsOf(const PooledEffectNode& pooled) noexcept {
  const EffectNode& node = pooled.node;
  EffectNodeKinematics state;
  state.ground_position = pooled.ground_position;
  state.yaw_degrees = pooled.node_yaw_degrees;
  state.spin_degrees = node.spin_degrees;
  state.sprite_scale = node.sprite_scale;
  state.alpha = node.alpha;
  state.color = node.color;
  state.half_width = node.half_width.half_size;
  state.half_height = node.half_height.half_size;
  state.radius_at_anchor = node.radius_at_anchor.half_size;
  state.radius_at_height = node.radius_at_height.half_size;
  state.ring_radius = node.ring_radius.half_size;
  return state;
}

void RememberEffectNodeTick(PooledEffectNode& pooled, bool beats_every_frame) {
  if (beats_every_frame || !EffectNodeBlendsBetweenTicks(pooled)) {
    pooled.previous_tick.reset();
    return;
  }
  pooled.previous_tick = EffectNodeKinematicsOf(pooled);
}

glm::vec3 EffectNodeDrawnPosition(const PooledEffectNode& pooled, float weight) noexcept {
  if (!pooled.previous_tick.has_value()) {
    return pooled.ground_position;
  }
  const glm::vec3& previous = pooled.previous_tick->ground_position;
  const glm::vec3& current = pooled.ground_position;
  return {BlendScalar(previous.x, current.x, weight), BlendScalar(previous.y, current.y, weight),
          BlendScalar(previous.z, current.z, weight)};
}

float EffectNodeDrawnYawDegrees(const PooledEffectNode& pooled, float weight) noexcept {
  if (!pooled.previous_tick.has_value()) {
    return pooled.node_yaw_degrees;
  }
  return BlendDegrees(pooled.previous_tick->yaw_degrees, pooled.node_yaw_degrees, weight);
}

EffectNode EffectNodeDrawnBetweenTicks(const PooledEffectNode& pooled, float weight) {
  EffectNode node = pooled.node;
  if (!pooled.previous_tick.has_value()) {
    return node;
  }
  const EffectNodeKinematics& previous = *pooled.previous_tick;
  node.spin_degrees = BlendDegrees(previous.spin_degrees, node.spin_degrees, weight);
  node.sprite_scale = BlendScalar(previous.sprite_scale, node.sprite_scale, weight);
  node.alpha = BlendScalar(previous.alpha, node.alpha, weight);
  for (std::size_t channel = 0; channel < node.color.size(); ++channel) {
    node.color[channel] = BlendScalar(previous.color[channel], node.color[channel], weight);
  }
  node.half_width.half_size = BlendScalar(previous.half_width, node.half_width.half_size, weight);
  node.half_height.half_size =
      BlendScalar(previous.half_height, node.half_height.half_size, weight);
  node.radius_at_anchor.half_size =
      BlendScalar(previous.radius_at_anchor, node.radius_at_anchor.half_size, weight);
  node.radius_at_height.half_size =
      BlendScalar(previous.radius_at_height, node.radius_at_height.half_size, weight);
  node.ring_radius.half_size =
      BlendScalar(previous.ring_radius, node.ring_radius.half_size, weight);
  return node;
}

}  // namespace ro::render

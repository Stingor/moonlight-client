#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool.h"

namespace ro::test_support {

// UN SEUL PORTEUR QUI BOUGE : la liste de `MoveBearers`, réduite à lui.
inline void MoveBearer(render::EffectNodePool& pool, std::uint32_t actor_id,
                       glm::vec3 ground_position,
                       std::optional<float> owner_body_degrees = std::nullopt) {
  const render::EffectBearerPlace place{.actor_id = actor_id,
                                        .ground_position = ground_position,
                                        .owner_body_degrees = owner_body_degrees};
  pool.MoveBearers(std::span<const render::EffectBearerPlace>(&place, 1));
}

// Un nœud de cette troupe est-il encore accroché à cet acteur ?
[[nodiscard]] inline bool IsAnchoredTo(const render::EffectNodePool& pool, std::uint32_t actor_id) {
  return std::ranges::any_of(pool.nodes(), [actor_id](const render::PooledEffectNode& node) {
    return node.anchor_id == actor_id;
  });
}

}  // namespace ro::test_support

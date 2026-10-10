#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"

namespace moonclient {

// SÈME LES CARRÉS QUI MONTENT d'un effet dans la troupe, un nœud par patron.
//
// ⚠ UN NŒUD SANS IMAGE NE SE SÈME PAS : la figure peint l'image sur chaque
// carré, et un nœud sans elle vivrait des minutes sans se montrer. C'est
// l'appelant qui sait charger une image ; il le dit par `has_texture`.
bool SowRisingQuads(ro::render::EffectNodePool& pool, std::uint32_t effect_id,
                    glm::vec3 ground_position, std::int32_t effect_life_ticks,
                    std::uint32_t generator,
                    const std::function<bool(const std::string&)>& has_texture);

}  // namespace moonclient

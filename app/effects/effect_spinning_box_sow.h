#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"

namespace moonclient {

// SÈME LA PETITE BOÎTE QUI TOURNE d'un effet dans la troupe, une par patron.
//
// ⚠ UNE BOÎTE SANS IMAGE NE SE SÈME PAS : sa figure peint l'image sur chaque
// face, et un nœud sans elle vivrait vingt-neuf mille tics sans se montrer.
// C'est l'appelant qui sait charger une image ; il le dit par `has_texture`.
//
// La graine vient de l'appelant, comme pour toutes les familles : deux boîtes
// lancées ensemble ne tournent pas d'accord.
bool SowSpinningBoxes(ro::render::EffectNodePool& pool, std::uint32_t effect_id,
                      glm::vec3 ground_position, std::uint32_t generator,
                      const std::function<bool(const std::string&)>& has_texture);

}  // namespace moonclient

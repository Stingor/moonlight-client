#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"

namespace moonclient {

// SÈME L'EMPREINTE d'un effet dans la troupe, tournée de `azimuth_degrees`
// — l'angle, en degrés du client, que celui qui la sème a tiré de la marche.
// Un effet sans empreinte ne sème rien.
//
// ⚠ UNE EMPREINTE SANS IMAGE NE SE SÈME PAS : c'est l'appelant qui sait charger
// une image, et il le dit par `has_texture`.
bool SowFootprints(ro::render::EffectNodePool& pool, std::uint32_t effect_id,
                   glm::vec3 ground_position, float azimuth_degrees,
                   const std::function<bool(const std::string&)>& has_texture);

}  // namespace moonclient

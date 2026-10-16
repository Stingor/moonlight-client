#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "ro/mapedit/map_edit.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/point_lights.h"

namespace moonclient {

// LES LUMIÈRES PONCTUELLES DE L'IMAGE : celles du document, passées au monde,
// leur intensité de l'instant — une lumière « la nuit seulement » vaut
// `1 − daylight`, nulle à midi, pleine au cœur de la nuit ; du jour, c'est la
// composante la plus claire qui compte —, puis les plus proches de `eye`. Voir
// docs/adr/0119.
[[nodiscard]] ro::render::PointLightSet MapPointLights(
    const std::vector<ro::mapedit::AddedLight>& lights, const ro::render::MapEffectFrame& frame,
    glm::vec3 daylight, glm::vec3 eye);

}  // namespace moonclient

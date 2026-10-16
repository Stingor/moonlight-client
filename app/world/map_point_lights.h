#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "ro/mapedit/map_edit.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/point_lights.h"
#include "ro/render/scene_light.h"

namespace moonclient {

// LES LUMIÈRES PONCTUELLES DE L'IMAGE : celles du document, passées au monde,
// leur intensité de l'instant, puis les plus proches de `eye`. Voir
// docs/adr/0119.
//
// Une lumière « la nuit seulement » vaut `1 − jour`, où le jour est la
// composante la plus claire de `daylight × tint` : le ciel à l'heure ET la
// nuit que le serveur pose. Sur une carte fermée au ciel — intérieur, donjon,
// `always_night` —, elle brille pleinement : rien n'y fait le jour.
[[nodiscard]] ro::render::PointLightSet MapPointLights(
    const std::vector<ro::mapedit::AddedLight>& lights, const ro::render::MapEffectFrame& frame,
    const ro::render::SceneLight& sky, bool always_night, glm::vec3 eye);

}  // namespace moonclient

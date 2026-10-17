#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "ro/mapedit/map_edit.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/point_lights.h"
#include "ro/render/scene_light.h"

namespace ro::render {

// LES LUMIÈRES PONCTUELLES DE L'IMAGE : celles du document, passées au monde,
// leur intensité de l'instant, puis les plus proches de `eye`. Voir
// docs/adr/0119.
//
// Une lumière « la nuit seulement » vaut `1 − jour`, où le jour est la
// composante la plus claire de `daylight × tint` : le ciel à l'heure ET la
// nuit que le serveur pose. Sur une carte fermée au ciel — intérieur, donjon,
// `always_night` —, elle brille pleinement : rien n'y fait le jour.
[[nodiscard]] PointLightSet MapPointLights(const std::vector<ro::mapedit::AddedLight>& lights,
                                           const MapEffectFrame& frame, const SceneLight& sky,
                                           bool always_night, glm::vec3 eye, double time_ms = 0.0);

// TOUTES LES LUMIÈRES DU DOCUMENT : les siennes, puis la lueur de chaque effet
// qui en porte une, à sa place, avec la règle de nuit de l'effet.
[[nodiscard]] std::vector<ro::mapedit::AddedLight> DocumentLights(const ro::mapedit::MapEdit& edit);

// LE VACILLEMENT d'une lumière à l'instant `time_ms`, facteur de son intensité
// entre `1 − kFlickerDepth` et 1 : deux ondes de périodes premières entre
// elles, décalées par le rang `slot` pour que deux torches voisines ne
// battent pas ensemble. Déterministe : la même image donne la même lumière.
inline constexpr float kFlickerDepth = 0.35F;
[[nodiscard]] float FlickerFactor(std::size_t slot, double time_ms);

}  // namespace ro::render

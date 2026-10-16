#pragma once

#include <array>
#include <cstddef>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/ground_pick.h"
#include "ro/render/map_scene.h"

namespace ro::render {

// LE MODÈLE DU DÉCOR SOUS UN RAYON D'ÉCRAN, que l'établi désigne d'un clic.
//
// Chaque pose dessinée porte sa boîte orientée : l'emprise de son maillage,
// portée par sa matrice de pose. Parmi les boîtes que le rayon traverse devant
// l'œil, la PLUS PETITE gagne : un petit objet posé sur une falaise, ou adossé
// à une maison, se désigne sans que la grande boîte qui l'englobe ne le cache.
// Le résultat est le RANG du modèle dans le RSW ; rien quand le rayon ne
// traverse aucune boîte.
//
// Les poses masquées ne se dessinent pas, et ne se désignent donc pas.
[[nodiscard]] std::optional<std::size_t> PickSceneModel(const MapScene& scene,
                                                        const ScreenRay& ray);

// LA POSE DESSINÉE D'UN MODÈLE, retrouvée par son rang dans le RSW : son
// maillage et sa matrice. Rien pour un modèle masqué, hors de la carte, ou dont
// le fichier ne s'est pas lu.
struct ScenePose {
  const ModelMesh* mesh = nullptr;
  glm::mat4 transform{1.0F};
};
[[nodiscard]] std::optional<ScenePose> FindScenePose(const MapScene& scene, std::size_t rsw_rank);

// Les huit coins de la boîte d'une pose, dans le monde : l'emprise de son
// maillage, portée par sa matrice. Le coin `n` prend le haut de l'axe x quand
// son bit 1 est levé, de l'axe y pour le bit 2, de l'axe z pour le bit 4.
[[nodiscard]] std::array<glm::vec3, 8> PoseCorners(const ScenePose& pose);

}  // namespace ro::render

#pragma once

#include <cstddef>
#include <optional>

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

}  // namespace ro::render

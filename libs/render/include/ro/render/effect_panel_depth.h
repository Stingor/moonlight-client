#pragma once

#include <array>

#include <glm/glm.hpp>

namespace ro::render {

// LA PROFONDEUR D'UN PANNEAU D'EFFET, celle qui le fait cacher par le décor
// sans que le sol le coupe.
//
// Un panneau d'effet fait face à l'écran, ancré en l'air au-dessus de son point
// au sol. La caméra plonge : ce que le panneau montre SOUS le niveau de ce
// point recouvre à l'écran du terrain plus proche que lui, et ce terrain
// mangerait sa moitié basse. Le panneau prend donc UNE profondeur pour ses
// quatre coins : la plus proche du terrain plat qu'il recouvre, moins une
// marge. Le sol ne l'entame plus ; un arbre, un mur ou une statue qui se
// dressent devant lui le cachent.
//
// L'écran ne bouge pas : chaque coin glisse le long de son propre rayon de
// visée. Ce qui en sort est la position dont le renderer tire la profondeur.

// La marge prise sur le terrain, en unités du monde : une demi-cellule, pour
// qu'une pente douce sous l'effet ne l'entame pas non plus.
inline constexpr float kEffectPanelGroundClearance = 2.5F;

// `forward` est l'axe de visée, unitaire ; `ground` le point au sol sous
// l'ancre.
[[nodiscard]] std::array<glm::vec3, 4> EffectPanelDepthCorners(
    const std::array<glm::vec3, 4>& corners, glm::vec3 ground, glm::vec3 eye, glm::vec3 forward);

}  // namespace ro::render

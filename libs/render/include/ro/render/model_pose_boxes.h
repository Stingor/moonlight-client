#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "ro/render/model_mesh.h"
#include "ro/render/model_veil.h"
#include "ro/render/world_frustum.h"

namespace ro::render {

// L'EMPRISE DE CHAQUE POSE D'UN MODÈLE DU DÉCOR, dans le monde : de quoi ne
// peindre dans une carte d'ombre ponctuelle que les poses que voit sa face.
struct ModelPoseBoxes {
  // Une par pose, dans l'ordre de `ModelOpacities` : les figées, puis les
  // animées groupe par groupe.
  std::vector<WorldBox> boxes;
  std::size_t still_poses = 0;
};

// UNE POSE ANIMÉE BOUGE SES NŒUDS hors de l'emprise de son image zéro : la
// sienne s'élargit de cette part de son étendue, de chaque côté.
inline constexpr float kAnimatedPoseReachShare = 0.25F;

[[nodiscard]] ModelPoseBoxes PoseBoxesOf(const ModelInstances& model);

// LES POSES QUE VOIT `sight` ET QUI PORTENT OMBRE, écrites dans `into` à la
// façon de `ModelOpacities` : pleines, les autres à zéro. `casting` dit celles
// qui portent ombre ; nul, toutes. `boxes` est indexé par modèle, comme
// `casting`. Rend vrai quand une pose animée en est : ce que la face voit
// alors bouge d'une image à l'autre.
bool SightedCasting(std::span<const ModelPoseBoxes> boxes, const ModelOpacities* casting,
                    const ShadowSight& sight, ModelOpacities& into);

}  // namespace ro::render

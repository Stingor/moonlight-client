#pragma once

#include <glm/glm.hpp>

#include "ro/formats/rsw.h"

namespace ro::render {

// Éclairage global, prêt pour le shader.
struct SceneLight {
  // Direction VERS la source, normalisée.
  glm::vec3 direction{0.0F, 1.0F, 0.0F};
  glm::vec3 ambient{1.0F, 1.0F, 1.0F};
  glm::vec3 diffuse{1.0F, 1.0F, 1.0F};
  // Ce que la lumière calculée reçoit en facteur, canal par canal : la nuit
  // d'une carte — voir `DayNightLight`. Un, rien ne change.
  glm::vec3 tint{1.0F, 1.0F, 1.0F};
};

// LA LUMIÈRE DES MODÈLES D'UNITÉ AU SOL — un piège, un leurre : FIXE, la même
// sur toute carte, et non celle du RSW. Diffuse pleine, ambiante moitié, et une
// direction que le client ne normalise pas — (0,5 ; 0,7 ; 0,5) dans son repère,
// dont la hauteur et la profondeur sont les opposées des nôtres.
inline const SceneLight kGroundUnitModelLight{.direction = {0.5F, -0.7F, -0.5F},
                                              .ambient = {0.5F, 0.5F, 0.5F},
                                              .diffuse = {1.0F, 1.0F, 1.0F}};

// Convertit la lumière déclarée par le RSW.
//
// La direction se construit en faisant subir à l'axe vertical deux rotations
// NÉGATIVES : la latitude autour de X, puis la longitude autour de Y. Les signes
// viennent de GndRenderer.cs et ne s'improvisent pas — les inverser éclaire la
// carte depuis le mauvais côté, ce qui ne se voit que sur les reliefs.
[[nodiscard]] SceneLight MakeSceneLight(const formats::RswLight& light);

}  // namespace ro::render

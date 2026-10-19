#include "ro/render/scene_light.h"

#include <glm/gtc/matrix_transform.hpp>

namespace ro::render {

SceneLight MakeSceneLight(const formats::RswLight& light) {
  SceneLight scene_light;
  scene_light.ambient = {light.ambient[0], light.ambient[1], light.ambient[2]};
  scene_light.diffuse = {light.diffuse[0], light.diffuse[1], light.diffuse[2]};

  // L'ORDRE compte, et se tromper ne se voit pas : la longitude devient
  // simplement sans effet.
  //
  // On part de l'axe vertical. La latitude l'incline d'abord — sinon la
  // longitude ferait tourner l'axe Y autour de Y, c'est-à-dire rien du tout, et
  // toutes les cartes recevraient la même lumière quelle que soit leur
  // longitude déclarée. glm::rotate compose à DROITE, donc la première rotation
  // écrite est la dernière appliquée : la longitude s'écrit en premier.
  //
  // ⚠ Les SIGNES ne se déduisent pas du calcul de la référence : ses normales
  // de sol pointent vers le bas, elle compose ses rotations dans l'autre
  // convention, et elle n'emploie pas les mêmes pour le sol et pour les
  // modèles. Elle ne peut donc pas trancher.
  //
  // Ils ont été MESURÉS, en confrontant quatre modèles de gonryun à une capture
  // du client, chacun ramené à sa propre texture brute. Écart moyen au client,
  // sur les quatre combinaisons possibles :
  //
  //     latitude +  longitude -    0,010   <- retenue
  //     latitude +  longitude +    0,046
  //     latitude -  longitude -    0,219
  //     latitude -  longitude +    0,229
  //
  // Le signe de la latitude décide du CÔTÉ où se tient le soleil. Avec le
  // mauvais, les façades exposées sortent sombres et celles à l'ombre claires —
  // le défaut que montrait chaque capture, et qu'aucun réglage de l'intensité
  // ne pouvait corriger.
  glm::mat4 rotation{1.0F};
  rotation = glm::rotate(rotation, -glm::radians(static_cast<float>(light.longitude)),
                         glm::vec3{0.0F, 1.0F, 0.0F});
  rotation = glm::rotate(rotation, glm::radians(static_cast<float>(light.latitude)),
                         glm::vec3{1.0F, 0.0F, 0.0F});
  scene_light.direction = glm::normalize(glm::vec3(rotation * glm::vec4{0.0F, 1.0F, 0.0F, 0.0F}));
  return scene_light;
}

SceneLight SkyBoundLight(SceneLight light) {
  light.ambient = glm::min(light.ambient, glm::vec3(kBrightestSkyAmbient));
  return light;
}

}  // namespace ro::render

#pragma once

#include <array>
#include <cstddef>
#include <span>

#include <glm/glm.hpp>

namespace ro::render {

// UNE LUMIÈRE PONCTUELLE de l'instant, telle que le terrain et le décor la
// reçoivent.
//
// Elle s'éteint en douceur avec la distance : pleine en son centre, nulle à
// `range_world`. Voir `kPointLightFragmentGlsl`.
struct PointLight {
  glm::vec3 world_position{0};
  // Déjà multipliée par son intensité de l'instant : de zéro à un par canal,
  // davantage pour une lumière qui sature.
  glm::vec3 color{0};
  // Le rayon où elle s'éteint, en unités du monde.
  float range_world = 0;
};

// Le plafond des lumières qu'un dessin reçoit : la taille des tableaux
// d'uniformes des nuanceurs.
inline constexpr std::size_t kMaxPointLights = 8;

// Les lumières d'un dessin : les `count` premières de `lights` comptent.
struct PointLightSet {
  std::array<PointLight, kMaxPointLights> lights{};
  std::size_t count = 0;
};

// Les kMaxPointLights lumières de `lights` les plus proches de `eye`, plus
// proches d'abord. Une lumière sans couleur ou sans portée n'éclaire rien :
// elle ne prend pas de place.
[[nodiscard]] PointLightSet NearestPointLights(std::span<const PointLight> lights, glm::vec3 eye);

}  // namespace ro::render

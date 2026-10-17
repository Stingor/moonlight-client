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
  // CE QUE SON OMBRE LUI RETIRE, de zéro à un. Un : l'ombre éteint toute sa
  // lumière, comme celle d'une flamme. Moins : une part passe, comme autour
  // d'une source large qu'un obstacle ne cache jamais tout entière.
  float shadow_depth = 1.0F;
  // LA PÉNOMBRE de son ombre : la demi-largeur du carré où la carte se lit, en
  // part de la distance à la lumière. Zéro : le bord le plus net que la carte
  // permet.
  float shadow_spread = 0.0F;
};

// Le plafond des lumières qu'un dessin reçoit : la taille des tableaux
// d'uniformes des nuanceurs. Assez pour une place éclairée de lanternes ET les
// sorts qui s'y lancent : les `kMaxEffectLights` des sorts n'y chassent pas
// les lumières du document tant qu'elles tiennent dans le reste. Un pixel ne
// paie que les lumières de l'image, pas le plafond.
inline constexpr std::size_t kMaxPointLights = 16;

class PointShadowMaps;

// Les lumières d'un dessin : les `count` premières de `lights` comptent.
struct PointLightSet {
  std::array<PointLight, kMaxPointLights> lights{};
  std::size_t count = 0;
  // LEURS OMBRES : les `shadowed_count` premières lumières lisent chacune la
  // carte cubique de même rang dans `shadow_maps`. Sans carte, aucune lumière
  // n'est masquée. Voir `DrawPointShadows`.
  const PointShadowMaps* shadow_maps = nullptr;
  std::size_t shadowed_count = 0;
};

// Les kMaxPointLights lumières de `lights` les plus proches de `eye`, plus
// proches d'abord. Une lumière sans couleur ou sans portée n'éclaire rien :
// elle ne prend pas de place.
[[nodiscard]] PointLightSet NearestPointLights(std::span<const PointLight> lights, glm::vec3 eye);

// LA LUMIÈRE QUE `set` DÉPOSE EN `world_position` : la somme, par source, de sa
// couleur atténuée par le carré de ce qui reste de sa portée. C'est la règle de
// `PointLightingOf` dans `kPointLightGlsl`, évaluée une fois pour un point
// entier. Une source sans portée n'éclaire rien. ⚠ SANS OMBRE : la carte qui
// masquerait une source ne se lit que sur le GPU.
[[nodiscard]] glm::vec3 PointLightingAt(const PointLightSet& set, glm::vec3 world_position);

}  // namespace ro::render

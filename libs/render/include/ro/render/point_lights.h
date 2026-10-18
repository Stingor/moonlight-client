#pragma once

#include <cstdint>
#include <span>
#include <vector>

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
  // FAUX : elle n'a pas de carte d'ombre, et n'en prend aucune aux autres.
  bool casts_shadow = true;
};

// LA CARTE D'OMBRE D'UNE LUMIÈRE : sa couche dans le tableau de cubes de son
// palier. Un palier négatif : la lumière éclaire sans ombre. Voir
// `PointShadowSlots`.
inline constexpr std::int32_t kNoPointShadowTier = -1;
struct PointShadowSlot {
  std::int32_t tier = kNoPointShadowTier;
  std::uint32_t layer = 0;
};

class PointLightStage;

// LES LUMIÈRES D'UN DESSIN, sans borne de nombre. Un pixel ne paie que celles
// dont la portée l'atteint.
struct PointLightSet {
  std::vector<PointLight> lights;
  // LEURS OMBRES, une par lumière dans le même ordre, ou aucune : sans elles,
  // aucune lumière n'est masquée. Voir `DrawPointShadows`.
  std::vector<PointShadowSlot> shadows;
  // CE QUE LES NUANCEURS LISENT : les lumières téléversées et leurs cartes.
  // Sans lui, un dessin ne reçoit aucune lumière ponctuelle.
  const PointLightStage* stage = nullptr;
};

// Les lumières de `lights` rangées de la plus proche de `eye` à la plus
// lointaine. Une lumière sans couleur ou sans portée n'éclaire rien : elle ne
// prend pas de place.
[[nodiscard]] PointLightSet PointLightsByDistance(std::span<const PointLight> lights,
                                                  glm::vec3 eye);

// Les lumières de `set` dont la portée touche ce que voit `view_projection`,
// dans le même ordre : les autres n'éclairent aucun pixel de l'image.
[[nodiscard]] PointLightSet VisiblePointLights(const PointLightSet& set,
                                               const glm::mat4& view_projection);

// LA LUMIÈRE QUE `set` DÉPOSE EN `world_position` : la somme, par source, de sa
// couleur atténuée par le carré de ce qui reste de sa portée. C'est la règle de
// `PointLightingOf` dans `kPointLightGlsl`, évaluée une fois pour un point
// entier. Une source sans portée n'éclaire rien. ⚠ SANS OMBRE : la carte qui
// masquerait une source ne se lit que sur le GPU.
[[nodiscard]] glm::vec3 PointLightingAt(const PointLightSet& set, glm::vec3 world_position);

}  // namespace ro::render

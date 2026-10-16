#pragma once

#include <array>
#include <cmath>
#include <numbers>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/sprite_frame.h"

namespace ro::render {

// Combien de pixels de sprite tiennent dans une unité du monde : SEPT.
//
// C'est le client qui le dit. Il projette l'origine de l'acteur à l'écran et
// grandit ses quads — en pixels d'ÉCRAN — d'une échelle tirée de la
// profondeur : la focale de la projection, fois l'inverse de la profondeur,
// DIVISÉ PAR SEPT. Or la focale sur la profondeur est
// exactement ce qu'une unité du monde occupe à l'écran à cet endroit : un
// pixel de sprite vaut donc un septième d'unité, quelle que soit la caméra —
// et un panneau de cette taille, posé dans le monde, se projette comme le
// client dessine.
//
// Un cube du GND vaut kWorldUnitsPerCube unités et couvre deux cellules : une
// cellule fait cinq unités, soit trente-cinq pixels de sprite, et un
// personnage d'une soixantaine de pixels en occupe presque deux. À dix pixels
// par unité, calibrés à l'œil, tout le monde était trop petit de trois
// dixièmes.
inline constexpr float kSpritePixelsPerWorldUnit = 7.0F;

// Les axes dans lesquels s'inscrit un panneau, plus la visée.
//
// La visée ne sert pas à la FORME du panneau — elle lui est perpendiculaire —
// mais à sa PROFONDEUR : voir BillboardDepthCorners.
struct BillboardBasis {
  glm::vec3 right{1.0F, 0.0F, 0.0F};
  glm::vec3 up{0.0F, 1.0F, 0.0F};
  glm::vec3 forward{0.0F, 0.0F, -1.0F};
};

// De combien un sprite passe DEVANT le sol qu'il touche, en unités du monde.
//
// Sans cette marge, le bas d'un sprite et le sol sur lequel il pose sont à la
// même distance : le tampon de profondeur les départage au hasard, et le
// personnage se déchire au ras du sol. La marge vaut un dixième de cellule —
// assez pour trancher, trop peu pour faire passer un acteur devant un obstacle
// qu'il devrait avoir devant lui.
inline constexpr float kSpriteGroundClearance = 0.5F;

// DE COMBIEN LE HAUT D'UN SPRITE — ce qui se dresse au-dessus de son origine —
// passe DEVANT son plan, en unités du monde. Le jeu avance chaque calque d'un
// acteur d'au moins autant vers la caméra ; c'est ce qui garde entier un
// personnage debout devant un décor qui ne fait que l'effleurer, comme les
// pieux d'un brasier. Le bas, posé sur le sol, garde la marge du sol.
inline constexpr float kSpriteStandingClearance = 2.5F;

// Les axes d'un panneau face à la caméra.
//
// Le panneau est aligné sur l'ÉCRAN, pas seulement pivoté autour de la
// verticale. C'est ce qui reproduit le trait le plus visible de RO : un
// personnage ne s'écrase jamais, quelle que soit l'inclinaison de la caméra.
// Un panneau seulement pivoté se raccourcirait à mesure qu'on plonge, et les
// personnages se coucheraient au sol vus de haut.
[[nodiscard]] BillboardBasis MakeBillboardBasis(const Camera& camera);

// La part minimale de sa largeur que le panneau d'ombre montre au soleil : le
// cosinus de l'angle entre sa face et la trace du soleil au sol, ici 60°.
inline constexpr float kShadowPanelMinimumSunFacing = 0.5F;

// LE PANNEAU QUI PORTE L'OMBRE D'UN SPRITE : debout sur les pieds, et parallèle
// à l'ÉCRAN. Son bord bas suit la ligne des pieds telle que la caméra la
// montre, si bien que l'ombre portée est l'image du sprite cisaillée depuis
// ses pieds. Quand le soleil rase ce plan, l'ombre se réduirait à un trait :
// le panneau pivote alors autour de la verticale juste assez pour montrer au
// soleil `kShadowPanelMinimumSunFacing` de sa largeur. Un soleil au zénith ne
// donne aucune direction au sol, et le panneau reste parallèle à l'écran.
[[nodiscard]] BillboardBasis ShadowPanelBasis(const glm::vec3& toward_sun,
                                              const BillboardBasis& facing_camera);

// DE COMBIEN LES PIEDS D'UN SPRITE RECOUVRENT LE DÉBUT DE SON OMBRE, en pixels
// du sprite : ce qu'une image vue de face dessine sous son point d'ancrage
// (six pixels pour un chevalier sur Peco, dix pour un épéiste).
inline constexpr float kShadowFootOverlapPixels = 6.0F;
// La plongée la plus faible dont on tient compte pour reculer une silhouette :
// le sinus de l'angle sous l'horizon, une quinzaine de degrés. En deçà, un pas
// de l'écran couvrirait au sol une distance sans fin.
inline constexpr float kShadowFootMinimumDive = 0.25F;

// LE RECUL QUI GLISSE L'OMBRE D'UN SPRITE SOUS SES PIEDS.
//
// Vu de face, un sprite dessine ses pieds sous son point d'ancrage : ils
// recouvrent le début de son ombre, et il touche le sol. Vu de dos, la même
// créature s'arrête souvent à l'ancrage, et l'ombre de ses jambes, trop fine,
// laisse un vide entre les pieds et celle du corps : il paraît flotter.
//
// `sunk_height` est la profondeur, en unités du monde, du point le plus bas du
// sprite sous son ancrage. Ce qui manque pour atteindre
// `kShadowFootOverlapPixels` se rattrape en reculant le panneau d'ombre, à
// l'opposé de la caméra, jusqu'au point du sol que l'écran montre d'autant
// plus haut. Une image qui recouvre déjà son ombre ne bouge pas.
[[nodiscard]] glm::vec3 ShadowFootShift(float sunk_height, const BillboardBasis& facing_camera);

// L'ANGLE D'ÉCRAN D'UNE DIRECTION DU MONDE, en degrés, pour un sprite dont
// l'image pointe vers le HAUT de l'écran au repos : de combien la tourner pour
// qu'elle pointe le long de cette direction, telle que la caméra la montre.
//
// C'est l'angle dont un sprite d'effet qui VOLE se couche sur son vol — voir
// `EffectNodePattern::turns_with_flight` —, mesuré sur la base du billboard
// plutôt que sur des angles : la projection dit d'elle-même où va la direction
// à l'écran, plongée de la caméra comprise. ⚠ L'ordonnée d'écran DESCEND, et un
// angle positif tourne dans le sens des aiguilles d'une montre — la convention
// de `ComposeEffectNodeSprite`. Une direction que la caméra ne peut pas montrer
// — le long de son regard — rend zéro.
//
// ⚠ LA FLÈCHE D'UN ARC NE PASSE PAS PAR ICI : le client la tourne du lacet seul,
// sans la plongée, et son image pointe vers le BAS au repos. Voir
// `EffectProjectileScreenAngleDegrees`.
[[nodiscard]] inline float ScreenRotationOfDirection(glm::vec3 direction,
                                                     const BillboardBasis& basis) {
  const float rightward = glm::dot(direction, basis.right);
  const float downward = -glm::dot(direction, basis.up);
  if (rightward == 0.0F && downward == 0.0F) {
    return 0.0F;
  }
  return std::atan2(rightward, -downward) * 180.0F / std::numbers::pi_v<float>;
}

// Les quatre coins d'un panneau, en coordonnées du monde.
//
// La surcharge qui prend les COINS SEULS sert aussi bien un sprite qu'un effet
// animé : le jeu les grandit de la même échelle tirée de la profondeur, pour
// un acteur comme pour un effet. Un pixel d'effet vaut donc un septième d'unité, comme un pixel de
// sprite.
//
//
// `ground_position` est le point du sol où se tient l'acteur : c'est l'origine
// du repère du sprite, celle que le .act pose à l'emplacement du personnage.
//
// ⚠ L'axe Y d'un sprite descend, celui du monde monte. L'inversion est faite
// ici ; l'oublier planterait les personnages la tête en bas, exactement comme
// les modèles du décor l'ont été.
[[nodiscard]] std::array<glm::vec3, 4> BillboardCorners(const std::array<glm::vec2, 4>& corners,
                                                        const glm::vec3& ground_position,
                                                        const BillboardBasis& basis);

// Un seul point du panneau, par la même règle que ses coins : ce qui se pose à
// l'écran à la place d'un sprite — un texte — tombe ainsi là où il tomberait.
[[nodiscard]] glm::vec3 BillboardPoint(glm::vec2 sprite_pixels, const glm::vec3& ground_position,
                                       const BillboardBasis& basis);
[[nodiscard]] std::array<glm::vec3, 4> BillboardCorners(const SpriteQuad& quad,
                                                        const glm::vec3& ground_position,
                                                        const BillboardBasis& basis);

// Les points dont le quad prend sa PROFONDEUR — et eux seuls.
//
// Un panneau face à l'écran est tout entier à la distance de son point d'appui.
// Le tampon de profondeur le tranche alors net à la hauteur de ce point : sous
// l'origine, le sol vu à l'écran est PLUS PROCHE que l'acteur et le recouvre.
// D'où deux défauts qui n'en font qu'un — les pieds rognés de tout le monde, un
// mort à demi enterré parce que son corps est couché SOUS son origine — et,
// dans l'autre sens, un personnage entier effacé par le moindre décor plus
// proche que ses pieds.
//
// Le jeu ne fait pas cela : il empile ses quads d'acteur en coordonnées d'écran
// avec une profondeur PAR SOMMET, et les deux sommets du haut n'ont pas celle
// des deux du bas : la paire haute reçoit une valeur, la paire basse l'autre ;
// ce sont des inverses de distance, convertis avec un plan proche à 10 et un
// plan lointain à 1500.
//
// La règle retenue lit le sprite comme ce qu'il représente :
//
//   * AU-DESSUS de l'origine, il se dresse — la tête d'un personnage est plus
//     haut dans le monde, donc plus près d'une caméra qui plonge, et passe
//     devant ce que ses pieds ont derrière. Chaque pixel prend la profondeur
//     du PLAN VERTICAL de l'acteur là où il le voit : la profondeur monte comme
//     la tangente du plongeon ;
//   * EN DESSOUS, il se COUCHE sur le sol — un corps à terre s'étend vers la
//     caméra, pas sous le terrain.
//
// La FORME, elle, reste celle du panneau face à l'écran : c'est ce qui empêche
// un personnage de s'écraser quand on plonge. On sépare donc ce qu'on dessine
// de ce dont on prend la profondeur.
[[nodiscard]] std::array<glm::vec3, 4> BillboardDepthCorners(
    const std::array<glm::vec2, 4>& corners, const glm::vec3& ground_position,
    const BillboardBasis& basis);
[[nodiscard]] std::array<glm::vec3, 4> BillboardDepthCorners(const SpriteQuad& quad,
                                                             const glm::vec3& ground_position,
                                                             const BillboardBasis& basis);

// COMBIEN DE PIXELS DE SPRITE VAUT UN PIXEL D'ÉCRAN à la profondeur de ce point,
// pour une vue haute de `viewport_height_pixels`. Un panneau posé plus loin a
// besoin de plus de pixels de sprite pour couvrir le même pixel d'écran. Zéro
// pour un point derrière l'œil ou une vue sans hauteur.
[[nodiscard]] float SpritePixelsPerScreenPixel(const Camera& camera,
                                               const glm::vec3& ground_position,
                                               float viewport_height_pixels);

// LE QUAD ÉLARGI de `grow` unités de ses coins sur chacun de ses quatre côtés,
// le long de ses PROPRES bords : un quad tourné s'élargit dans son repère, comme
// le rectangle que le client élargit avant de le tourner.
[[nodiscard]] SpriteQuad GrowSpriteQuad(const SpriteQuad& quad, float grow);

// LE QUAD TOURNÉ SUR SON CENTRE de `degrees`, dans le sens des aiguilles d'une
// montre à l'écran : les coins sont en pixels de sprite, y vers le BAS.
[[nodiscard]] SpriteQuad SpinSpriteQuad(const SpriteQuad& quad, float degrees);

}  // namespace ro::render

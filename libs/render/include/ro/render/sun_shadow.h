#pragma once

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/scene_light.h"

namespace ro::render {

// L'OMBRE DU SOLEIL : où se pose la carte de profondeur vue depuis la lumière
// directionnelle de la carte.
//
// Aucune dépendance à OpenGL : ce ne sont que des matrices, qui se vérifient
// par des assertions numériques.
//
// La carte ne couvre pas toute la carte du jeu : seulement ce que la caméra
// voit, tranché sur une épaisseur de relief autour du point de visée. La
// caméra de RO plonge sous un champ étroit, et cette emprise reste de quelques
// centaines d'unités : une seule carte, sans cascade, y garde des texels plus
// fins qu'une unité.
//
// La profondeur, elle, couvre toute l'emprise du terrain et le décor qui la
// dépasse : une tour hors champ porte encore son ombre dans le champ.

// L'épaisseur de relief, de part et d'autre du point de visée, où l'on cherche
// ce que la caméra voit. En unités du monde : un cube de sol en vaut dix.
inline constexpr float kSunShadowReliefMargin = 150.0F;
// Ce que le décor dépasse au plus le sommet du terrain.
inline constexpr float kSunShadowCasterRise = 1000.0F;
// Un rayon de caméra qui ne rencontre pas le sol s'arrête à tant de fois la
// distance de vue.
inline constexpr float kSunShadowReachPerViewDistance = 3.0F;
// LE CÔTÉ DE L'EMPRISE SE COMPTE PAR CE PAS : il ne change qu'au franchissement
// d'un pas, et la carte ne se redimensionne pas à chaque image d'une rotation.
inline constexpr float kSunShadowSideStep = 64.0F;

struct SunShadowFrame {
  // Du monde aux coordonnées normalisées de la carte d'ombre.
  glm::mat4 view_projection{1.0F};
  // Le côté d'un texel, en unités du monde.
  float texel_world = 0.0F;
};

// `light_direction` pointe VERS le soleil, comme `SceneLight::direction`.
// `bounds_min` et `bounds_max` sont l'emprise du terrain, voir `TerrainMesh`.
//
// 🔴 LE CADRE AVANCE PAR TEXELS ENTIERS : la caméra qui glisse ne fait pas
// scintiller les bords d'ombre, chaque texel retombe sur la même parcelle du
// monde.
// L'ÉCLAIREMENT D'UN SOL PLAT sous `light`, canal par canal, par la formule
// que le sol emploie.
[[nodiscard]] glm::vec3 FlatGroundLightOf(const SceneLight& light);

// CE QUE L'OMBRE DU SOLEIL LAISSE D'UN SOL PLAT, canal par canal : son
// éclairement à l'ombre sur son éclairement au soleil, par la formule que le
// sol emploie. Un sprite n'est pas éclairé par sommet, et c'est ce rapport qui
// l'assombrit comme le sol où il se tient. Un, sans diffuse : rien à retirer.
[[nodiscard]] glm::vec3 SunShadeOf(const SceneLight& light);

[[nodiscard]] SunShadowFrame FrameSunShadow(const glm::vec3& light_direction, const Camera& camera,
                                            float aspect_ratio, const glm::vec3& bounds_min,
                                            const glm::vec3& bounds_max, int map_texels);

}  // namespace ro::render

#pragma once

#include <array>
#include <cstddef>

#include <glm/glm.hpp>

#include "ro/render/point_lights.h"

namespace ro::render {

// LES OMBRES DES LUMIÈRES PONCTUELLES, côté calcul : quelles lumières en
// portent, et comment chacune voit le monde. La carte elle-même est
// `PointShadowMaps`, la passe `DrawPointShadows`.
//
// Une lumière ombrée voit la scène par SIX FACES d'un cube centré sur elle,
// chacune un champ carré de 90° tourné vers un axe. Ses faces suivent la
// convention des textures cubiques d'OpenGL : la direction d'un point vu de la
// lumière désigne, lue telle quelle, le texel qui le couvre.

// Les lumières qui portent ombre au plus : les plus proches de l'œil parmi
// celles de l'image. Les suivantes éclairent sans ombre. ⚠ LA MÊME BORNE QUE
// `kMaxShadowedPointLights` du GLSL, qui écrit une branche par carte.
inline constexpr std::size_t kMaxShadowedPointLights = 2;

// Le côté d'une face, en texels.
inline constexpr int kPointShadowMapTexels = 512;

inline constexpr std::size_t kCubeFaceCount = 6;

// LE PLAN PROCHE de chaque face, en unités du monde : ce qui touche la lumière
// de plus près ne l'ombre pas. Le plan lointain est la portée de la lumière,
// au-delà de laquelle elle n'éclaire rien.
inline constexpr float kPointShadowNearWorld = 0.5F;

// LE PLAN LOINTAIN d'une lumière de portée `range_world` : sa portée, jamais
// plus près que deux fois le plan proche, où la profondeur perdrait son sens.
[[nodiscard]] float PointShadowFar(float range_world);

// Les lumières de `set` qui portent ombre : ses `kMaxShadowedPointLights`
// premières, déjà rangées de la plus proche de l'œil à la plus lointaine.
[[nodiscard]] std::size_t ShadowedPointLightCount(const PointLightSet& set);

// LES SIX VUES D'UNE LUMIÈRE posée en `light_position`, dans l'ordre des faces
// d'OpenGL : +X, −X, +Y, −Y, +Z, −Z. Chacune projette son quart de l'espace
// sur sa face entière, de `kPointShadowNearWorld` à `far_world`.
[[nodiscard]] std::array<glm::mat4, kCubeFaceCount> PointShadowFaceViewProjections(
    glm::vec3 light_position, float far_world);

// L'axe que regarde la face `face`, dans le même ordre.
[[nodiscard]] glm::vec3 CubeFaceForward(std::size_t face);

// LA PROFONDEUR QU'UNE FACE ÉCRIT pour un point dont l'écart à la lumière, le
// long de l'axe de cette face, vaut `axis_distance_world` : de zéro au plan
// proche à un au plan lointain. Les nuanceurs la recalculent pour comparer —
// voir `kPointLightGlsl`.
[[nodiscard]] float PointShadowDepth(float axis_distance_world, float far_world);

}  // namespace ro::render

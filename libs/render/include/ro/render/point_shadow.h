#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

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
// celles de l'image. Les suivantes éclairent sans ombre — et les personnages
// qu'elles éclairent n'en projettent aucune : assez pour une place éclairée
// de quelques braseros. Une carte ne se repeint qu'à son tour
// (`kPointShadowRepaintPeriod`). ⚠ LA MÊME BORNE QUE `kMaxShadowedPointLights`
// du GLSL, qui écrit une branche par carte.
inline constexpr std::size_t kMaxShadowedPointLights = 4;

// Le côté d'une face, en texels.
inline constexpr int kPointShadowMapTexels = 512;

inline constexpr std::size_t kCubeFaceCount = 6;

// LE DÉGAGEMENT d'une lumière, en unités du monde : le plan proche de chaque
// face. Ce qui se tient à moins de cet écart de la lumière, le long de l'axe
// d'une face, n'est pas peint dans sa carte et n'ombre donc rien ; ce qui s'y
// tient n'est pas ombré non plus. C'est le verre d'une lanterne, la vasque
// d'un brasero où l'on pose la flamme : ils entourent la lumière sans la
// boucher. Le plan lointain est la portée de la lumière, au-delà de laquelle
// elle n'éclaire rien.
inline constexpr float kPointShadowClearanceWorld = 10.0F;

// LE PLAN LOINTAIN d'une lumière de portée `range_world` : sa portée, jamais
// plus près que deux fois le dégagement, où la profondeur perdrait son sens.
[[nodiscard]] float PointShadowFar(float range_world);

// CE QU'UNE CARTE PORTE DÉJÀ : la lumière pour laquelle elle a été peinte.
struct PaintedPointLight {
  glm::vec3 world_position{0.0F};
  float range_world = 0.0F;

  friend bool operator==(const PaintedPointLight&, const PaintedPointLight&) = default;
};

// TOUTES LES CARTES NE SE REPEIGNENT PAS À CHAQUE IMAGE. Une lumière nouvelle
// ou déplacée se repeint aussitôt ; une lumière immobile, une image sur
// `kPointShadowRepaintPeriod`, à tour de rôle avec les autres — ce qui bouge
// devant elle, un personnage, voit son ombre suivre à cette cadence. Rend,
// pour chacune des `count` premières lumières de `set`, s'il faut la
// repeindre à l'image `frame`, sachant ce que `painted` porte déjà.
inline constexpr std::size_t kPointShadowRepaintPeriod = 2;
[[nodiscard]] std::array<bool, kMaxShadowedPointLights> PointShadowRepaints(
    const std::array<std::optional<PaintedPointLight>, kMaxShadowedPointLights>& painted,
    const PointLightSet& set, std::size_t count, std::uint64_t frame);

// UNE OMBRE NE SAUTE PAS D'UNE LUMIÈRE À L'AUTRE pour un pas de la caméra.
// Les lumières les plus proches de `eye` portent ombre ; mais une lumière qui
// en porte déjà une — celle que `painted` dit peinte, à sa place et à sa
// portée — la garde tant que les lumières sans ombre nettement plus proches
// qu'elle, à moins de `kShadowSwapRatio` fois sa distance, ne suffisent pas à
// remplir les cartes. Elle garde aussi son rang, donc sa carte : rien à
// repeindre. Rend `set` réordonné, ses lumières ombrées d'abord. Une lumière
// qui ne porte pas d'ombre (`casts_shadow` faux) ne prend jamais de carte.
inline constexpr float kShadowSwapRatio = 0.7F;
[[nodiscard]] PointLightSet KeepShadowedOrder(
    const PointLightSet& set,
    const std::array<std::optional<PaintedPointLight>, kMaxShadowedPointLights>& painted,
    glm::vec3 eye);

// Les lumières de `set` qui portent ombre : ses premières, au plus
// `kMaxShadowedPointLights`, autant qu'il y en a qui la projettent — voir
// `KeepShadowedOrder`, qui les range en tête.
[[nodiscard]] std::size_t ShadowedPointLightCount(const PointLightSet& set);

// LES SIX VUES D'UNE LUMIÈRE posée en `light_position`, dans l'ordre des faces
// d'OpenGL : +X, −X, +Y, −Y, +Z, −Z. Chacune projette son quart de l'espace
// sur sa face entière, de `kPointShadowClearanceWorld` à `far_world`.
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

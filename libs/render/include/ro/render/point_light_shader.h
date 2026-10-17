#pragma once

#include <string>
#include <string_view>

#include "ro/render/gl_program.h"
#include "ro/render/point_lights.h"

namespace ro::render {

// LES LUMIÈRES PONCTUELLES DANS LES NUANCEURS, écrites une fois pour le terrain,
// le décor, les acteurs-modèles et les sprites.
//
// `kPointLightGlsl` déclare les uniformes et `PointLightingOf(position, normale,
// recul)`, qui somme la lumière que chaque source dépose en un point du monde :
// sa couleur, atténuée par le carré de ce qui reste de sa portée, puis masquée
// par sa carte d'ombre quand elle en a une — voir `PointVisibility`. Il vaut
// pour l'une ou l'autre étape : un sprite l'évalue par sommet, à ses pieds, par
// `PointLightingPulled`.
//
// Le terrain et le décor l'évaluent par fragment, par `PointSurfaceLighting` :
// leur nuanceur de sommets passe la position MONDE de chaque sommet par
// `v_world_position`, qu'il doit écrire, et celui des fragments la reçoit
// interpolée.
//
// Cette lumière S'AJOUTE à celle de la carte et multiplie la texture comme
// elle. ⚠ ELLE N'EST PAS ASSOMBRIE PAR LA NUIT : une torche éclaire la nuit.
inline constexpr std::string_view kPointLightVertexGlsl = R"GLSL(
out vec3 v_world_position;
)GLSL";

// Le fragment y gagne `PointSurfaceNormal()`, la normale GÉOMÉTRIQUE de la face
// qu'il peint, tirée des dérivées de sa position : son signe ne compte pas.
// ⚠ À LIRE AVANT TOUT `discard`, où les dérivées ne sont plus définies.
// `PointSurfaceLighting(normal)` rend alors la lumière ponctuelle en ce
// fragment, l'ombre lue au-dessus de sa face — voir `PointShadowTaps`.
inline constexpr std::string_view kPointLightFragmentGlsl = R"GLSL(
in vec3 v_world_position;

vec3 PointSurfaceNormal() {
    vec3 across = cross(dFdx(v_world_position), dFdy(v_world_position));
    float span = length(across);
    return span > 0.0 ? across / span : vec3(0.0);
}

vec3 PointSurfaceLighting(vec3 surface_normal) {
    return PointLightingOf(v_world_position, surface_normal, 0.0);
}
)GLSL";

// L'OMBRE d'une lumière se lit dans sa carte cubique, sur un carré de texels
// pris dans le plan perpendiculaire au rayon, pour adoucir le bord.
//
// UNE FACE NE S'OMBRE PAS ELLE-MÊME : le point lu quitte la surface avant la
// lecture, de deux façons dont les tailles se comptent en texels de la carte
// à sa distance de la lumière, donc grandissent avec elle.
// - Il se soulève le long de la normale de sa face, tournée vers la lumière,
//   d'autant plus que la lumière la frappe de biais : un sol que la lumière
//   rase voit chaque texel de la carte couvrir une longue bande, et c'est la
//   seule correction qui ne demande pas un recul infini le long du rayon.
// - Il avance vers la lumière d'un texel et demi, et de `pull` en plus.
// Ce qui se tient à moins de `u_point_shadow_clearance` de la lumière, le long
// de l'axe de sa face, n'est pas ombré : voir `kPointShadowClearanceWorld`.
inline constexpr std::string_view kPointLightGlsl = R"GLSL(
// La même borne que `kMaxPointLights` côté C++.
const int kMaxPointLights = 8;
// La même que `kMaxShadowedPointLights` : une carte par branche de `PointVisibility`.
const int kMaxShadowedPointLights = 2;
uniform int u_point_count;
uniform vec3 u_point_position[kMaxPointLights];
uniform vec3 u_point_color[kMaxPointLights];
uniform float u_point_range[kMaxPointLights];
// Les `u_point_shadowed` premières lumières lisent leur carte.
uniform int u_point_shadowed;
uniform samplerCubeShadow u_point_shadow_0;
uniform samplerCubeShadow u_point_shadow_1;
uniform float u_point_shadow_far[kMaxShadowedPointLights];
// Voir `PointLight::shadow_depth` et `PointLight::shadow_spread`.
uniform float u_point_shadow_depth[kMaxShadowedPointLights];
uniform float u_point_shadow_spread[kMaxShadowedPointLights];
// Le plan proche de chaque face : voir `kPointShadowClearanceWorld`.
uniform float u_point_shadow_clearance;
// Le côté d'un texel à une unité de la lumière.
uniform float u_point_shadow_texel;

const float kPointShadowPullTexels = 1.5;
// LE SOULÈVEMENT, en texels, d'une face que la lumière rase : il couvre la
// lecture voisine du carré, le filtrage qui mêle deux texels, et le demi-texel
// qui sépare le point du centre de celui qui le couvre.
const float kPointShadowLiftTexels = 3.0;
const int kPointTapRadius = 1;
// Une ombre à pénombre se lit sur un carré plus large, aux lectures écartées :
// sans quoi l'écart ferait des marches.
const int kPointSoftTapRadius = 2;
// Sous ce cosinus, le rayon est trop près de la verticale pour en tirer un côté.
const float kPointUprightCosine = 0.9;

// L'écart à la lumière le long de l'axe de la face qui voit `from_light`.
float PointShadowAxis(vec3 from_light) {
    vec3 spread = abs(from_light);
    return max(spread.x, max(spread.y, spread.z));
}

// La profondeur qu'a écrite la face qui voit `from_light` : voir `PointShadowDepth`.
float PointShadowReference(vec3 from_light, float far_world) {
    float axis = PointShadowAxis(from_light);
    float near_world = u_point_shadow_clearance;
    float normalized = (far_world + near_world) / (far_world - near_world) -
                       2.0 * far_world * near_world / ((far_world - near_world) * axis);
    return normalized * 0.5 + 0.5;
}

// `surface_normal` : la normale de la face qui reçoit, de signe quelconque, ou
// zéro pour un point sans face.
float PointShadowTaps(samplerCubeShadow map, vec3 from_light, float far_world, float pull,
                      vec3 surface_normal, float spread) {
    if (PointShadowAxis(from_light) <= u_point_shadow_clearance) {
        return 1.0;
    }
    float distance_world = length(from_light);
    vec3 way = from_light / distance_world;
    float texel_world = distance_world * u_point_shadow_texel;
    // Le sinus de l'angle entre la normale et la lumière : nul pour une face
    // vue de front, un pour une face rasée.
    vec3 facing = dot(surface_normal, way) > 0.0 ? -surface_normal : surface_normal;
    float frontal = clamp(-dot(facing, way), 0.0, 1.0);
    float slant = sqrt(1.0 - frontal * frontal) * length(facing);
    vec3 lifted = from_light + facing * (slant * kPointShadowLiftTexels * texel_world);
    float lifted_world = length(lifted);
    way = lifted / lifted_world;
    vec3 nearer = way * max(lifted_world - pull - texel_world * kPointShadowPullTexels,
                            u_point_shadow_clearance);
    // L'écart entre deux lectures : un texel, ou la pénombre partagée entre
    // elles quand elle est plus large.
    int radius = spread > 0.0 ? kPointSoftTapRadius : kPointTapRadius;
    float step_world = max(texel_world, distance_world * spread / float(radius));
    // LE CARRÉ SE POSE perpendiculairement au rayon ; celui d'une pénombre, à
    // plat sur la face qui reçoit. Écartées en travers du rayon, ses lectures
    // passeraient derrière une face inclinée, qui s'ombrerait elle-même.
    vec3 across = spread > 0.0 && length(facing) > 0.0 ? facing : way;
    vec3 other = abs(across.y) < kPointUprightCosine ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 side = normalize(cross(across, other));
    vec3 lift = cross(across, side);
    float lit = 0.0;
    float taps = 0.0;
    for (int row = -radius; row <= radius; ++row) {
        for (int column = -radius; column <= radius; ++column) {
            vec3 tap = nearer + (side * float(column) + lift * float(row)) * step_world;
            lit += texture(map, vec4(tap, PointShadowReference(tap, far_world)));
            taps += 1.0;
        }
    }
    return lit / taps;
}

// La part de la lumière `index` qui atteint `world_position` : un sans carte.
// L'ombre n'en retire que sa profondeur.
float PointVisibility(int index, vec3 world_position, float pull, vec3 surface_normal) {
    if (index >= u_point_shadowed) {
        return 1.0;
    }
    vec3 from_light = world_position - u_point_position[index];
    float lit = index == 0
                    ? PointShadowTaps(u_point_shadow_0, from_light, u_point_shadow_far[0], pull,
                                      surface_normal, u_point_shadow_spread[0])
                    : PointShadowTaps(u_point_shadow_1, from_light, u_point_shadow_far[1], pull,
                                      surface_normal, u_point_shadow_spread[1]);
    return mix(1.0, lit, u_point_shadow_depth[index]);
}

vec3 PointLightingOf(vec3 world_position, vec3 surface_normal, float pull) {
    vec3 sum = vec3(0.0);
    for (int index = 0; index < u_point_count; ++index) {
        float remaining = clamp(1.0 - distance(world_position, u_point_position[index]) /
                                          u_point_range[index], 0.0, 1.0);
        if (remaining > 0.0) {
            sum += u_point_color[index] * remaining * remaining *
                   PointVisibility(index, world_position, pull, surface_normal);
        }
    }
    return sum;
}

// Un point sans face : un sommet de sprite.
vec3 PointLightingPulled(vec3 world_position, float pull) {
    return PointLightingOf(world_position, vec3(0.0), pull);
}
)GLSL";

// `shader` avec `points` inséré juste après sa ligne `#version`, qui doit
// rester la première.
[[nodiscard]] std::string WithPointLights(std::string_view shader, std::string_view points);
// Le même, avec `lighting` — `kPointLightGlsl` — inséré aussi.
[[nodiscard]] std::string WithPointLights(std::string_view shader, std::string_view points,
                                          std::string_view lighting);

// Pose les uniformes des lumières ponctuelles sur le programme EN SERVICE, et
// lie leurs cartes d'ombre. Un ensemble vide n'ajoute aucune lumière.
//
// ⚠ À APPELER POUR TOUT PROGRAMME QUI PORTE `kPointLightGlsl`, même sans
// lumière : ses lecteurs de cartes cubiques prennent alors leurs propres
// unités, et non l'unité zéro, que la texture occupe sous un autre type.
void SetPointLightUniforms(const GlProgram& program, const PointLightSet& set);

}  // namespace ro::render

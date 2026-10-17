#pragma once

#include <string>
#include <string_view>

#include "ro/render/gl_program.h"
#include "ro/render/point_lights.h"

namespace ro::render {

// LES LUMIÈRES PONCTUELLES DANS LES NUANCEURS, écrites une fois pour le terrain,
// le décor, les acteurs-modèles et les sprites.
//
// `kPointLightGlsl` déclare les uniformes et `PointLighting(vec3)`, qui somme
// la lumière que chaque source dépose en un point du monde : sa couleur,
// atténuée par le carré de ce qui reste de sa portée, puis masquée par sa
// carte d'ombre quand elle en a une — voir `PointVisibility`. Il vaut pour
// l'une ou l'autre étape : un sprite l'évalue par sommet, à ses pieds.
//
// Le terrain et le décor l'évaluent par fragment : leur nuanceur de sommets
// passe la position MONDE de chaque sommet par `v_world_position`, qu'il doit
// écrire, et celui des fragments la reçoit interpolée.
//
// Cette lumière S'AJOUTE à celle de la carte et multiplie la texture comme
// elle. ⚠ ELLE N'EST PAS ASSOMBRIE PAR LA NUIT : une torche éclaire la nuit.
inline constexpr std::string_view kPointLightVertexGlsl = R"GLSL(
out vec3 v_world_position;
)GLSL";

inline constexpr std::string_view kPointLightFragmentGlsl = R"GLSL(
in vec3 v_world_position;
)GLSL";

// L'OMBRE d'une lumière se lit dans sa carte cubique, sur un carré de texels
// pris dans le plan perpendiculaire au rayon, pour adoucir le bord. Le point
// lu avance vers la lumière d'un texel et demi — à sa distance —, et de
// `pull` en plus : une face ne s'ombre pas elle-même.
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
uniform float u_point_shadow_near;
// Le côté d'un texel à une unité de la lumière.
uniform float u_point_shadow_texel;

const float kPointShadowBiasTexels = 1.5;
const int kPointTapRadius = 1;
// Sous ce cosinus, le rayon est trop près de la verticale pour en tirer un côté.
const float kPointUprightCosine = 0.9;

// La profondeur qu'a écrite la face qui voit `from_light` : voir `PointShadowDepth`.
float PointShadowReference(vec3 from_light, float far_world) {
    vec3 spread = abs(from_light);
    float axis = max(spread.x, max(spread.y, spread.z));
    float near_world = u_point_shadow_near;
    float normalized = (far_world + near_world) / (far_world - near_world) -
                       2.0 * far_world * near_world / ((far_world - near_world) * axis);
    return normalized * 0.5 + 0.5;
}

float PointShadowTaps(samplerCubeShadow map, vec3 from_light, float far_world, float pull) {
    float distance_world = length(from_light);
    if (distance_world <= u_point_shadow_near) {
        return 1.0;
    }
    vec3 way = from_light / distance_world;
    float texel_world = distance_world * u_point_shadow_texel;
    vec3 nearer = way * max(distance_world - pull - texel_world * kPointShadowBiasTexels,
                            u_point_shadow_near);
    vec3 other = abs(way.y) < kPointUprightCosine ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 side = normalize(cross(way, other));
    vec3 lift = cross(way, side);
    float lit = 0.0;
    float taps = 0.0;
    for (int row = -kPointTapRadius; row <= kPointTapRadius; ++row) {
        for (int column = -kPointTapRadius; column <= kPointTapRadius; ++column) {
            vec3 tap = nearer + (side * float(column) + lift * float(row)) * texel_world;
            lit += texture(map, vec4(tap, PointShadowReference(tap, far_world)));
            taps += 1.0;
        }
    }
    return lit / taps;
}

// La part de la lumière `index` qui atteint `world_position` : un sans carte.
float PointVisibility(int index, vec3 world_position, float pull) {
    if (index >= u_point_shadowed) {
        return 1.0;
    }
    vec3 from_light = world_position - u_point_position[index];
    if (index == 0) {
        return PointShadowTaps(u_point_shadow_0, from_light, u_point_shadow_far[0], pull);
    }
    return PointShadowTaps(u_point_shadow_1, from_light, u_point_shadow_far[1], pull);
}

vec3 PointLightingPulled(vec3 world_position, float pull) {
    vec3 sum = vec3(0.0);
    for (int index = 0; index < u_point_count; ++index) {
        float remaining = clamp(1.0 - distance(world_position, u_point_position[index]) /
                                          u_point_range[index], 0.0, 1.0);
        if (remaining > 0.0) {
            sum += u_point_color[index] * remaining * remaining *
                   PointVisibility(index, world_position, pull);
        }
    }
    return sum;
}

vec3 PointLighting(vec3 world_position) {
    return PointLightingPulled(world_position, 0.0);
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

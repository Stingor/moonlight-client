#pragma once

#include <string>
#include <string_view>

#include "ro/render/gl_program.h"
#include "ro/render/point_lights.h"

namespace ro::render {

// LES LUMIÈRES PONCTUELLES DANS LES NUANCEURS, écrites une fois pour le terrain
// et le décor.
//
// Le nuanceur de sommets passe la position MONDE de chaque sommet par
// `v_world_position`, qu'il doit écrire ; celui des fragments la reçoit
// interpolée et somme, par `PointLighting`, la lumière que chaque source y
// dépose : sa couleur, atténuée par le carré de ce qui reste de sa portée.
//
// Cette lumière S'AJOUTE à celle de la carte et multiplie la texture comme
// elle. ⚠ ELLE N'EST PAS ASSOMBRIE PAR LA NUIT : une torche éclaire la nuit.
inline constexpr std::string_view kPointLightVertexGlsl = R"GLSL(
out vec3 v_world_position;
)GLSL";

inline constexpr std::string_view kPointLightFragmentGlsl = R"GLSL(
in vec3 v_world_position;

// La même borne que `kMaxPointLights` côté C++.
const int kMaxPointLights = 8;
uniform int u_point_count;
uniform vec3 u_point_position[kMaxPointLights];
uniform vec3 u_point_color[kMaxPointLights];
uniform float u_point_range[kMaxPointLights];

vec3 PointLighting(vec3 world_position) {
    vec3 sum = vec3(0.0);
    for (int index = 0; index < u_point_count; ++index) {
        float remaining = clamp(1.0 - distance(world_position, u_point_position[index]) /
                                          u_point_range[index], 0.0, 1.0);
        sum += u_point_color[index] * remaining * remaining;
    }
    return sum;
}
)GLSL";

// `shader` avec `points` inséré juste après sa ligne `#version`, qui doit
// rester la première.
[[nodiscard]] std::string WithPointLights(std::string_view shader, std::string_view points);

// Pose les uniformes des lumières ponctuelles sur le programme EN SERVICE.
// Un ensemble vide n'ajoute aucune lumière.
void SetPointLightUniforms(const GlProgram& program, const PointLightSet& set);

}  // namespace ro::render

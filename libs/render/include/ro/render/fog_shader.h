#pragma once

#include <string>
#include <string_view>

#include "ro/render/gl_program.h"
#include "ro/render/scene_fog.h"

namespace ro::render {

// LE BROUILLARD DANS LES NUANCEURS, écrit une fois pour tous ceux du monde.
//
// Le nuanceur de sommets calcule la part de couleur que chaque sommet GARDE —
// `FogKeptPart`, sur sa profondeur de vue — ; celui des fragments la reçoit
// mélangée le long du triangle, et donne le reste à la couleur du brouillard —
// `Fogged`. L'alpha n'est pas touché.
//
// ⚠ LA PART SE TRONQUE AU DEUX-CENT-CINQUANTE-CINQUIÈME, SOMMET PAR SOMMET :
// le client la range dans l'octet d'alpha de la couleur spéculaire de chaque
// sommet, et c'est cet octet que la carte graphique mélange ensuite.
inline constexpr std::string_view kFogVertexGlsl = R"GLSL(
uniform float u_fog_on;
uniform float u_fog_start;
uniform float u_fog_end;

float FogKeptPart(float view_depth) {
    float covered = clamp((view_depth - u_fog_start) / (u_fog_end - u_fog_start), 0.0, 1.0);
    return mix(1.0, floor((1.0 - covered) * 255.0) / 255.0, u_fog_on);
}
)GLSL";

inline constexpr std::string_view kFogFragmentGlsl = R"GLSL(
uniform vec3 u_fog_color;

vec3 Fogged(vec3 color, float kept_part) {
    return mix(u_fog_color, color, kept_part);
}
)GLSL";

// `shader` avec `fog` inséré juste après sa ligne `#version`, qui doit rester
// la première.
[[nodiscard]] std::string WithFog(std::string_view shader, std::string_view fog);

// Pose les uniformes du brouillard sur le programme EN SERVICE. Éteint, le
// brouillard laisse à chaque sommet toute sa couleur.
void SetFogUniforms(const GlProgram& program, const SceneFog& fog);

}  // namespace ro::render

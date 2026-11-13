#pragma once

#include <string>
#include <string_view>

#include "ro/render/effect_dye.h"
#include "ro/render/gl_program.h"

namespace ro::render {

// LA TEINTURE D'EFFET DANS UN NUANCEUR : le même calcul pour les effets et pour
// les sprites qu'un effet de chapeau fait naître. C'est la copie, ligne pour
// ligne, de `DyeEffectTexel`.
//
// `Dyed(couleur)` rend la couleur telle quelle quand `u_dyed` est nul : un lot
// sans teinture garde ses texels au bit près. `u_dye` porte le tour de teinte
// en degrés, puis les facteurs de saturation et de clarté ; `u_dye_darken`
// peint le lot en alpha (voir `EffectDye::darken`).
inline constexpr std::string_view kDyeFragmentGlsl = R"GLSL(
uniform int u_dyed;
uniform vec3 u_dye;
uniform int u_dye_darken;

vec3 RgbToHsl(vec3 color) {
    float high = max(color.r, max(color.g, color.b));
    float low = min(color.r, min(color.g, color.b));
    float lightness = (high + low) * 0.5;
    float spread = high - low;
    if (spread <= 0.0) {
        return vec3(0.0, 0.0, lightness);
    }
    float saturation = lightness > 0.5 ? spread / (2.0 - high - low) : spread / (high + low);
    float sextant = high == color.r ? mod((color.g - color.b) / spread + 6.0, 6.0)
                  : high == color.g ? (color.b - color.r) / spread + 2.0
                                    : (color.r - color.g) / spread + 4.0;
    return vec3(sextant * 60.0, saturation, lightness);
}

float HueChannel(float low, float high, float hue) {
    float sextant = mod(hue, 360.0) / 60.0;
    if (sextant < 1.0) {
        return low + (high - low) * sextant;
    }
    if (sextant < 3.0) {
        return high;
    }
    if (sextant < 4.0) {
        return low + (high - low) * (4.0 - sextant);
    }
    return low;
}

vec3 HslToRgb(vec3 hsl) {
    if (hsl.y <= 0.0) {
        return vec3(hsl.z);
    }
    float high = hsl.z <= 0.5 ? hsl.z * (1.0 + hsl.y) : hsl.z + hsl.y - hsl.z * hsl.y;
    float low = 2.0 * hsl.z - high;
    return vec3(HueChannel(low, high, hsl.x + 120.0), HueChannel(low, high, hsl.x),
                HueChannel(low, high, hsl.x - 120.0));
}

vec4 Dyed(vec4 color) {
    if (u_dyed == 0) {
        return color;
    }
    if (u_dye_darken != 0) {
        float high = max(color.r, max(color.g, color.b));
        if (high <= 0.0) {
            return vec4(0.0);
        }
        color = vec4(color.rgb / high, color.a * high);
    }
    vec3 hsl = RgbToHsl(color.rgb);
    hsl = vec3(hsl.x + u_dye.x, clamp(hsl.y * u_dye.y, 0.0, 1.0), clamp(hsl.z * u_dye.z, 0.0, 1.0));
    return vec4(HslToRgb(hsl), color.a);
}
)GLSL";

// Le nuanceur de fragments, la teinture insérée après sa ligne `#version`.
[[nodiscard]] std::string WithDye(std::string_view fragment_shader);

// Pose la teinture d'un lot ; nulle, le lot n'y passe pas. `darkened` dit que
// ce lot ajoutait au fond et se peint désormais en alpha.
void SetDyeUniforms(const GlProgram& program, const EffectDye* dye, bool darkened);

}  // namespace ro::render

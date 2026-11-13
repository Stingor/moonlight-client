#include "ro/render/dye_shader.h"

#include <glm/glm.hpp>

#include "ro/render/fog_shader.h"

namespace ro::render {

// ⚠ LA MÊME INSERTION QUE LE BROUILLARD : après la ligne `#version`, que GLSL
// veut en tête.
std::string WithDye(std::string_view fragment_shader) {
  return WithFog(fragment_shader, kDyeFragmentGlsl);
}

void SetDyeUniforms(const GlProgram& program, const EffectDye* dye, bool darkened) {
  program.SetInt("u_dyed", dye != nullptr ? 1 : 0);
  if (dye == nullptr) {
    return;
  }
  program.SetVector3("u_dye",
                     glm::vec3{dye->hue_degrees, dye->saturation_scale, dye->lightness_scale});
  program.SetInt("u_dye_darken", darkened ? 1 : 0);
}

}  // namespace ro::render

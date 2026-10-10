#include "ro/render/fog_shader.h"

#include <cstddef>
#include <string>

#include <glm/glm.hpp>

namespace ro::render {

std::string WithFog(std::string_view shader, std::string_view fog) {
  const std::size_t version = shader.find("#version");
  const std::size_t line_end =
      version == std::string_view::npos ? std::string_view::npos : shader.find('\n', version);
  if (line_end == std::string_view::npos) {
    return std::string(fog) + std::string(shader);
  }
  std::string combined(shader.substr(0, line_end + 1));
  combined += fog;
  combined += shader.substr(line_end + 1);
  return combined;
}

void SetFogUniforms(const GlProgram& program, const SceneFog& fog) {
  program.SetFloat("u_fog_on", fog.on ? 1.0F : 0.0F);
  program.SetFloat("u_fog_start", fog.start_depth);
  program.SetFloat("u_fog_end", fog.end_depth);
  program.SetVector3("u_fog_color", glm::vec3(fog.color[0], fog.color[1], fog.color[2]));
}

}  // namespace ro::render

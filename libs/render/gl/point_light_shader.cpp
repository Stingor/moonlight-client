#include "ro/render/point_light_shader.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/fog_shader.h"
#include "ro/render/point_light_stage.h"
#include "ro/render/point_shadow.h"

namespace ro::render {
namespace {

constexpr std::string_view kGlslTexelBound = "const int kPointLightTexels = ";
constexpr std::string_view kGlslTierBound = "const int kPointShadowTierCount = ";
constexpr std::size_t kDecimalBase = 10;

// Le nom du lecteur du tableau de chaque palier, du plus fin au plus grossier.
constexpr std::array<std::string_view, kPointShadowTierCount> kTierSamplers = {
    "u_point_shadow_fine", "u_point_shadow_middle", "u_point_shadow_coarse"};

// La borne que le GLSL déclare après `declaration`, lue dans son texte ; zéro
// s'il n'en déclare pas.
constexpr std::size_t GlslBound(std::string_view declaration) {
  const std::size_t at = kPointLightGlsl.find(declaration);
  if (at == std::string_view::npos) {
    return 0;
  }
  std::size_t bound = 0;
  for (const char digit : kPointLightGlsl.substr(at + declaration.size())) {
    if (digit < '0' || digit > '9') {
      break;
    }
    bound = bound * kDecimalBase + static_cast<std::size_t>(digit - '0');
  }
  return bound;
}

// 🔴 LE GLSL LIT LES LUMIÈRES COMME LE C++ LES ÉCRIT, et il a un lecteur par
// palier.
static_assert(GlslBound(kGlslTexelBound) == kPointLightTexels,
              "les texels d'une lumière diffèrent entre le GLSL et kPointLightTexels");
static_assert(GlslBound(kGlslTierBound) == kPointShadowTierCount,
              "les paliers d'ombre diffèrent entre le GLSL et kPointShadowTierCount");

// La ligne `#version` de `shader` remplacée par `kPointLightGlslVersion`.
std::string RaisedVersion(std::string_view shader) {
  const std::size_t version = shader.find("#version");
  if (version == std::string_view::npos) {
    return std::string(shader);
  }
  const std::size_t line_end = shader.find('\n', version);
  std::string raised(shader.substr(0, version));
  raised += kPointLightGlslVersion;
  if (line_end != std::string_view::npos) {
    raised += shader.substr(line_end);
  }
  return raised;
}

}  // namespace

std::string WithPointLights(std::string_view shader, std::string_view points) {
  // L'insertion après `#version` est la même que celle du brouillard.
  return WithFog(RaisedVersion(shader), points);
}

std::string WithPointLights(std::string_view shader, std::string_view points,
                            std::string_view lighting) {
  return WithFog(WithFog(RaisedVersion(shader), points), lighting);
}

std::vector<glm::vec4> PointLightTexelsOf(const PointLightSet& set) {
  std::vector<glm::vec4> texels;
  texels.reserve(set.lights.size() * kPointLightTexels);
  for (std::size_t rank = 0; rank < set.lights.size(); ++rank) {
    const PointLight& light = set.lights[rank];
    const PointShadowSlot slot = rank < set.shadows.size() ? set.shadows[rank] : PointShadowSlot();
    texels.emplace_back(light.world_position, light.range_world);
    texels.emplace_back(light.color, std::clamp(light.shadow_depth, 0.0F, 1.0F));
    texels.emplace_back(PointShadowFar(light.range_world), std::max(light.shadow_spread, 0.0F),
                        static_cast<float>(slot.tier), static_cast<float>(slot.layer));
  }
  return texels;
}

void SetPointLightUniforms(const GlProgram& program, const PointLightSet& set) {
  const bool lit = set.stage != nullptr && set.stage->Holds(set);
  program.SetInt("u_point_count", lit ? static_cast<int>(set.lights.size()) : 0);
  program.SetInt("u_point_lights", kPointLightBufferTextureUnit);
  std::array<float, kPointShadowTierCount> texels{};
  // Une face couvre deux unités de large à une unité de la lumière.
  constexpr float kFaceWidthAtUnitDistance = 2.0F;
  for (std::size_t tier = 0; tier < kPointShadowTierCount; ++tier) {
    program.SetInt(kTierSamplers[tier], kPointShadowFirstTextureUnit + static_cast<int>(tier));
    texels[tier] = kFaceWidthAtUnitDistance / static_cast<float>(kPointShadowTierTexels[tier]);
  }
  program.SetFloatArray("u_point_shadow_texel", std::span(texels));
  program.SetFloat("u_point_shadow_clearance", kPointShadowClearanceWorld);
  if (lit) {
    set.stage->Bind();
  }
}

}  // namespace ro::render

#include "ro/render/point_light_shader.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include <glad/glad.h>

#include <glm/glm.hpp>

#include "ro/render/fog_shader.h"
#include "ro/render/point_shadow.h"
#include "ro/render/point_shadow_maps.h"

namespace ro::render {
namespace {

constexpr std::string_view kGlslLightBound = "const int kMaxPointLights = ";
constexpr std::string_view kGlslShadowBound = "const int kMaxShadowedPointLights = ";
constexpr std::size_t kDecimalBase = 10;

// Le nom du lecteur de la carte cubique de rang `rank`.
constexpr std::array<std::string_view, kMaxShadowedPointLights> kShadowSamplers = {
    "u_point_shadow_0", "u_point_shadow_1"};

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

// 🔴 LES TABLEAUX DU GLSL ONT LA TAILLE DE CEUX DU C++ : sinon les uniformes
// s'écriraient au-delà, et OpenGL le refuserait sans bruit. Il a de même une
// carte cubique par lumière ombrée.
static_assert(GlslBound(kGlslLightBound) == kMaxPointLights,
              "la borne GLSL des lumières ponctuelles diffère de kMaxPointLights");
static_assert(GlslBound(kGlslShadowBound) == kMaxShadowedPointLights,
              "la borne GLSL des lumières ombrées diffère de kMaxShadowedPointLights");

}  // namespace

std::string WithPointLights(std::string_view shader, std::string_view points) {
  // L'insertion après `#version` est la même que celle du brouillard.
  return WithFog(shader, points);
}

std::string WithPointLights(std::string_view shader, std::string_view points,
                            std::string_view lighting) {
  return WithFog(WithFog(shader, points), lighting);
}

void SetPointLightUniforms(const GlProgram& program, const PointLightSet& set) {
  const std::size_t count = std::min(set.count, kMaxPointLights);
  std::array<glm::vec3, kMaxPointLights> positions{};
  std::array<glm::vec3, kMaxPointLights> colors{};
  std::array<float, kMaxPointLights> ranges{};
  for (std::size_t rank = 0; rank < count; ++rank) {
    positions[rank] = set.lights[rank].world_position;
    colors[rank] = set.lights[rank].color;
    ranges[rank] = set.lights[rank].range_world;
  }
  program.SetInt("u_point_count", static_cast<int>(count));
  program.SetVector3Array("u_point_position", std::span(positions).first(count));
  program.SetVector3Array("u_point_color", std::span(colors).first(count));
  program.SetFloatArray("u_point_range", std::span(ranges).first(count));

  const bool maps = set.shadow_maps != nullptr && set.shadow_maps->valid();
  const std::size_t shadowed =
      maps ? std::min({set.shadowed_count, count, kMaxShadowedPointLights}) : 0;
  std::array<float, kMaxShadowedPointLights> fars{};
  std::array<float, kMaxShadowedPointLights> depths{};
  std::array<float, kMaxShadowedPointLights> spreads{};
  for (std::size_t rank = 0; rank < kMaxShadowedPointLights; ++rank) {
    const int unit = kPointShadowFirstTextureUnit + static_cast<int>(rank);
    program.SetInt(kShadowSamplers[rank], unit);
    if (rank < shadowed) {
      fars[rank] = PointShadowFar(set.lights[rank].range_world);
      depths[rank] = std::clamp(set.lights[rank].shadow_depth, 0.0F, 1.0F);
      spreads[rank] = std::max(set.lights[rank].shadow_spread, 0.0F);
      set.shadow_maps->Bind(unit, rank);
    }
  }
  program.SetFloatArray("u_point_shadow_depth", std::span(depths));
  program.SetFloatArray("u_point_shadow_spread", std::span(spreads));
  if (shadowed > 0) {
    glActiveTexture(GL_TEXTURE0);
  }
  program.SetInt("u_point_shadowed", static_cast<int>(shadowed));
  program.SetFloatArray("u_point_shadow_far", std::span(fars));
  program.SetFloat("u_point_shadow_clearance", kPointShadowClearanceWorld);
  // Une face couvre deux unités de large à une unité de la lumière.
  constexpr float kFaceWidthAtUnitDistance = 2.0F;
  program.SetFloat(
      "u_point_shadow_texel",
      maps ? kFaceWidthAtUnitDistance / static_cast<float>(set.shadow_maps->side_texels()) : 0.0F);
}

}  // namespace ro::render

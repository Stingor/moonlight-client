#include "ro/render/point_light_shader.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/fog_shader.h"

namespace ro::render {
namespace {

constexpr std::string_view kGlslBoundDeclaration = "const int kMaxPointLights = ";
constexpr std::size_t kDecimalBase = 10;

// La borne que le GLSL déclare, lue dans son texte ; zéro s'il n'en déclare pas.
constexpr std::size_t GlslPointLightBound() {
  const std::size_t at = kPointLightFragmentGlsl.find(kGlslBoundDeclaration);
  if (at == std::string_view::npos) {
    return 0;
  }
  std::size_t bound = 0;
  for (const char digit : kPointLightFragmentGlsl.substr(at + kGlslBoundDeclaration.size())) {
    if (digit < '0' || digit > '9') {
      break;
    }
    bound = bound * kDecimalBase + static_cast<std::size_t>(digit - '0');
  }
  return bound;
}

// 🔴 LES TABLEAUX DU GLSL ONT LA TAILLE DE CEUX DU C++ : sinon les uniformes
// s'écriraient au-delà, et OpenGL le refuserait sans bruit.
static_assert(GlslPointLightBound() == kMaxPointLights,
              "la borne GLSL des lumières ponctuelles diffère de kMaxPointLights");

}  // namespace

std::string WithPointLights(std::string_view shader, std::string_view points) {
  // L'insertion après `#version` est la même que celle du brouillard.
  return WithFog(shader, points);
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
}

}  // namespace ro::render

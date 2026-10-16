#include "ro/render/void_shade.h"

#include <array>
#include <string_view>
#include <utility>

#include <glad/glad.h>

namespace ro::render {
namespace {

// UN TRIANGLE QUI COUVRE L'ÉCRAN, tiré du seul numéro de sommet, posé à la
// profondeur la plus lointaine : celle que l'effacement a laissée.
constexpr std::string_view kVertexShader = R"GLSL(
#version 330 core
void main() {
    vec2 corner = vec2(float((gl_VertexID & 1) << 2), float((gl_VertexID & 2) << 1));
    gl_Position = vec4(corner - 1.0, 1.0, 1.0);
}
)GLSL";

constexpr std::string_view kFragmentShader = R"GLSL(
#version 330 core
uniform vec3 u_daylight;
out vec4 frag_color;
void main() {
    frag_color = vec4(u_daylight, 1.0);
}
)GLSL";

// LE DISQUE ET SON HALO, calculés au pixel : la distance au centre, en rayons
// du disque, à l'échelle de la hauteur de l'écran.
constexpr std::string_view kBodyFragmentShader = R"GLSL(
#version 330 core
uniform vec4 u_viewport;
uniform vec2 u_center;
uniform float u_radius;
uniform float u_aspect;
uniform float u_visibility;
uniform vec3 u_core;
uniform vec3 u_glow;
uniform float u_glow_reach;
out vec4 frag_color;
void main() {
    vec2 ndc = (gl_FragCoord.xy - u_viewport.xy) / u_viewport.zw * 2.0 - 1.0;
    float distance = length((ndc - u_center) * vec2(u_aspect, 1.0)) / u_radius;
    // Le bord du disque se fond sur un dixième de rayon.
    float disc = 1.0 - smoothstep(0.9, 1.0, distance);
    float glow = exp(-distance / u_glow_reach);
    frag_color = vec4((u_core * disc + u_glow * glow) * u_visibility, 1.0);
}
)GLSL";

constexpr GLsizei kFullScreenVertices = 3;
constexpr int kViewportValues = 4;

// LE SOLEIL, blanc chaud dans un halo doré qui porte loin ; LA LUNE, pâle et
// bleutée, au halo court.
constexpr glm::vec3 kSunCore{1.0F, 0.97F, 0.85F};
constexpr glm::vec3 kSunGlow{0.55F, 0.42F, 0.2F};
constexpr float kSunGlowReach = 1.6F;
constexpr glm::vec3 kMoonCore{0.78F, 0.84F, 0.95F};
constexpr glm::vec3 kMoonGlow{0.12F, 0.16F, 0.26F};
constexpr float kMoonGlowReach = 0.8F;

}  // namespace

VoidShade::VoidShade(VoidShade&& other) noexcept
    : program_(std::move(other.program_)),
      body_program_(std::move(other.body_program_)),
      vertex_array_(std::exchange(other.vertex_array_, 0)) {}

VoidShade& VoidShade::operator=(VoidShade&& other) noexcept {
  if (this != &other) {
    program_ = std::move(other.program_);
    body_program_ = std::move(other.body_program_);
    std::swap(vertex_array_, other.vertex_array_);
  }
  return *this;
}

VoidShade::~VoidShade() {
  if (vertex_array_ != 0) {
    glDeleteVertexArrays(1, &vertex_array_);
  }
}

core::Result<VoidShade> VoidShade::Create() {
  auto program = GlProgram::Link(kVertexShader, kFragmentShader);
  if (!program) {
    return std::unexpected(program.error());
  }
  auto body_program = GlProgram::Link(kVertexShader, kBodyFragmentShader);
  if (!body_program) {
    return std::unexpected(body_program.error());
  }
  VoidShade shade;
  shade.program_ = std::move(*program);
  shade.body_program_ = std::move(*body_program);
  glGenVertexArrays(1, &shade.vertex_array_);
  return shade;
}

void VoidShade::Draw(const glm::vec3& daylight) const {
  if (vertex_array_ == 0 || daylight == glm::vec3(1.0F)) {
    return;
  }
  // ⚠ « INFÉRIEUR OU ÉGAL » contre une profondeur effacée à un : seuls les
  // pixels que rien n'a couverts passent.
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  // La destination, multipliée par la couleur peinte.
  glBlendFunc(GL_DST_COLOR, GL_ZERO);
  program_.Use();
  program_.SetVector3("u_daylight", daylight);
  glBindVertexArray(vertex_array_);
  glDrawArrays(GL_TRIANGLES, 0, kFullScreenVertices);
  glBindVertexArray(0);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
}

void VoidShade::DrawBody(const SkyBodyOnScreen& body, bool moon, float aspect_ratio) const {
  if (vertex_array_ == 0 || body.visibility <= 0.0F || body.radius <= 0.0F) {
    return;
  }
  std::array<GLint, kViewportValues> viewport{};
  glGetIntegerv(GL_VIEWPORT, viewport.data());
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  // AJOUTÉ à ce qui est peint : la lumière d'un astre ne cache pas le ciel.
  glBlendFunc(GL_ONE, GL_ONE);
  body_program_.Use();
  body_program_.SetVector4("u_viewport",
                           glm::vec4(viewport[0], viewport[1], viewport[2], viewport[3]));
  body_program_.SetVector2("u_center", body.center);
  body_program_.SetFloat("u_radius", body.radius);
  body_program_.SetFloat("u_aspect", aspect_ratio);
  body_program_.SetFloat("u_visibility", body.visibility);
  body_program_.SetVector3("u_core", moon ? kMoonCore : kSunCore);
  body_program_.SetVector3("u_glow", moon ? kMoonGlow : kSunGlow);
  body_program_.SetFloat("u_glow_reach", moon ? kMoonGlowReach : kSunGlowReach);
  glBindVertexArray(vertex_array_);
  glDrawArrays(GL_TRIANGLES, 0, kFullScreenVertices);
  glBindVertexArray(0);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
}

}  // namespace ro::render

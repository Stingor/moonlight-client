#include "ro/render/ground_quad_renderer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glad/glad.h>

#include "ro/render/fog_shader.h"

namespace ro::render {
namespace {

// Le biais de profondeur est posé ICI, dans le nuanceur, et non par un
// glPolygonOffset : celui-ci travaille en unités du tampon, dont la taille
// dépend du pilote, quand le client raisonne en profondeur normalisée. Le
// multiplier par w le fait survivre à la division perspective qui suit.
constexpr std::string_view kVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texture_uv;
layout(location = 2) in vec4 a_color;

uniform mat4 u_view_projection;
uniform float u_depth_bias;

out vec2 v_texture_uv;
out vec4 v_color;
out float v_fog_kept;

void main() {
    v_texture_uv = a_texture_uv;
    v_color = a_color;
    vec4 drawn = u_view_projection * vec4(a_position, 1.0);
    gl_Position = vec4(drawn.xy, drawn.z - u_depth_bias * drawn.w, drawn.w);
    v_fog_kept = FogKeptPart(drawn.w);
}
)GLSL";

// Le même rejet des texels vides que pour les effets : le client passe ses
// primitives mélangées avec un test d'alpha de seuil un. Sans lui, le filtrage du bord
// d'une équerre laisserait un liseré là où la texture ne dit rien.
constexpr std::string_view kFragmentShader = R"GLSL(
#version 330 core
in vec2 v_texture_uv;
in vec4 v_color;
in float v_fog_kept;

uniform sampler2D u_texture;

out vec4 frag_color;

const float kAlphaReference = 1.0 / 255.0;

void main() {
    vec4 color = texture(u_texture, v_texture_uv) * v_color;
    if (color.a < kAlphaReference) {
        discard;
    }
    frag_color = vec4(Fogged(color.rgb, v_fog_kept), color.a);
}
)GLSL";

struct GroundVertex {
  glm::vec3 position;
  glm::vec2 texture_uv;
  std::array<std::uint8_t, 4> color;
};

// Deux triangles par quad, dans l'ordre des coins de GroundQuad.
constexpr std::array<std::size_t, 6> kTriangleOrder = {0, 1, 3, 0, 3, 2};

}  // namespace

GroundQuadRenderer::GroundQuadRenderer(GroundQuadRenderer&& other) noexcept
    : program_(std::move(other.program_)),
      texture_(std::move(other.texture_)),
      textures_(std::move(other.textures_)),
      vertex_array_(other.vertex_array_),
      vertex_buffer_(other.vertex_buffer_) {
  other.vertex_array_ = 0;
  other.vertex_buffer_ = 0;
}

GroundQuadRenderer& GroundQuadRenderer::operator=(GroundQuadRenderer&& other) noexcept {
  if (this != &other) {
    Release();
    program_ = std::move(other.program_);
    texture_ = std::move(other.texture_);
    textures_ = std::move(other.textures_);
    vertex_array_ = other.vertex_array_;
    vertex_buffer_ = other.vertex_buffer_;
    other.vertex_array_ = 0;
    other.vertex_buffer_ = 0;
  }
  return *this;
}

GroundQuadRenderer::~GroundQuadRenderer() { Release(); }

void GroundQuadRenderer::Release() noexcept {
  if (vertex_buffer_ != 0) {
    glDeleteBuffers(1, &vertex_buffer_);
    vertex_buffer_ = 0;
  }
  if (vertex_array_ != 0) {
    glDeleteVertexArrays(1, &vertex_array_);
    vertex_array_ = 0;
  }
}

core::Result<GroundQuadRenderer> GroundQuadRenderer::Create() {
  auto program = GlProgram::Link(WithFog(kVertexShader, kFogVertexGlsl),
                                 WithFog(kFragmentShader, kFogFragmentGlsl));
  if (!program) {
    return std::unexpected(program.error());
  }

  GroundQuadRenderer renderer;
  renderer.program_ = std::move(*program);
  glGenVertexArrays(1, &renderer.vertex_array_);
  glGenBuffers(1, &renderer.vertex_buffer_);

  glBindVertexArray(renderer.vertex_array_);
  glBindBuffer(GL_ARRAY_BUFFER, renderer.vertex_buffer_);
  const auto stride = static_cast<GLsizei>(sizeof(GroundVertex));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(GroundVertex, position)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(GroundVertex, texture_uv)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                        reinterpret_cast<void*>(offsetof(GroundVertex, color)));
  glBindVertexArray(0);
  return renderer;
}

core::Result<void> GroundQuadRenderer::UseTexture(const formats::Image& image) {
  // kSprite : une image SEULE, adressée d'un bord à l'autre. Elle ne se répète
  // pas — au bord, le filtrage irait chercher l'autre extrémité — et ses pixels
  // transparents reçoivent la couleur opaque voisine, faute de quoi le contour
  // des équerres se noircirait.
  auto uploaded = GlTexture::Upload(image, GlTexture::Filtering::kSprite);
  if (!uploaded) {
    return std::unexpected(uploaded.error());
  }
  texture_ = std::move(*uploaded);
  return {};
}

const GlTexture* GroundQuadRenderer::TextureFor(const std::string& path,
                                                const formats::Image& image) {
  const auto known = textures_.find(path);
  if (known != textures_.end()) {
    return known->second.valid() ? &known->second : nullptr;
  }
  auto uploaded = GlTexture::Upload(image, GlTexture::Filtering::kSprite);
  if (!uploaded) {
    // ⚠ L'ÉCHEC SE RETIENT AUSSI, par une texture invalide : sans quoi une image
    // illisible serait retentée à chaque image, et la panne coûterait le prix
    // d'un téléversement par image sans jamais rien montrer.
    textures_.emplace(path, GlTexture{});
    return nullptr;
  }
  const auto stored = textures_.emplace(path, std::move(*uploaded)).first;
  return &stored->second;
}

void GroundQuadRenderer::Draw(const Camera& camera, std::span<const DrawnGroundQuads> lots,
                              float aspect_ratio, const SceneFog& fog) {
  for (const DrawnGroundQuads& lot : lots) {
    if (lot.quads.empty() || lot.texture == nullptr || lot.texture_path == nullptr) {
      continue;
    }
    const GlTexture* texture = TextureFor(*lot.texture_path, *lot.texture);
    if (texture == nullptr) {
      continue;
    }
    Paint(camera, lot.quads, *texture, aspect_ratio, fog);
  }
}

void GroundQuadRenderer::Draw(const Camera& camera, std::span<const GroundQuad> quads,
                              float aspect_ratio, const SceneFog& fog) {
  if (quads.empty() || !ready()) {
    return;
  }
  Paint(camera, quads, texture_, aspect_ratio, fog);
}

void GroundQuadRenderer::Paint(const Camera& camera, std::span<const GroundQuad> quads,
                               const GlTexture& texture, float aspect_ratio, const SceneFog& fog) {
  std::vector<GroundVertex> vertices;
  vertices.reserve(quads.size() * kTriangleOrder.size());
  for (const GroundQuad& quad : quads) {
    for (const std::size_t corner : kTriangleOrder) {
      vertices.push_back(GroundVertex{quad.corners[corner], quad.texture_uv[corner], quad.color});
    }
  }

  glBindVertexArray(vertex_array_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(GroundVertex)),
               vertices.data(), GL_STREAM_DRAW);

  // La profondeur s'ÉPROUVE mais ne s'ÉCRIT pas : la marque doit disparaître
  // derrière un mur du décor, et ne doit rien masquer de ce qui vient après
  // elle.
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  // Le mélange du client pour ces quads : la source par son alpha, la
  // destination par le complément.
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  program_.Use();
  program_.SetMatrix4("u_view_projection",
                      camera.ProjectionMatrix(aspect_ratio) * camera.ViewMatrix());
  program_.SetInt("u_texture", 0);
  program_.SetFloat("u_depth_bias", kGroundQuadDepthBias);
  SetFogUniforms(program_, fog);
  texture.Bind(0);
  glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));

  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glBindVertexArray(0);
}

}  // namespace ro::render

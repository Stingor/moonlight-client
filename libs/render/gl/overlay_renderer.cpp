#include "ro/render/overlay_renderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include <glad/glad.h>

#include "ro/formats/bmp.h"

namespace ro::render {
namespace {

// Le nuanceur convertit lui-même les pixels de fenêtre en coordonnées de
// découpage : l'appelant n'a donc aucune matrice à fournir, et le repère qu'il
// manipule est celui de la souris.
constexpr std::string_view kVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec2 a_pixel;
layout(location = 1) in vec2 a_texture_uv;
layout(location = 2) in vec4 a_tint;

uniform vec2 u_viewport;

out vec2 v_texture_uv;
out vec4 v_tint;

void main() {
    v_texture_uv = a_texture_uv;
    v_tint = a_tint;
    // De [0, largeur] x [0, hauteur], ordonnée vers le BAS, vers [-1, 1] x
    // [-1, 1], ordonnée vers le haut.
    vec2 ndc = vec2(a_pixel.x / u_viewport.x, 1.0 - a_pixel.y / u_viewport.y) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)GLSL";

constexpr std::string_view kFragmentShader = R"GLSL(
#version 330 core
in vec2 v_texture_uv;
in vec4 v_tint;

uniform sampler2D u_texture;

out vec4 frag_color;

void main() {
    vec4 texel = texture(u_texture, v_texture_uv) * v_tint;
    if (texel.a < 0.01) {
        discard;
    }
    frag_color = texel;
}
)GLSL";

constexpr std::size_t kVerticesPerQuad = 6;

}  // namespace

struct OverlayRenderer::OverlayVertex {
  glm::vec2 pixel;
  glm::vec2 texture_uv;
  std::array<std::uint8_t, 4> tint;
};

OverlayRenderer::OverlayRenderer() = default;

OverlayRenderer::OverlayRenderer(OverlayRenderer&& other) noexcept
    : program_(std::move(other.program_)),
      white_(std::move(other.white_)),
      textures_(std::move(other.textures_)),
      atlases_(std::move(other.atlases_)),
      vertex_array_(other.vertex_array_),
      vertex_buffer_(other.vertex_buffer_),
      quad_textures_(std::move(other.quad_textures_)),
      vertices_(std::move(other.vertices_)) {
  other.vertex_array_ = 0;
  other.vertex_buffer_ = 0;
}

OverlayRenderer& OverlayRenderer::operator=(OverlayRenderer&& other) noexcept {
  if (this != &other) {
    Release();
    program_ = std::move(other.program_);
    white_ = std::move(other.white_);
    textures_ = std::move(other.textures_);
    atlases_ = std::move(other.atlases_);
    vertex_array_ = other.vertex_array_;
    vertex_buffer_ = other.vertex_buffer_;
    quad_textures_ = std::move(other.quad_textures_);
    vertices_ = std::move(other.vertices_);
    other.vertex_array_ = 0;
    other.vertex_buffer_ = 0;
  }
  return *this;
}

OverlayRenderer::~OverlayRenderer() { Release(); }

void OverlayRenderer::Release() noexcept {
  if (vertex_buffer_ != 0) {
    glDeleteBuffers(1, &vertex_buffer_);
    vertex_buffer_ = 0;
  }
  if (vertex_array_ != 0) {
    glDeleteVertexArrays(1, &vertex_array_);
    vertex_array_ = 0;
  }
}

core::Result<OverlayRenderer> OverlayRenderer::Create() {
  auto program = GlProgram::Link(kVertexShader, kFragmentShader);
  if (!program) {
    return std::unexpected(program.error());
  }

  OverlayRenderer renderer;
  renderer.program_ = std::move(*program);
  glGenVertexArrays(1, &renderer.vertex_array_);
  glGenBuffers(1, &renderer.vertex_buffer_);

  glBindVertexArray(renderer.vertex_array_);
  glBindBuffer(GL_ARRAY_BUFFER, renderer.vertex_buffer_);
  const auto stride = static_cast<GLsizei>(sizeof(OverlayVertex));
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(OverlayVertex, pixel)));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void*>(offsetof(OverlayVertex, texture_uv)));
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                        reinterpret_cast<void*>(offsetof(OverlayVertex, tint)));
  glBindVertexArray(0);
  return renderer;
}

const GlTexture* OverlayRenderer::TextureFor(const ScreenQuad& quad) {
  if (quad.atlas != nullptr) {
    auto found = atlases_.find(quad.atlas);
    if (found == atlases_.end()) {
      GlTexture texture;
      if (auto uploaded = GlTexture::Upload(*quad.atlas, GlTexture::Filtering::kAtlas); uploaded) {
        texture = std::move(*uploaded);
      }
      found = atlases_.emplace(quad.atlas, std::move(texture)).first;
    }
    return found->second.valid() ? &found->second : nullptr;
  }
  const SpriteImageKey& key = quad.source;
  // Sans sprite, le quad est uni : une image d'un pixel blanc lui donne sa
  // teinte sans qu'il faille un second nuanceur.
  if (key.sprite == nullptr) {
    if (!white_.valid()) {
      formats::Image pixel;
      pixel.width = 1;
      pixel.height = 1;
      pixel.pixels = {255, 255, 255, 255};
      if (auto uploaded = GlTexture::Upload(pixel, GlTexture::Filtering::kAtlas); uploaded) {
        white_ = std::move(*uploaded);
      }
    }
    return white_.valid() ? &white_ : nullptr;
  }
  auto found = textures_.find(key);
  if (found == textures_.end()) {
    auto uploaded = GlTexture::Upload(ResolveImage(key), GlTexture::Filtering::kSprite);
    CachedTexture cached;
    if (uploaded) {
      cached.texture = std::move(*uploaded);
    }
    found = textures_.emplace(key, std::move(cached)).first;
  }
  found->second.used = true;
  return found->second.texture.valid() ? &found->second.texture : nullptr;
}

void OverlayRenderer::ForgetAtlas(const formats::Image& atlas) { atlases_.erase(&atlas); }

void OverlayRenderer::ForgetUnusedTextures() {
  std::erase_if(textures_, [](const auto& entry) { return !entry.second.used; });
  for (auto& [key, cached] : textures_) {
    cached.used = false;
  }
}

void OverlayRenderer::Draw(std::span<const ScreenQuad> quads, float viewport_width,
                           float viewport_height) {
  if (quads.empty() || viewport_width <= 0.0F || viewport_height <= 0.0F) {
    return;
  }

  quad_textures_.clear();
  vertices_.clear();
  quad_textures_.reserve(quads.size());
  vertices_.reserve(quads.size() * kVerticesPerQuad);
  for (const ScreenQuad& quad : quads) {
    const GlTexture* texture = TextureFor(quad);
    if (texture == nullptr) {
      continue;
    }
    quad_textures_.push_back(texture);
    const float left_u = quad.mirrored ? quad.uv_max.x : quad.uv_min.x;
    const float right_u = quad.mirrored ? quad.uv_min.x : quad.uv_max.x;
    const std::array<glm::vec2, 4> corners = {
        glm::vec2{quad.min.x, quad.min.y}, glm::vec2{quad.max.x, quad.min.y},
        glm::vec2{quad.max.x, quad.max.y}, glm::vec2{quad.min.x, quad.max.y}};
    const std::array<glm::vec2, 4> uv = {
        glm::vec2{left_u, quad.uv_min.y}, glm::vec2{right_u, quad.uv_min.y},
        glm::vec2{right_u, quad.uv_max.y}, glm::vec2{left_u, quad.uv_max.y}};
    constexpr std::array<std::size_t, kVerticesPerQuad> kOrder = {0, 1, 2, 0, 2, 3};
    for (const std::size_t corner : kOrder) {
      vertices_.push_back(OverlayVertex{corners[corner], uv[corner], quad.tint});
    }
  }
  if (vertices_.empty()) {
    return;
  }

  glBindVertexArray(vertex_array_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(OverlayVertex)),
               vertices_.data(), GL_STREAM_DRAW);

  // Rien de ce qui est peint ici n'appartient au monde : ni test ni écriture de
  // profondeur, et l'ordre donné décide seul.
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  program_.Use();
  program_.SetVector2("u_viewport", glm::vec2{viewport_width, viewport_height});
  program_.SetInt("u_texture", 0);

  // ⚠ LES QUADS VOISINS D'UNE MÊME TEXTURE PARTENT ENSEMBLE : l'ordre ne
  // change pas, seul le nombre d'appels baisse — les glyphes d'un texte en sont
  // le cas type.
  for (std::size_t first = 0; first < quad_textures_.size();) {
    std::size_t end = first + 1;
    while (end < quad_textures_.size() && quad_textures_[end] == quad_textures_[first]) {
      ++end;
    }
    quad_textures_[first]->Bind(0);
    glDrawArrays(GL_TRIANGLES, static_cast<GLint>(first * kVerticesPerQuad),
                 static_cast<GLsizei>((end - first) * kVerticesPerQuad));
    first = end;
  }

  glBindVertexArray(0);
  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
}

}  // namespace ro::render

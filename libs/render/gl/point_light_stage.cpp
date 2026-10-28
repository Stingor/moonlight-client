#include "ro/render/point_light_stage.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include <glad/glad.h>

#include "ro/render/point_light_shader.h"

namespace ro::render {
namespace {

using core::ErrorCode;
using core::Fail;

// LE DÉCALAGE DES FACES PEINTES, comme celui du soleil : proportionnel à leur
// pente vue de la lumière, plus un plancher en crans de profondeur.
constexpr float kDepthSlopeOffset = 1.0F;
constexpr float kDepthUnitOffset = 2.0F;

// Une profondeur de 24 bits occupe quatre octets par texel.
constexpr std::size_t kDepthTexelBytes = 4;

// LE PREMIER TABLEAU d'un palier tient tant de cartes, puis double.
constexpr std::size_t kFirstLayers = 2;

[[nodiscard]] GLsizei FaceLayers(std::size_t layers) {
  return static_cast<GLsizei>(layers * kCubeFaceCount);
}

// Un tableau de `layers` cartes cubiques de profondeur, de `side` texels.
[[nodiscard]] unsigned int CreateCubeArray(int side, std::size_t layers) {
  unsigned int texture = 0;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, texture);
  glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT24, side, side, FaceLayers(layers),
               0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
  // LA COMPARAISON SE FAIT À LA LECTURE, mêlée aux texels voisins.
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
  return texture;
}

}  // namespace

PointLightStage::PointLightStage(PointLightStage&& other) noexcept { *this = std::move(other); }

PointLightStage& PointLightStage::operator=(PointLightStage&& other) noexcept {
  if (this != &other) {
    std::swap(framebuffer_, other.framebuffer_);
    std::swap(light_buffer_, other.light_buffer_);
    std::swap(light_texture_, other.light_texture_);
    std::swap(uploaded_lights_, other.uploaded_lights_);
    std::swap(tier_textures_, other.tier_textures_);
    std::swap(tier_layers_, other.tier_layers_);
    std::swap(cubes_, other.cubes_);
    std::swap(slots_, other.slots_);
    std::swap(scene_, other.scene_);
    std::swap(generation_, other.generation_);
    std::swap(frame_, other.frame_);
  }
  return *this;
}

PointLightStage::~PointLightStage() {
  for (const unsigned int texture : tier_textures_) {
    if (texture != 0) {
      glDeleteTextures(1, &texture);
    }
  }
  if (light_texture_ != 0) {
    glDeleteTextures(1, &light_texture_);
  }
  if (light_buffer_ != 0) {
    glDeleteBuffers(1, &light_buffer_);
  }
  if (framebuffer_ != 0) {
    glDeleteFramebuffers(1, &framebuffer_);
  }
}

core::Result<PointLightStage> PointLightStage::Create() {
  PointLightStage stage;
  // UN FILTRAGE QUI ENJAMBE LES ARÊTES DU CUBE : sans lui, la couture de deux
  // faces se lit comme une ligne d'ombre.
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  glGenBuffers(1, &stage.light_buffer_);
  glBindBuffer(GL_TEXTURE_BUFFER, stage.light_buffer_);
  glBufferData(GL_TEXTURE_BUFFER, static_cast<GLsizeiptr>(sizeof(glm::vec4)), nullptr,
               GL_STREAM_DRAW);
  glGenTextures(1, &stage.light_texture_);
  glBindTexture(GL_TEXTURE_BUFFER, stage.light_texture_);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, stage.light_buffer_);
  glBindTexture(GL_TEXTURE_BUFFER, 0);
  glBindBuffer(GL_TEXTURE_BUFFER, 0);

  // ⚠ LES CIBLES LIÉES SE RETROUVENT APRÈS, celle de dessin comme celle de
  // lecture : l'étage naît à la première lumière, au milieu d'une image qui
  // peut se peindre hors écran.
  GLint previous_draw_framebuffer = 0;
  GLint previous_read_framebuffer = 0;
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw_framebuffer);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_framebuffer);
  glGenFramebuffers(1, &stage.framebuffer_);
  glBindFramebuffer(GL_FRAMEBUFFER, stage.framebuffer_);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previous_draw_framebuffer));
  glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous_read_framebuffer));
  if (const GLenum error = glGetError(); error != GL_NO_ERROR) {
    return Fail(ErrorCode::kCorruptData, "lumières ponctuelles refusées, erreur OpenGL " +
                                             std::to_string(static_cast<unsigned>(error)));
  }
  return stage;
}

void PointLightStage::Upload(const PointLightSet& set) {
  const std::vector<glm::vec4> texels = PointLightTexelsOf(set);
  glBindBuffer(GL_TEXTURE_BUFFER, light_buffer_);
  // UN TAMPON NEUF À CHAQUE IMAGE : le pilote n'attend pas que l'image
  // précédente ait fini de lire l'ancien.
  glBufferData(GL_TEXTURE_BUFFER,
               static_cast<GLsizeiptr>(std::max<std::size_t>(texels.size(), 1) * sizeof(glm::vec4)),
               texels.empty() ? nullptr : texels.data(), GL_STREAM_DRAW);
  glBindBuffer(GL_TEXTURE_BUFFER, 0);
  uploaded_lights_ = set.lights.size();
}

void PointLightStage::Bind() const {
  for (std::size_t tier = 0; tier < kPointShadowTierCount; ++tier) {
    glActiveTexture(
        static_cast<GLenum>(GL_TEXTURE0 + kPointShadowFirstTextureUnit + static_cast<int>(tier)));
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, tier_textures_[tier]);
  }
  glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + kPointLightBufferTextureUnit));
  glBindTexture(GL_TEXTURE_BUFFER, light_texture_);
  glActiveTexture(GL_TEXTURE0);
}

void PointLightStage::EnsureLayers() {
  for (std::size_t tier = 0; tier < kPointShadowTierCount; ++tier) {
    const std::size_t used = slots_.layers_used(tier);
    cubes_[tier].resize(std::max(cubes_[tier].size(), used));
    if (used <= tier_layers_[tier]) {
      continue;
    }
    std::size_t layers = std::max(tier_layers_[tier], kFirstLayers);
    while (layers < used) {
      layers *= 2;
    }
    layers = std::min(layers, kPointShadowTierMaxLayers.at(tier));
    const int side = kPointShadowTierTexels.at(tier);
    const unsigned int grown = CreateCubeArray(side, layers);
    // CE QUE LES CARTES PORTENT SE GARDE : la copie ne passe pas par le
    // processeur.
    if (tier_textures_[tier] != 0) {
      glCopyImageSubData(tier_textures_[tier], GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 0, grown,
                         GL_TEXTURE_CUBE_MAP_ARRAY, 0, 0, 0, 0, side, side,
                         FaceLayers(tier_layers_[tier]));
      glDeleteTextures(1, &tier_textures_[tier]);
    }
    tier_textures_[tier] = grown;
    tier_layers_[tier] = layers;
  }
}

void PointLightStage::BeginDepthPass() {
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw_framebuffer_);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous_read_framebuffer_);
  glGetIntegerv(GL_VIEWPORT, previous_viewport_);
  // ⚠ UNE CARTE NE DOIT PAS ÊTRE LUE PENDANT QU'ON L'ÉCRIT.
  for (std::size_t tier = 0; tier < kPointShadowTierCount; ++tier) {
    glActiveTexture(
        static_cast<GLenum>(GL_TEXTURE0 + kPointShadowFirstTextureUnit + static_cast<int>(tier)));
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, 0);
  }
  glActiveTexture(GL_TEXTURE0);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  glDepthMask(GL_TRUE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(kDepthSlopeOffset, kDepthUnitOffset);
}

void PointLightStage::BeginFace(std::size_t tier, std::size_t layer, std::size_t face) const {
  glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, tier_textures_.at(tier), 0,
                            static_cast<GLint>((layer * kCubeFaceCount) + face));
  const int side = kPointShadowTierTexels.at(tier);
  glViewport(0, 0, side, side);
  glClear(GL_DEPTH_BUFFER_BIT);
}

void PointLightStage::SetCasterOffset(bool on) const {
  if (on) {
    glEnable(GL_POLYGON_OFFSET_FILL);
  } else {
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
}

void PointLightStage::EndDepthPass() const {
  glDisable(GL_POLYGON_OFFSET_FILL);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previous_draw_framebuffer_));
  glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous_read_framebuffer_));
  glViewport(previous_viewport_[0], previous_viewport_[1], previous_viewport_[2],
             previous_viewport_[3]);
}

std::size_t PointLightStage::shadow_bytes() const noexcept {
  std::size_t bytes = 0;
  for (std::size_t tier = 0; tier < kPointShadowTierCount; ++tier) {
    const auto side = static_cast<std::size_t>(kPointShadowTierTexels[tier]);
    bytes += side * side * kDepthTexelBytes * kCubeFaceCount * tier_layers_[tier];
  }
  return bytes;
}

}  // namespace ro::render

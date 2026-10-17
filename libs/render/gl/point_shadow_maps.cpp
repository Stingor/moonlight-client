#include "ro/render/point_shadow_maps.h"

#include <cstddef>
#include <string>
#include <utility>

#include <glad/glad.h>

namespace ro::render {
namespace {

using core::ErrorCode;
using core::Fail;

constexpr int kMaxSide = 4096;

// LE DÉCALAGE DES FACES PEINTES, comme celui du soleil : proportionnel à leur
// pente vue de la lumière, plus un plancher en crans de profondeur.
constexpr float kDepthSlopeOffset = 1.0F;
constexpr float kDepthUnitOffset = 2.0F;

[[nodiscard]] GLenum FaceTarget(std::size_t face) {
  return static_cast<GLenum>(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face);
}

}  // namespace

PointShadowMaps::PointShadowMaps(PointShadowMaps&& other) noexcept
    : framebuffer_(std::exchange(other.framebuffer_, 0)),
      cubes_(std::exchange(other.cubes_, {})),
      side_texels_(std::exchange(other.side_texels_, 0)) {}

PointShadowMaps& PointShadowMaps::operator=(PointShadowMaps&& other) noexcept {
  if (this != &other) {
    std::swap(framebuffer_, other.framebuffer_);
    std::swap(cubes_, other.cubes_);
    std::swap(side_texels_, other.side_texels_);
  }
  return *this;
}

PointShadowMaps::~PointShadowMaps() {
  if (cubes_.front() != 0) {
    glDeleteTextures(static_cast<GLsizei>(cubes_.size()), cubes_.data());
  }
  if (framebuffer_ != 0) {
    glDeleteFramebuffers(1, &framebuffer_);
  }
}

core::Result<PointShadowMaps> PointShadowMaps::Create(int side_texels) {
  if (side_texels <= 0 || side_texels > kMaxSide) {
    return Fail(ErrorCode::kCorruptData, "carte d'ombre cubique de " + std::to_string(side_texels));
  }
  PointShadowMaps maps;
  maps.side_texels_ = side_texels;
  // UN FILTRAGE QUI ENJAMBE LES ARÊTES DU CUBE : sans lui, la couture de deux
  // faces se lit comme une ligne d'ombre.
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  glGenTextures(static_cast<GLsizei>(maps.cubes_.size()), maps.cubes_.data());
  for (const unsigned int cube : maps.cubes_) {
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    for (std::size_t face = 0; face < kCubeFaceCount; ++face) {
      glTexImage2D(FaceTarget(face), 0, GL_DEPTH_COMPONENT24, side_texels, side_texels, 0,
                   GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    }
    // LA COMPARAISON SE FAIT À LA LECTURE, mêlée aux texels voisins.
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  }
  glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

  glGenFramebuffers(1, &maps.framebuffer_);
  glBindFramebuffer(GL_FRAMEBUFFER, maps.framebuffer_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, FaceTarget(0), maps.cubes_.front(),
                         0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    return Fail(ErrorCode::kCorruptData, "carte d'ombre cubique incomplète, état " +
                                             std::to_string(static_cast<unsigned>(status)));
  }
  return maps;
}

void PointShadowMaps::BeginDepthPass() {
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_framebuffer_);
  glGetIntegerv(GL_VIEWPORT, previous_viewport_);
  // ⚠ UNE CARTE NE DOIT PAS ÊTRE LUE PENDANT QU'ON L'ÉCRIT.
  for (std::size_t rank = 0; rank < cubes_.size(); ++rank) {
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + kPointShadowFirstTextureUnit + rank));
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
  }
  glActiveTexture(GL_TEXTURE0);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
  glViewport(0, 0, side_texels_, side_texels_);
  glDepthMask(GL_TRUE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(kDepthSlopeOffset, kDepthUnitOffset);
}

void PointShadowMaps::BeginFace(std::size_t rank, std::size_t face) const {
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, FaceTarget(face), cubes_.at(rank), 0);
  glClear(GL_DEPTH_BUFFER_BIT);
}

void PointShadowMaps::SetCasterOffset(bool on) const {
  if (on) {
    glEnable(GL_POLYGON_OFFSET_FILL);
  } else {
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
}

void PointShadowMaps::EndDepthPass() const {
  glDisable(GL_POLYGON_OFFSET_FILL);
  glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer_));
  glViewport(previous_viewport_[0], previous_viewport_[1], previous_viewport_[2],
             previous_viewport_[3]);
}

void PointShadowMaps::Bind(int unit, std::size_t rank) const {
  glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
  glBindTexture(GL_TEXTURE_CUBE_MAP, cubes_.at(rank));
}

}  // namespace ro::render

#include "ro/render/sun_shadow_map.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

#include <glad/glad.h>

#include "ro/render/fog_shader.h"

namespace ro::render {
namespace {

using core::ErrorCode;
using core::Fail;

constexpr int kMaxSide = 8192;

// LE DÉCALAGE DES FACES PEINTES DANS LA CARTE : proportionnel à leur pente vue
// du soleil, plus un plancher en crans de profondeur.
constexpr float kDepthSlopeOffset = 2.0F;
constexpr float kDepthUnitOffset = 4.0F;
// Le décalage du point lu le long de sa normale, en texels de la carte.
constexpr float kNormalOffsetTexels = 1.5F;

// La bordure : la profondeur la plus lointaine, que rien n'ombre.
constexpr float kBorderDepth[] = {1.0F, 1.0F, 1.0F, 1.0F};

[[nodiscard]] std::size_t RankOf(SunShadowLayer layer) { return static_cast<std::size_t>(layer); }

}  // namespace

SunShadowMap::SunShadowMap(SunShadowMap&& other) noexcept
    : framebuffers_(std::exchange(other.framebuffers_, {})),
      depths_(std::exchange(other.depths_, {})),
      side_texels_(std::exchange(other.side_texels_, 0)) {}

SunShadowMap& SunShadowMap::operator=(SunShadowMap&& other) noexcept {
  if (this != &other) {
    std::swap(framebuffers_, other.framebuffers_);
    std::swap(depths_, other.depths_);
    std::swap(side_texels_, other.side_texels_);
  }
  return *this;
}

SunShadowMap::~SunShadowMap() {
  if (depths_.front() != 0) {
    glDeleteTextures(static_cast<GLsizei>(depths_.size()), depths_.data());
  }
  if (framebuffers_.front() != 0) {
    glDeleteFramebuffers(static_cast<GLsizei>(framebuffers_.size()), framebuffers_.data());
  }
}

core::Result<SunShadowMap> SunShadowMap::Create(int side_texels) {
  if (side_texels <= 0 || side_texels > kMaxSide) {
    return Fail(ErrorCode::kCorruptData, "carte d'ombre de " + std::to_string(side_texels));
  }
  SunShadowMap map;
  map.side_texels_ = side_texels;

  glGenTextures(static_cast<GLsizei>(map.depths_.size()), map.depths_.data());
  glGenFramebuffers(static_cast<GLsizei>(map.framebuffers_.size()), map.framebuffers_.data());
  for (std::size_t rank = 0; rank < map.depths_.size(); ++rank) {
    glBindTexture(GL_TEXTURE_2D, map.depths_[rank]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, side_texels, side_texels, 0,
                 GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    // LA COMPARAISON SE FAIT À LA LECTURE, et le filtrage linéaire mêle les
    // quatre texels voisins déjà comparés.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, kBorderDepth);

    glBindFramebuffer(GL_FRAMEBUFFER, map.framebuffers_[rank]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, map.depths_[rank],
                           0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glBindTexture(GL_TEXTURE_2D, 0);
      return Fail(ErrorCode::kCorruptData, "carte d'ombre incomplète, état " +
                                               std::to_string(static_cast<unsigned>(status)));
    }
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  return map;
}

void SunShadowMap::BeginDepthPass() {
  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_framebuffer_);
  glGetIntegerv(GL_VIEWPORT, previous_viewport_);
  // ⚠ LA CARTE NE DOIT PAS ÊTRE LUE PENDANT QU'ON L'ÉCRIT.
  glActiveTexture(GL_TEXTURE0 + kSunShadowTextureUnit);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0);

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[RankOf(SunShadowLayer::kWorld)]);
  glViewport(0, 0, side_texels_, side_texels_);
  glDepthMask(GL_TRUE);
  glClear(GL_DEPTH_BUFFER_BIT);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(kDepthSlopeOffset, kDepthUnitOffset);
}

void SunShadowMap::BeginActorLayer() const {
  const GLuint world = framebuffers_[RankOf(SunShadowLayer::kWorld)];
  const GLuint with_actors = framebuffers_[RankOf(SunShadowLayer::kWithActors)];
  glBindFramebuffer(GL_READ_FRAMEBUFFER, world);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, with_actors);
  glBlitFramebuffer(0, 0, side_texels_, side_texels_, 0, 0, side_texels_, side_texels_,
                    GL_DEPTH_BUFFER_BIT, GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, with_actors);
}

void SunShadowMap::SetCasterOffset(bool on) const {
  if (on) {
    glEnable(GL_POLYGON_OFFSET_FILL);
  } else {
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
}

void SunShadowMap::EndDepthPass() const {
  glDisable(GL_POLYGON_OFFSET_FILL);
  glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer_));
  glViewport(previous_viewport_[0], previous_viewport_[1], previous_viewport_[2],
             previous_viewport_[3]);
}

void SunShadowMap::Bind(int unit, SunShadowLayer layer) const {
  glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
  glBindTexture(GL_TEXTURE_2D, depths_[RankOf(layer)]);
}

std::string WithSunShadow(std::string_view shader, std::string_view sun) {
  return WithFog(shader, sun);
}

void SetSunShadowUniforms(const GlProgram& program, const SunShadowView& sun,
                          SunShadowLayer layer) {
  const bool shadows = sun.map != nullptr && sun.map->valid() && sun.strength > 0.0F;
  program.SetInt("u_sun_shadow", kSunShadowTextureUnit);
  program.SetMatrix4("u_sun_view_projection", sun.view_projection);
  program.SetFloat("u_sun_shadow_strength", shadows ? sun.strength : 0.0F);
  program.SetFloat("u_sun_normal_offset", shadows ? sun.texel_world * kNormalOffsetTexels : 0.0F);
  program.SetFloat("u_sun_texel",
                   shadows ? 1.0F / static_cast<float>(sun.map->side_texels()) : 0.0F);
  if (shadows) {
    sun.map->Bind(kSunShadowTextureUnit, layer);
    glActiveTexture(GL_TEXTURE0);
  }
}

void SetSunSpriteShadowUniforms(const GlProgram& program, const SunShadowView& sun) {
  SetSunShadowUniforms(program, sun, SunShadowLayer::kWorld);
  // L'écart entre deux points de lecture, en unités du monde : le rayon,
  // partagé entre les lectures d'un côté.
  constexpr float kTapsPerSide = 2.0F;
  program.SetFloat("u_sun_sprite_step", kSunSpriteShadeRadius / kTapsPerSide);
  // L'OMBRE DES AUTRES ACTEURS, lue dans la couche qui les porte.
  program.SetInt("u_sun_actor_shadow", kSunActorShadowTextureUnit);
  program.SetVector3("u_sun_toward", sun.toward_sun);
  const float ground_cosine = glm::length(glm::vec2(sun.toward_sun.x, sun.toward_sun.z));
  program.SetFloat("u_sun_actor_bias", sun.texel_world * kSunActorBiasTexels /
                                           std::max(ground_cosine, kSunActorBiasLowestCosine));
  if (sun.map != nullptr && sun.map->valid() && sun.strength > 0.0F) {
    sun.map->Bind(kSunActorShadowTextureUnit, SunShadowLayer::kWithActors);
    glActiveTexture(GL_TEXTURE0);
  }
}

}  // namespace ro::render

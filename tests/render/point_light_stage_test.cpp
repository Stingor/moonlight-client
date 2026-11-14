// L'ÉTAGE DES LUMIÈRES PONCTUELLES REND LES CIBLES QU'IL TROUVE : à sa
// naissance, au milieu d'une image, comme autour de sa passe de profondeur, la
// cible de dessin et la cible de lecture se retrouvent chacune liée là où
// l'appelant l'avait mise — même quand il les a liées séparément.
//
// Sans contexte OpenGL (un poste sans carte graphique), l'essai se saute.

#include "ro/render/point_light_stage.h"

#include <glad/glad.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/gl_context.h"
#include "ro/render/offscreen_target.h"

namespace {

constexpr int kSide = 16;

[[nodiscard]] GLint BindingOf(GLenum query) {
  GLint name = 0;
  glGetIntegerv(query, &name);
  return name;
}

// Une cible de lecture nommée, distincte de toute cible de l'étage.
class DistinctReadTarget {
 public:
  DistinctReadTarget() { glGenFramebuffers(1, &framebuffer_); }
  DistinctReadTarget(const DistinctReadTarget&) = delete;
  DistinctReadTarget& operator=(const DistinctReadTarget&) = delete;
  ~DistinctReadTarget() { glDeleteFramebuffers(1, &framebuffer_); }

  [[nodiscard]] GLint name() const { return static_cast<GLint>(framebuffer_); }
  void Bind() const { glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_); }

 private:
  GLuint framebuffer_ = 0;
};

}  // namespace

TEST_CASE("Lumières ponctuelles : l'étage naît et peint en rendant dessin et lecture",
          "[render][light]") {
  auto context = ro::render::GlContext::CreateHeadless(kSide, kSide);
  if (!context) SKIP("pas de contexte OpenGL : " + ro::core::Describe(context.error()));
  auto target = ro::render::OffscreenTarget::Create(kSide, kSide);
  REQUIRE(target.has_value());
  const DistinctReadTarget read_target;

  target->BindAsDrawTarget();
  read_target.Bind();
  const GLint draw_framebuffer = BindingOf(GL_DRAW_FRAMEBUFFER_BINDING);
  REQUIRE(draw_framebuffer != read_target.name());

  auto stage = ro::render::PointLightStage::Create();
  REQUIRE(stage.has_value());
  CHECK(BindingOf(GL_DRAW_FRAMEBUFFER_BINDING) == draw_framebuffer);
  CHECK(BindingOf(GL_READ_FRAMEBUFFER_BINDING) == read_target.name());

  stage->BeginDepthPass();
  stage->EndDepthPass();
  CHECK(BindingOf(GL_DRAW_FRAMEBUFFER_BINDING) == draw_framebuffer);
  CHECK(BindingOf(GL_READ_FRAMEBUFFER_BINDING) == read_target.name());
}

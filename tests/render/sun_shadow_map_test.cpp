// LA CARTE D'OMBRE DU SOLEIL NAÎT ET PEINT SANS DÉTOURNER LA PEINTURE : créée
// pendant qu'une cible hors écran est liée — à la première image qui demande
// l'ombre —, elle laisse cette cible liée, et ce qui se peint ensuite y arrive.
// Sa création rend aussi la cible de LECTURE et la texture 2D trouvées, et sa
// passe de profondeur rend les cibles de dessin et de lecture, chacune la
// sienne : un appelant qui les lie séparément (une capture d'écran) les
// retrouve.
//
// Sans contexte OpenGL (un poste sans carte graphique), l'essai se saute.

#include "ro/render/sun_shadow_map.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include <glad/glad.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/gl_context.h"
#include "ro/render/offscreen_target.h"

namespace {

constexpr int kSide = 16;
constexpr std::size_t kRgbaBytes = 4;
constexpr std::uint8_t kFull = 255;
// Un rouge plein : ni le noir d'une cible jamais peinte, ni le blanc.
constexpr std::array<float, 4> kPaintColor = {1.0F, 0.0F, 0.0F, 1.0F};

[[nodiscard]] GLint BindingOf(GLenum query) {
  GLint name = 0;
  glGetIntegerv(query, &name);
  return name;
}

// Une cible de lecture et une texture 2D nommées, distinctes de tout ce que la
// carte crée, libérées à la sortie.
class DistinctBindings {
 public:
  DistinctBindings() {
    glGenFramebuffers(1, &read_framebuffer_);
    glGenTextures(1, &texture_);
  }
  DistinctBindings(const DistinctBindings&) = delete;
  DistinctBindings& operator=(const DistinctBindings&) = delete;
  ~DistinctBindings() {
    glDeleteTextures(1, &texture_);
    glDeleteFramebuffers(1, &read_framebuffer_);
  }

  [[nodiscard]] GLint read_framebuffer() const { return static_cast<GLint>(read_framebuffer_); }
  [[nodiscard]] GLint texture() const { return static_cast<GLint>(texture_); }

  // La lecture vers la cible nommée, le dessin laissé où il est ; la texture
  // nommée sur l'unité 0.
  void Bind() const {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, read_framebuffer_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
  }

 private:
  GLuint read_framebuffer_ = 0;
  GLuint texture_ = 0;
};

}  // namespace

TEST_CASE("Ombre du soleil : la carte naît sans délier la cible hors écran en cours de peinture",
          "[render][sun][snapshot]") {
  auto context = ro::render::GlContext::CreateHeadless(kSide, kSide);
  if (!context) SKIP("pas de contexte OpenGL : " + ro::core::Describe(context.error()));
  auto target = ro::render::OffscreenTarget::Create(kSide, kSide);
  REQUIRE(target.has_value());

  target->Bind();
  auto map = ro::render::SunShadowMap::Create(ro::render::kSunShadowMapTexels);
  REQUIRE(map.has_value());
  (*context)->ClearBoundTarget(kPaintColor);

  auto painted = target->ReadPixels();
  REQUIRE(painted.has_value());
  REQUIRE(painted->pixels.size() >= kRgbaBytes);
  std::size_t red_pixels = 0;
  for (std::size_t offset = 0; offset + kRgbaBytes <= painted->pixels.size();
       offset += kRgbaBytes) {
    if (painted->pixels[offset] == kFull && painted->pixels[offset + 1] == 0 &&
        painted->pixels[offset + 2] == 0 && painted->pixels[offset + 3] == kFull) {
      ++red_pixels;
    }
  }
  CHECK(red_pixels == painted->pixels.size() / kRgbaBytes);
}

TEST_CASE("Ombre du soleil : la carte naît en rendant la cible de lecture et la texture 2D",
          "[render][sun]") {
  auto context = ro::render::GlContext::CreateHeadless(kSide, kSide);
  if (!context) SKIP("pas de contexte OpenGL : " + ro::core::Describe(context.error()));
  auto target = ro::render::OffscreenTarget::Create(kSide, kSide);
  REQUIRE(target.has_value());
  const DistinctBindings distinct;

  target->BindAsDrawTarget();
  distinct.Bind();
  const GLint draw_framebuffer = BindingOf(GL_DRAW_FRAMEBUFFER_BINDING);
  REQUIRE(draw_framebuffer != distinct.read_framebuffer());

  auto map = ro::render::SunShadowMap::Create(ro::render::kSunShadowMapTexels);
  REQUIRE(map.has_value());

  CHECK(BindingOf(GL_DRAW_FRAMEBUFFER_BINDING) == draw_framebuffer);
  CHECK(BindingOf(GL_READ_FRAMEBUFFER_BINDING) == distinct.read_framebuffer());
  CHECK(BindingOf(GL_TEXTURE_BINDING_2D) == distinct.texture());
}

TEST_CASE("Ombre du soleil : la passe de profondeur rend les cibles de dessin et de lecture",
          "[render][sun]") {
  auto context = ro::render::GlContext::CreateHeadless(kSide, kSide);
  if (!context) SKIP("pas de contexte OpenGL : " + ro::core::Describe(context.error()));
  auto target = ro::render::OffscreenTarget::Create(kSide, kSide);
  REQUIRE(target.has_value());
  auto map = ro::render::SunShadowMap::Create(ro::render::kSunShadowMapTexels);
  REQUIRE(map.has_value());
  const DistinctBindings distinct;

  target->BindAsDrawTarget();
  distinct.Bind();
  const GLint draw_framebuffer = BindingOf(GL_DRAW_FRAMEBUFFER_BINDING);

  map->BeginDepthPass();
  map->BeginActorLayer();
  map->EndDepthPass();

  CHECK(BindingOf(GL_DRAW_FRAMEBUFFER_BINDING) == draw_framebuffer);
  CHECK(BindingOf(GL_READ_FRAMEBUFFER_BINDING) == distinct.read_framebuffer());
}

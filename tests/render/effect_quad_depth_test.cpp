#include "ro/render/effect_quad_depth.h"

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ro/render/effect_breathing_quad.h"
#include "ro/render/effect_particle_quad.h"

using Catch::Approx;
using ro::render::EffectQuadDepth;
using ro::render::EffectWorldQuad;
using ro::render::PlaceEffectQuadVertices;

namespace {

constexpr float kFieldOfViewDegrees = 45.0F;
constexpr float kNear = 1.0F;
constexpr float kFar = 1000.0F;
const glm::vec3 kEye{0.0F, 0.0F, 100.0F};

glm::mat4 ViewAndProjection() {
  return glm::perspective(glm::radians(kFieldOfViewDegrees), 1.0F, kNear, kFar) *
         glm::lookAt(kEye, glm::vec3{0.0F, 0.0F, 0.0F}, glm::vec3{0.0F, 1.0F, 0.0F});
}

glm::vec3 Screen(const glm::mat4& matrice, glm::vec3 point) {
  const glm::vec4 clip = matrice * glm::vec4(point, 1.0F);
  return glm::vec3(clip) / clip.w;
}

EffectWorldQuad Square(EffectQuadDepth depth) {
  EffectWorldQuad quad;
  quad.corners = {glm::vec3{-5.0F, 5.0F, 0.0F}, glm::vec3{5.0F, 5.0F, 0.0F},
                  glm::vec3{5.0F, -5.0F, 0.0F}, glm::vec3{-5.0F, -5.0F, 0.0F}};
  quad.depth = depth;
  return quad;
}

}  // namespace

TEST_CASE("un quad projeté garde ses coins", "[render][quad_depth]") {
  const EffectWorldQuad quad = Square(EffectQuadDepth::kProjected);
  const auto sommets = PlaceEffectQuadVertices(quad, ViewAndProjection(), kEye);
  REQUIRE(sommets.has_value());
  CHECK((*sommets)[1].position == quad.corners[1]);
  CHECK((*sommets)[5].depth_position == quad.corners[3]);
  CHECK_FALSE(ro::render::EffectQuadDepthIsFrontmost(EffectQuadDepth::kProjected));
  CHECK(ro::render::EffectQuadDepthIsFrontmost(EffectQuadDepth::kFrontmost));
}

TEST_CASE("un quad avancé gagne un souffle sur l'inverse de sa profondeur",
          "[render][quad_depth]") {
  const glm::mat4 matrice = ViewAndProjection();
  const auto sommets = PlaceEffectQuadVertices(Square(EffectQuadDepth::kNudged), matrice, kEye);
  REQUIRE(sommets.has_value());
  const auto& vertex = (*sommets)[0];
  const float w = (matrice * glm::vec4(vertex.position, 1.0F)).w;
  const float advanced_w = (matrice * glm::vec4(vertex.depth_position, 1.0F)).w;
  CHECK(1.0F / advanced_w == Approx(1.0F / w + ro::render::kEffectQuadDepthNudge));
  // L'écran ne voit rien bouger.
  CHECK(Screen(matrice, vertex.depth_position).x == Approx(Screen(matrice, vertex.position).x));
  CHECK(Screen(matrice, vertex.depth_position).y == Approx(Screen(matrice, vertex.position).y));
}

TEST_CASE("un quad recentré se pose sur l'abscisse de son ancre, levé d'un seizième",
          "[render][quad_depth]") {
  const glm::mat4 matrice = ViewAndProjection();
  EffectWorldQuad quad = Square(EffectQuadDepth::kScreenRecentred);
  quad.screen_anchor = {20.0F, 0.0F, 0.0F};
  const auto sommets = PlaceEffectQuadVertices(quad, matrice, kEye);
  REQUIRE(sommets.has_value());
  const float anchor = Screen(matrice, quad.screen_anchor).x;
  const float half =
      std::abs(Screen(matrice, quad.corners[0]).x - Screen(matrice, quad.corners[1]).x) * 0.5F;
  // A à droite, B à gauche, à la demi-largeur projetée du premier triangle.
  CHECK(Screen(matrice, (*sommets)[0].position).x == Approx(anchor + half));
  CHECK(Screen(matrice, (*sommets)[1].position).x == Approx(anchor - half));
  // La hauteur projetée du coin, levée d'un seizième de l'écran.
  const float levee = ro::render::kEffectQuadRecentredRiseScreenShare * 2.0F;
  CHECK(Screen(matrice, (*sommets)[2].position).y ==
        Approx(Screen(matrice, quad.corners[2]).y + levee));
}

TEST_CASE("un quad recentré derrière l'œil ne se pose pas", "[render][quad_depth]") {
  EffectWorldQuad quad = Square(EffectQuadDepth::kScreenRecentred);
  quad.screen_anchor = kEye + glm::vec3{0.0F, 0.0F, 50.0F};
  CHECK_FALSE(PlaceEffectQuadVertices(quad, ViewAndProjection(), kEye).has_value());
}

TEST_CASE("la demande porte sa variante de profondeur jusqu'au quad", "[render][quad_depth]") {
  ro::render::EffectParticleQuadRequest request;
  request.depth = EffectQuadDepth::kScreenRecentred;
  const glm::vec3 origin{3.0F, 4.0F, 5.0F};
  const EffectWorldQuad quad = ro::render::ComposeEffectParticleQuad(request, origin);
  CHECK(quad.depth == EffectQuadDepth::kScreenRecentred);
  CHECK(quad.screen_anchor == origin);
}

TEST_CASE("le bleu et le rouge sombre du carre qui respire passent devant tout",
          "[render][quad_depth]") {
  CHECK(ro::render::EffectBreathingQuadStyleFor(113, 0.0F).depth == EffectQuadDepth::kFrontmost);
  CHECK(ro::render::EffectBreathingQuadStyleFor(114, 0.0F).depth == EffectQuadDepth::kFrontmost);
  CHECK(ro::render::EffectBreathingQuadStyleFor(110, 0.0F).depth == EffectQuadDepth::kProjected);
  CHECK(ro::render::EffectBreathingQuadStyleFor(0, 0.0F).depth == EffectQuadDepth::kProjected);
}

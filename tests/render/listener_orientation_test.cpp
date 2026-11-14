// L'OREILLE DROITE DE L'AUDITEUR, comme le client la tourne : du lacet du jeu,
// TRONQUÉ au degré et lu dans ses tables.

#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"

using Catch::Approx;

namespace {

constexpr float kClientPi = 3.141592F;

float Radians(float degrees) { return degrees / 180.0F * kClientPi; }

glm::vec3 RightAtYaw(float yaw_degrees) {
  ro::render::Camera camera;
  camera.yaw_degrees = yaw_degrees;
  return ro::render::ClientListenerRight(camera);
}

}  // namespace

TEST_CASE("l'auditeur a l'oreille droite vers notre abscisse à lacet nul", "[render][sound]") {
  const glm::vec3 right = RightAtYaw(0.0F);
  CHECK(right.x == Approx(1.0F));
  CHECK(right.y == Approx(0.0F));
  CHECK(right.z == Approx(0.0F).margin(1e-5));
  // Et c'est la droite de notre caméra, au degré près.
  ro::render::Camera camera;
  camera.yaw_degrees = 40.0F;
  const glm::vec3 devant = camera.look_at - camera.Position();
  const glm::vec3 attendue = glm::normalize(glm::cross(devant, glm::vec3{0.0F, 1.0F, 0.0F}));
  CHECK(glm::dot(ro::render::ClientListenerRight(camera), attendue) == Approx(1.0F).margin(1e-4));
}

TEST_CASE("l'auditeur tourne au degré entier, comme les tables du client", "[render][sound]") {
  // Notre lacet 30,7 : lacet du jeu −30,7, angle 180 + 30,7 = 210,7 tronqué à
  // 210. La droite est (−cos 210, 0, sin 210).
  const glm::vec3 right = RightAtYaw(30.7F);
  CHECK(right.x == Approx(-std::cos(Radians(210.0F))));
  CHECK(right.z == Approx(std::sin(Radians(210.0F))));
  // ⚠ UN ANGLE NÉGATIF SE LIT UN DEGRÉ DE TRAVERS : notre lacet −200 donne
  // l'angle −20, lu à l'entrée 341 et non 340.
  const glm::vec3 negatif = RightAtYaw(-200.0F);
  CHECK(negatif.x == Approx(-std::cos(Radians(341.0F))));
  CHECK(negatif.z == Approx(std::sin(Radians(341.0F))));
}

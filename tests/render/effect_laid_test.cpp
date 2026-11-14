#include "ro/render/effect_laid.h"

#include <array>
#include <span>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/effect_ground_ribbon_seed.h"

using Catch::Approx;

TEST_CASE("Effet debout : grandi, retourné, tourné autour du point du sol",
          "[render][effectlaid]") {
  // Un coin à dix pixels à droite de l'ancre, ancre relevée de vingt pixels.
  std::array<glm::vec2, 4> corners = {glm::vec2{10.0F, 0.0F}, glm::vec2{10.0F, 0.0F},
                                      glm::vec2{10.0F, 0.0F}, glm::vec2{10.0F, 0.0F}};
  // Sans rien, rien ne bouge.
  std::array<glm::vec2, 4> tels_quels = corners;
  ro::render::ShapeStandingEffectCorners(tels_quels, 20.0F, 0.0F, 1.0F, false, false);
  CHECK(tels_quels[0].x == Approx(10.0F));
  CHECK(tels_quels[0].y == Approx(0.0F));
  // L'échelle se prend depuis le SOL : l'ancre, vingt pixels plus haut, se
  // rapproche du sol en même temps.
  std::array<glm::vec2, 4> moitie = corners;
  ro::render::ShapeStandingEffectCorners(moitie, 20.0F, 0.0F, 0.5F, false, false);
  CHECK(moitie[0].x == Approx(5.0F));
  CHECK(moitie[0].y == Approx(10.0F));
  // Le retournement horizontal change le coin de côté.
  std::array<glm::vec2, 4> retourne = corners;
  ro::render::ShapeStandingEffectCorners(retourne, 0.0F, 0.0F, 1.0F, true, false);
  CHECK(retourne[0].x == Approx(-10.0F));
  // Un quart de tour, dans le sens des aiguilles d'une montre à l'écran : la
  // droite passe en bas.
  std::array<glm::vec2, 4> rotated = corners;
  ro::render::ShapeStandingEffectCorners(rotated, 0.0F, 90.0F, 1.0F, false, false);
  CHECK(rotated[0].x == Approx(0.0F).margin(0.001F));
  CHECK(rotated[0].y == Approx(10.0F).margin(0.001F));
  // ⚠ Une rotation NÉGATIVE ne s'applique pas.
  std::array<glm::vec2, 4> negatif = corners;
  ro::render::ShapeStandingEffectCorners(negatif, 0.0F, -90.0F, 1.0F, false, false);
  CHECK(negatif[0].x == Approx(10.0F));
}

TEST_CASE("Effet couché : les pixels deviennent le sol, deux unités au-dessus",
          "[render][effectlaid]") {
  ro::render::EffectQuad quad;
  quad.corners = {glm::vec2{10.0F, 0.0F}, glm::vec2{0.0F, 10.0F}, glm::vec2{-10.0F, 0.0F},
                  glm::vec2{0.0F, -10.0F}};
  ro::render::LaidEffectPose pose;
  pose.center = glm::vec3(100.0F, 5.0F, 200.0F);
  const auto relief = [](float, float) { return 7.0F; };
  std::vector<ro::render::EffectWorldQuad> layers;
  ro::render::LayEffectQuads(std::span(&quad, 1), pose, relief, layers);
  REQUIRE(layers.size() == 1);
  // Caméra de cap nul : la droite de l'image va vers +x, son bas vers +z.
  CHECK(layers[0].corners[0].x == Approx(110.0F));
  CHECK(layers[0].corners[0].z == Approx(200.0F).margin(0.001F));
  CHECK(layers[0].corners[1].z == Approx(210.0F));
  // Chaque coin épouse le relief.
  CHECK(layers[0].corners[0].y == Approx(7.0F + ro::render::kLaidEffectLiftUnits));
  // Grandi au quinze centième.
  pose.scale = 0.15F;
  layers.clear();
  ro::render::LayEffectQuads(std::span(&quad, 1), pose, relief, layers);
  CHECK(layers[0].corners[0].x == Approx(101.5F));
}

TEST_CASE("Ruban au sol : le cap vers la cible, profondeur retournée", "[render][effectlaid]") {
  const glm::vec3 start(0.0F, 0.0F, 0.0F);
  CHECK(ro::render::EffectGroundRibbonTargetDegrees(start, {10.0F, 0.0F, 0.0F}) == 0);
  // Notre profondeur négative est la positive du client : un quart de tour.
  CHECK(ro::render::EffectGroundRibbonTargetDegrees(start, {0.0F, 0.0F, -10.0F}) == 90);
  CHECK(ro::render::EffectGroundRibbonTargetDegrees(start, {0.0F, 0.0F, 10.0F}) == 270);
  CHECK(ro::render::EffectGroundRibbonTargetDegrees(start, start) == 0);
}

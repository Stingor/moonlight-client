// LA CAMÉRA DU PANTIN : ce qu'elle voit tombe sur le personnage d'un aperçu,
// les pieds sur ses pieds, à l'échelle où son sprite a été grandi.

#include "effects/doll_camera.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/sprite_billboard.h"
#include "ro/render/sprite_vignette.h"

namespace {

using Catch::Approx;

// La boîte de l'aperçu, et un cadrage où les pieds ne sont pas au centre.
constexpr float kBoxWidth = 260.0F;
constexpr float kBoxHeight = 240.0F;
constexpr float kFramingScale = 1.6F;
constexpr glm::vec2 kFeet(118.0F, 226.0F);
// Une case quelconque d'une carte, loin de l'origine du monde.
constexpr glm::vec3 kGround(812.5F, -14.0F, -377.5F);

// Où un point du monde tombe dans la boîte, en pixels depuis le coin haut-gauche.
glm::vec2 InBox(const ro::render::Camera& camera, glm::vec3 point) {
  const glm::vec4 clip = camera.ProjectionMatrix(kBoxWidth / kBoxHeight) * camera.ViewMatrix() *
                         glm::vec4(point, 1.0F);
  const glm::vec2 ndc(clip.x / clip.w, clip.y / clip.w);
  return {(ndc.x + 1.0F) * 0.5F * kBoxWidth, (1.0F - ndc.y) * 0.5F * kBoxHeight};
}

ro::render::VignetteFraming Framing() {
  ro::render::VignetteFraming framing;
  framing.scale = kFramingScale;
  framing.origin = kFeet;
  return framing;
}

}  // namespace

TEST_CASE("Pantin : la caméra pose le sol du porteur sur les pieds du pantin", "[doll_camera]") {
  const ro::render::Camera camera =
      moonclient::DollCamera(kGround, Framing(), kBoxWidth, kBoxHeight);
  const glm::vec2 feet = InBox(camera, kGround);
  CHECK(feet.x == Approx(kFeet.x).margin(0.01));
  CHECK(feet.y == Approx(kFeet.y).margin(0.01));
  // Sous l'angle du jeu.
  CHECK(camera.pitch_degrees == ro::render::kGameElevationDegrees);
  CHECK(camera.fov_degrees == ro::render::kGameFovDegrees);
}

TEST_CASE("Pantin : une unité du monde vaut sept pixels de sprite, grandis comme le pantin",
          "[doll_camera]") {
  const ro::render::Camera camera =
      moonclient::DollCamera(kGround, Framing(), kBoxWidth, kBoxHeight);
  const ro::render::BillboardBasis basis = ro::render::MakeBillboardBasis(camera);
  constexpr float kUnits = 10.0F;
  const float expected = kUnits * ro::render::kSpritePixelsPerWorldUnit * kFramingScale;
  // Vers le haut de l'écran : l'ordonnée de la boîte descend.
  const glm::vec2 raised = InBox(camera, kGround + basis.up * kUnits);
  CHECK(kFeet.y - raised.y == Approx(expected).epsilon(0.001));
  const glm::vec2 aside = InBox(camera, kGround + basis.right * kUnits);
  CHECK(aside.x - kFeet.x == Approx(expected).epsilon(0.001));
}

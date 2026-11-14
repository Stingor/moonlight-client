// UN NOMBRE ÉCRIT EN POLICE SE POSE LÀ OÙ SE POSERAIENT SES CHIFFRES : même
// milieu, même taille à la même profondeur.

#include "world/damage_text_view.h"

#include <cmath>
#include <optional>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/actor_pick.h"
#include "ro/render/camera.h"
#include "ro/render/floating_number.h"
#include "ro/render/sprite_billboard.h"

using moonclient::DamageTextOnScreen;
using moonclient::NumberScreen;

namespace {

constexpr float kViewportWidth = 1024.0F;
constexpr float kViewportHeight = 768.0F;
constexpr float kAspectRatio = kViewportWidth / kViewportHeight;
constexpr glm::vec3 kGround{1500.0F, 0.0F, -900.0F};
constexpr float kCameraDistance = 600.0F;
constexpr float kCameraYawDegrees = 37.0F;
// Un chiffre du client : onze pixels de haut, son milieu un demi-pixel
// au-dessus du point d'appui.
constexpr ro::render::GlyphBox kDigit{.height_pixels = 11.0F, .middle_y_pixels = -0.5F};

[[nodiscard]] ro::render::Camera GameCamera() {
  ro::render::Camera camera;
  camera.look_at = kGround;
  camera.distance = kCameraDistance;
  camera.pitch_degrees = ro::render::kGameElevationDegrees;
  camera.yaw_degrees = kCameraYawDegrees;
  return camera;
}

[[nodiscard]] ro::render::NumberText Text(float scale) {
  ro::render::NumberText text;
  text.landing_pixels = glm::vec2{4.0F, -30.0F};
  text.scale = scale;
  text.tint = {255, 128, 0, 200};
  text.glyphs = "1234";
  return text;
}

}  // namespace

TEST_CASE("Nombre en police : il tombe au milieu de ses chiffres, à leur taille", "[app][damage]") {
  const ro::render::Camera camera = GameCamera();
  const NumberScreen screen{&camera, kAspectRatio, kViewportWidth, kViewportHeight};
  const std::optional<ro::ui::DamageTextView> view =
      DamageTextOnScreen(screen, kGround, Text(2.0F), kDigit, 1.0F);
  REQUIRE(view.has_value());

  // Le milieu : le point d'appui, plus le milieu d'un chiffre à l'échelle,
  // posé sur le panneau face à la caméra puis projeté.
  const glm::vec3 middle = ro::render::BillboardPoint(
      glm::vec2{4.0F, -30.0F - 0.5F * 2.0F}, kGround, ro::render::MakeBillboardBasis(camera));
  const std::optional<glm::vec2> expected =
      ro::render::ProjectToScreen(camera, kAspectRatio, middle, kViewportWidth, kViewportHeight);
  REQUIRE(expected.has_value());
  CHECK(view->middle.x == Catch::Approx(expected->x));
  CHECK(view->middle.y == Catch::Approx(expected->y));

  // La taille : onze pixels de sprite à l'échelle deux, à la profondeur du point.
  const float screen_per_pixel =
      1.0F / ro::render::SpritePixelsPerScreenPixel(camera, middle, kViewportHeight);
  CHECK(view->digit_height == Catch::Approx(11.0F * 2.0F * screen_per_pixel));
  CHECK(view->pitch == Catch::Approx(ro::render::kDigitPitchPixels * 2.0F * screen_per_pixel));
  CHECK(view->outline == Catch::Approx(2.0F * screen_per_pixel));
  CHECK(view->color == IM_COL32(255, 128, 0, 200));
  CHECK(std::string_view(view->glyphs.data()) == "1234");
}

TEST_CASE("Nombre en police : tout suit son échelle, et rien sans caméra", "[app][damage]") {
  const ro::render::Camera camera = GameCamera();
  const NumberScreen screen{&camera, kAspectRatio, kViewportWidth, kViewportHeight};
  const std::optional<ro::ui::DamageTextView> small =
      DamageTextOnScreen(screen, kGround, Text(1.0F), kDigit, 0.0F);
  const std::optional<ro::ui::DamageTextView> large =
      DamageTextOnScreen(screen, kGround, Text(3.0F), kDigit, 0.0F);
  REQUIRE(small.has_value());
  REQUIRE(large.has_value());
  CHECK(large->digit_height == Catch::Approx(small->digit_height * 3.0F).epsilon(1e-3));
  CHECK(small->outline == 0.0F);

  CHECK_FALSE(DamageTextOnScreen(NumberScreen{}, kGround, Text(1.0F), kDigit, 1.0F).has_value());
}

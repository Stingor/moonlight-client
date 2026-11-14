// LA SECOUSSE D'ÉCRAN ET SA BASCULE : tremblements coupés, la caméra ne
// reçoit rien, quoi qu'on ait demandé.

#include <cmath>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/sim/world.h"

#include "input/camera_aim.h"

using moonclient::ScreenShake;

namespace {

constexpr std::uint32_t kShakeUntilMs = 1000;
constexpr std::uint32_t kDuringShakeMs = 10;

[[nodiscard]] bool SamePose(const ro::render::Camera& left, const ro::render::Camera& right) {
  return left.look_at == right.look_at && left.distance == right.distance &&
         left.pitch_degrees == right.pitch_degrees && left.yaw_degrees == right.yaw_degrees;
}

}  // namespace

TEST_CASE("Secousse d'écran : permise, une demande remue la caméra", "[app][camera][quake]") {
  const ro::sim::World world;
  ScreenShake shake;
  ro::render::Camera camera;
  const ro::render::Camera before = camera;
  shake.Request(kShakeUntilMs);
  shake.Apply(camera, world, kDuringShakeMs);
  CHECK(shake.shaking());
  CHECK_FALSE(SamePose(camera, before));
}

namespace {

constexpr std::uint32_t kLongShakeUntilMs = 2000;
constexpr std::uint32_t kOneSecondMs = 1000;
constexpr int kFlipsPerSecond = 60;
constexpr int kPlainRefreshHz = 60;
constexpr int kFastRefreshHz = 144;
constexpr int kFastestRefreshHz = 240;
// L'écart que laissent les retraits et reposes successifs, en flottants.
constexpr float kRoundingSlack = 1e-4F;

[[nodiscard]] bool Near(float left, float right) { return std::abs(left - right) < kRoundingSlack; }

[[nodiscard]] bool NearPose(const ro::render::Camera& left, const ro::render::Camera& right) {
  return Near(left.look_at.x, right.look_at.x) && Near(left.look_at.y, right.look_at.y) &&
         Near(left.look_at.z, right.look_at.z) && Near(left.distance, right.distance) &&
         Near(left.pitch_degrees, right.pitch_degrees);
}

// La pose de la caméra à `at_ms`, après une secousse jouée image après image
// à `refresh_hz` jusque-là : chaque image retire puis repose, comme le jeu.
ro::render::Camera ShakenAt(int refresh_hz, std::uint32_t at_ms) {
  const ro::sim::World world;
  ScreenShake shake;
  shake.Request(kLongShakeUntilMs);
  ro::render::Camera camera;
  for (int frame = 0;; ++frame) {
    const auto frame_ms =
        static_cast<std::uint32_t>(frame) * kOneSecondMs / static_cast<std::uint32_t>(refresh_hz);
    if (frame_ms >= at_ms) {
      break;
    }
    shake.Undo(camera);
    camera.look_at -= shake.look_at_offset();
    shake.Apply(camera, world, frame_ms);
  }
  shake.Undo(camera);
  camera.look_at -= shake.look_at_offset();
  shake.Apply(camera, world, at_ms);
  return camera;
}

// Combien de fois le recul change de signe en une seconde, vu à `refresh_hz`.
int FlipsInOneSecond(int refresh_hz) {
  const ro::sim::World world;
  ScreenShake shake;
  shake.Request(kLongShakeUntilMs);
  ro::render::Camera camera;
  const float rest_distance = camera.distance;
  int flips = 0;
  bool was_back = false;
  for (int frame = 0; frame < refresh_hz; ++frame) {
    const auto frame_ms =
        static_cast<std::uint32_t>(frame) * kOneSecondMs / static_cast<std::uint32_t>(refresh_hz);
    shake.Undo(camera);
    camera.look_at -= shake.look_at_offset();
    shake.Apply(camera, world, frame_ms);
    const bool back = camera.distance > rest_distance;
    if (frame > 0 && back != was_back) {
      ++flips;
    }
    was_back = back;
  }
  return flips;
}

}  // namespace

TEST_CASE("Secousse d'écran : la même à 60, 144 et 240 images par seconde",
          "[app][camera][quake]") {
  // Aux mêmes instants, la même pose : le tremblement ne dépend que du temps.
  for (const std::uint32_t at_ms : {100U, 333U, 500U, 649U}) {
    const ro::render::Camera plain = ShakenAt(kPlainRefreshHz, at_ms);
    CHECK_FALSE(SamePose(plain, ro::render::Camera()));
    CHECK(NearPose(ShakenAt(kFastRefreshHz, at_ms), plain));
    CHECK(NearPose(ShakenAt(kFastestRefreshHz, at_ms), plain));
  }
  // Soixante inversions par seconde, à une près — la dernière tombe au bord —
  // et non une par image.
  CHECK(FlipsInOneSecond(kFastRefreshHz) >= kFlipsPerSecond - 1);
  CHECK(FlipsInOneSecond(kFastRefreshHz) <= kFlipsPerSecond);
  CHECK(FlipsInOneSecond(kFastestRefreshHz) == FlipsInOneSecond(kFastRefreshHz));
}

TEST_CASE("Secousse d'écran : coupée, aucun décalage n'est posé", "[app][camera][quake]") {
  const ro::sim::World world;
  ScreenShake shake;
  shake.enabled = false;
  ro::render::Camera camera;
  const ro::render::Camera before = camera;
  shake.Request(kShakeUntilMs);
  shake.Apply(camera, world, kDuringShakeMs);
  CHECK_FALSE(shake.shaking());
  CHECK(SamePose(camera, before));
  CHECK(shake.look_at_offset() == glm::vec3(0.0F, 0.0F, 0.0F));

  // ⚠ LA DEMANDE S'OUBLIE : rallumés, les tremblements ne reprennent pas une
  // secousse demandée pendant qu'ils étaient coupés.
  shake.enabled = true;
  shake.Apply(camera, world, kDuringShakeMs + 1);
  CHECK(SamePose(camera, before));
}

#include "ro/render/spin_blur.h"

#include <cmath>
#include <cstdint>
#include <numbers>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"

using Catch::Approx;
using ro::render::Camera;
using ro::render::CameraSpinMeter;
using ro::render::CameraTurnRate;
using ro::render::ShortestTurnDegrees;
using ro::render::SpinBlurFrame;
using ro::render::SpinBlurFrameOf;
using ro::render::SpinBlurSettings;
using ro::render::SpinSweep;
using ro::render::SpinSweepOf;
namespace spin_blur = ro::render::spin_blur;

namespace {

constexpr SpinBlurSettings kOn{.enabled = true, .strength = 1.0F};
constexpr float kAspectRatio = 16.0F / 9.0F;

float SweepDegrees(const SpinSweep& sweep) {
  return std::hypot(sweep.yaw_degrees, sweep.pitch_degrees);
}

CameraTurnRate Yaw(float degrees_per_second) {
  return CameraTurnRate{.yaw_degrees_per_second = degrees_per_second};
}

// La vitesse lissée après `duration_ms` d'une rotation tenue à
// `degrees_per_second`, mesurée toutes les `frame_ms`.
CameraTurnRate SteadyTurn(float degrees_per_second, std::uint32_t frame_ms,
                          std::uint32_t duration_ms) {
  CameraSpinMeter meter;
  CameraTurnRate rate = meter.Measure(0.0F, 0.0F, true, 0);
  for (std::uint32_t now_ms = frame_ms; now_ms <= duration_ms; now_ms += frame_ms) {
    const float yaw = degrees_per_second * static_cast<float>(now_ms) / 1000.0F;
    rate = meter.Measure(std::fmod(yaw, 360.0F), 0.0F, true, now_ms);
  }
  return rate;
}

}  // namespace

TEST_CASE("Flou de rotation : éteint ou immobile, rien", "[render][spin_blur]") {
  CHECK(SweepDegrees(SpinSweepOf(SpinBlurSettings{}, Yaw(500.0F))) == 0.0F);
  CHECK(SweepDegrees(SpinSweepOf(kOn, Yaw(0.0F))) == 0.0F);
  CHECK(SweepDegrees(
            SpinSweepOf(SpinBlurSettings{.enabled = true, .strength = 0.0F}, Yaw(500.0F))) == 0.0F);
  const SpinBlurFrame idle = SpinBlurFrameOf(kOn, Yaw(0.0F), Camera{}, kAspectRatio);
  CHECK_FALSE(idle.active());
  CHECK_FALSE(SpinBlurFrameOf(SpinBlurSettings{}, Yaw(500.0F), Camera{}, kAspectRatio).active());
}

TEST_CASE("Flou de rotation : rien sous le seuil", "[render][spin_blur]") {
  CHECK(SweepDegrees(SpinSweepOf(kOn, Yaw(spin_blur::kFloorDegreesPerSecond))) == 0.0F);
  CHECK(SweepDegrees(SpinSweepOf(kOn, Yaw(-spin_blur::kFloorDegreesPerSecond * 0.5F))) == 0.0F);
  CHECK(SweepDegrees(SpinSweepOf(kOn, Yaw(spin_blur::kFloorDegreesPerSecond + 1.0F))) > 0.0F);
}

TEST_CASE("Flou de rotation : proportionnel à ce qui dépasse le seuil", "[render][spin_blur]") {
  const float floor = spin_blur::kFloorDegreesPerSecond;
  const float one = SweepDegrees(SpinSweepOf(kOn, Yaw(floor + 20.0F)));
  const float two = SweepDegrees(SpinSweepOf(kOn, Yaw(floor + 40.0F)));
  CHECK(two == Approx(2.0F * one));
  CHECK(one == Approx(20.0F * spin_blur::kExposureSeconds));
  // L'intensité règle la pose : la moitié, la moitié du balayage.
  const float half = SweepDegrees(
      SpinSweepOf(SpinBlurSettings{.enabled = true, .strength = 0.5F}, Yaw(floor + 40.0F)));
  CHECK(half == Approx(one));
  // Le sens ne change que le signe.
  CHECK(SpinSweepOf(kOn, Yaw(-(floor + 20.0F))).yaw_degrees == Approx(-one));
}

TEST_CASE("Flou de rotation : borné", "[render][spin_blur]") {
  CHECK(SweepDegrees(SpinSweepOf(kOn, Yaw(100000.0F))) == Approx(spin_blur::kMostSweepDegrees));
  const SpinSweep both = SpinSweepOf(
      kOn, CameraTurnRate{.yaw_degrees_per_second = 3000.0F, .pitch_degrees_per_second = 4000.0F});
  CHECK(SweepDegrees(both) == Approx(spin_blur::kMostSweepDegrees));
  // La borne garde la direction de la rotation.
  CHECK(both.pitch_degrees / both.yaw_degrees == Approx(4.0F / 3.0F));
}

TEST_CASE("Flou de rotation : l'écart le plus court passe par zéro", "[render][spin_blur]") {
  CHECK(ShortestTurnDegrees(359.0F, 1.0F) == Approx(2.0F));
  CHECK(ShortestTurnDegrees(1.0F, 359.0F) == Approx(-2.0F));
  CHECK(ShortestTurnDegrees(-179.0F, 179.0F) == Approx(-2.0F));
  CHECK(ShortestTurnDegrees(10.0F, 30.0F) == Approx(20.0F));
  CHECK(ShortestTurnDegrees(0.0F, 720.0F) == Approx(0.0F).margin(1e-4));

  // UNE ROTATION QUI FRANCHIT 0° se mesure à sa vraie vitesse.
  CameraSpinMeter meter;
  static_cast<void>(meter.Measure(359.0F, 50.0F, true, 1000));
  const CameraTurnRate rate = meter.Measure(1.0F, 50.0F, true, 1010);
  CHECK(rate.yaw_degrees_per_second > 0.0F);
  CHECK(rate.yaw_degrees_per_second < 200.0F);
}

TEST_CASE("Flou de rotation : la mesure ne dépend pas de la cadence", "[render][spin_blur]") {
  constexpr float kTurn = 180.0F;
  constexpr std::uint32_t kHeldMs = 480;
  const CameraTurnRate at_60 = SteadyTurn(kTurn, 16, kHeldMs);
  const CameraTurnRate at_240 = SteadyTurn(kTurn, 4, kHeldMs);
  const CameraTurnRate at_30 = SteadyTurn(kTurn, 32, kHeldMs);
  CHECK(at_60.yaw_degrees_per_second == Approx(kTurn).epsilon(0.01));
  CHECK(at_240.yaw_degrees_per_second == Approx(kTurn).epsilon(0.01));
  CHECK(at_30.yaw_degrees_per_second == Approx(kTurn).epsilon(0.01));

  // Le même temps écoulé lisse autant, en une image ou en plusieurs.
  CameraSpinMeter coarse;
  CameraSpinMeter fine;
  static_cast<void>(coarse.Measure(0.0F, 0.0F, true, 0));
  static_cast<void>(fine.Measure(0.0F, 0.0F, true, 0));
  const CameraTurnRate one_step = coarse.Measure(4.0F, 0.0F, true, 20);
  static_cast<void>(fine.Measure(2.0F, 0.0F, true, 10));
  const CameraTurnRate two_steps = fine.Measure(4.0F, 0.0F, true, 20);
  CHECK(one_step.yaw_degrees_per_second == Approx(two_steps.yaw_degrees_per_second));
}

TEST_CASE("Flou de rotation : un cran n'est pas un mouvement", "[render][spin_blur]") {
  CameraSpinMeter meter;
  static_cast<void>(meter.Measure(0.0F, 50.0F, false, 0));
  // Une remise de la caméra : quatre-vingt-dix degrés d'un coup, sans glisser.
  CameraTurnRate rate = meter.Measure(90.0F, 45.0F, false, 16);
  CHECK(rate.yaw_degrees_per_second == 0.0F);
  CHECK(rate.pitch_degrees_per_second == 0.0F);
  // Puis le glisser reprend depuis là, sans compter le saut.
  rate = meter.Measure(92.0F, 45.0F, true, 32);
  CHECK(rate.yaw_degrees_per_second > 0.0F);
  CHECK(rate.yaw_degrees_per_second < 2.0F / 0.016F);
}

TEST_CASE("Flou de rotation : l'arc tourne autour du point visé", "[render][spin_blur]") {
  Camera camera;
  camera.look_at = {10.0F, 0.0F, -20.0F};
  camera.distance = ro::render::kGameViewDistance;
  camera.pitch_degrees = ro::render::kGameElevationDegrees;
  camera.fov_degrees = ro::render::kGameFovDegrees;
  const SpinBlurFrame frame = SpinBlurFrameOf(kOn, Yaw(200.0F), camera, kAspectRatio);
  REQUIRE(frame.active());
  // La caméra regarde son point visé : il tombe au centre de l'écran.
  CHECK(frame.pivot_uv[0] == Approx(0.5F).margin(1e-3));
  CHECK(frame.pivot_uv[1] == Approx(0.5F).margin(1e-3));
  CHECK(frame.squash ==
        Approx(std::sin(ro::render::kGameElevationDegrees * std::numbers::pi_v<float> / 180.0F)));
  CHECK(frame.stretch == 0.0F);
  CHECK(frame.arc_radians > 0.0F);
}

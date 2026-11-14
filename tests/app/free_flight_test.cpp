// LE VOL LIBRE : l'œil part d'où était la caméra, suit le regard, et rend la
// pose en sortant.

#include "input/free_flight.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"

#include "input/first_person.h"

using Catch::Approx;
using moonclient::FlightKey;
using moonclient::FlightKeys;
using moonclient::FreeFlight;
using moonclient::FreeFlightStep;

namespace {

constexpr float kNorthYaw = 0.0F;
constexpr float kLevelPitch = 0.0F;
constexpr std::uint32_t kOneSecondMs = 1000;

[[nodiscard]] FlightKeys Holding(std::initializer_list<FlightKey> keys) {
  FlightKeys held{};
  for (const FlightKey key : keys) {
    held[static_cast<std::size_t>(key)] = true;
  }
  return held;
}

}  // namespace

TEST_CASE("Vol libre : avancer suit le regard, à la vitesse dite", "[app][free_flight]") {
  const glm::vec3 step = FreeFlightStep(kNorthYaw, kLevelPitch, Holding({FlightKey::kForward}),
                                        /*fast=*/false, kOneSecondMs);
  // Le regard d'une caméra au lacet nul tombe vers les z décroissants.
  CHECK(step.x == Approx(0.0F).margin(1e-4));
  CHECK(step.y == Approx(0.0F).margin(1e-4));
  CHECK(step.z == Approx(-moonclient::kFreeFlightUnitsPerSecond));
}

TEST_CASE("Vol libre : regarder le sol et avancer fait descendre", "[app][free_flight]") {
  constexpr float kLookingDown = 45.0F;
  const glm::vec3 step =
      FreeFlightStep(kNorthYaw, kLookingDown, Holding({FlightKey::kForward}), false, kOneSecondMs);
  CHECK(step.y < 0.0F);
  CHECK(glm::length(step) == Approx(moonclient::kFreeFlightUnitsPerSecond));
}

TEST_CASE("Vol libre : monter ignore le regard, Maj accélère", "[app][free_flight]") {
  constexpr float kLookingDown = 60.0F;
  const glm::vec3 step = FreeFlightStep(kNorthYaw, kLookingDown, Holding({FlightKey::kUp}),
                                        /*fast=*/true, kOneSecondMs);
  CHECK(step.x == Approx(0.0F).margin(1e-4));
  CHECK(step.z == Approx(0.0F).margin(1e-4));
  CHECK(step.y ==
        Approx(moonclient::kFreeFlightUnitsPerSecond * moonclient::kFreeFlightFastFactor));
}

TEST_CASE("Vol libre : la droite de l'écran est la droite du pas", "[app][free_flight]") {
  const glm::vec3 eye(0.0F);
  ro::render::Camera camera;
  moonclient::AimFirstPersonCamera(camera, eye, kNorthYaw, kLevelPitch);
  const glm::vec3 step =
      FreeFlightStep(kNorthYaw, kLevelPitch, Holding({FlightKey::kRight}), false, kOneSecondMs);
  // La droite de l'écran est la première ligne de la matrice de vue.
  const glm::mat4 view = camera.ViewMatrix();
  const glm::vec3 screen_right(view[0][0], view[1][0], view[2][0]);
  CHECK(glm::dot(glm::normalize(step), screen_right) == Approx(1.0F));
}

TEST_CASE("Vol libre : l'envol part de l'œil, la sortie rend la pose", "[app][free_flight]") {
  ro::render::Camera camera;
  camera.look_at = glm::vec3(100.0F, 0.0F, -200.0F);
  camera.distance = 150.0F;
  camera.yaw_degrees = 30.0F;
  camera.pitch_degrees = 50.0F;
  camera.fov_degrees = 15.0F;
  const ro::render::Camera before = camera;

  FreeFlight flight;
  flight.Enter(camera);
  CHECK(flight.active());
  CHECK(glm::length(flight.eye() - before.Position()) == Approx(0.0F).margin(1e-3));

  flight.Press(FlightKey::kUp);
  flight.Advance(0, false);
  flight.Advance(kOneSecondMs, false);
  CHECK(flight.eye().y ==
        Approx(before.Position().y + moonclient::kFreeFlightUnitsPerSecond).epsilon(1e-4));
  flight.Aim(camera);
  CHECK(glm::length(camera.Position() - flight.eye()) == Approx(0.0F).margin(1e-2));

  flight.Leave(camera);
  CHECK_FALSE(flight.active());
  CHECK(camera.look_at == before.look_at);
  CHECK(camera.distance == before.distance);
  CHECK(camera.yaw_degrees == before.yaw_degrees);
  CHECK(camera.pitch_degrees == before.pitch_degrees);
  CHECK(camera.fov_degrees == before.fov_degrees);
}

TEST_CASE("Vol libre : le corps suit l'œil qui s'éloigne, sans quitter la carte",
          "[app][free_flight]") {
  constexpr std::int32_t kMapCells = 100;
  constexpr std::int32_t kBody = 50;
  constexpr std::int32_t kNear = kBody + moonclient::kFreeFlightLeashCells - 1;
  constexpr std::int32_t kFar = kBody + moonclient::kFreeFlightLeashCells;
  // ASSEZ PRÈS : rien ne part.
  CHECK_FALSE(moonclient::FreeFlightLeashCell(kNear, kNear, kBody, kBody, kMapCells, kMapCells)
                  .has_value());
  // ASSEZ LOIN SUR UN SEUL AXE : la case sous l'œil.
  const auto far = moonclient::FreeFlightLeashCell(kFar, kBody, kBody, kBody, kMapCells, kMapCells);
  REQUIRE(far.has_value());
  CHECK(far->x == kFar);
  CHECK(far->y == kBody);
  // HORS DE LA CARTE : ramenée au bord.
  const auto outside =
      moonclient::FreeFlightLeashCell(-20, kMapCells + 20, kBody, kBody, kMapCells, kMapCells);
  REQUIRE(outside.has_value());
  CHECK(outside->x == 0);
  CHECK(outside->y == kMapCells - 1);
}

TEST_CASE("Vol libre : le corps ne se redemande qu'à sa cadence", "[app][free_flight]") {
  FreeFlight flight;
  CHECK(flight.LeashDue(0));
  CHECK_FALSE(flight.LeashDue(moonclient::kFreeFlightLeashRepeatMs - 1));
  CHECK(flight.LeashDue(moonclient::kFreeFlightLeashRepeatMs));
}

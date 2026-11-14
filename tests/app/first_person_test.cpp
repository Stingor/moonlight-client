// LA VUE À LA PREMIÈRE PERSONNE : l'œil où il doit être, et la case demandée
// devant le regard.

#include "input/first_person.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/sim/walk_map.h"

using Catch::Approx;
using moonclient::FirstPersonStride;
using moonclient::FirstPersonView;
using moonclient::StrideKey;
using moonclient::StrideKeys;

namespace {

constexpr float kNorthYaw = 0.0F;
constexpr float kQuarterTurn = 90.0F;

[[nodiscard]] StrideKeys Holding(std::initializer_list<StrideKey> keys) {
  StrideKeys held{};
  for (const StrideKey key : keys) {
    held[static_cast<std::size_t>(key)] = true;
  }
  return held;
}

// Neuf cases sur neuf, toutes franchissables : le personnage est au centre.
constexpr std::array<std::string_view, 9> kOpenField = {".........", ".........", ".........",
                                                        ".........", ".........", ".........",
                                                        ".........", ".........", "........."};

}  // namespace

TEST_CASE("Première personne : la caméra a son œil exactement à l'œil demandé",
          "[app][first_person]") {
  ro::render::Camera camera;
  const glm::vec3 eye(120.0F, 15.0F, 340.0F);
  moonclient::AimFirstPersonCamera(camera, eye, 37.0F, -20.0F);
  const glm::vec3 placed = camera.Position();
  CHECK(placed.x == Approx(eye.x).margin(1e-3));
  CHECK(placed.y == Approx(eye.y).margin(1e-3));
  CHECK(placed.z == Approx(eye.z).margin(1e-3));
}

TEST_CASE("Première personne : jauges et noms gardent leur taille", "[app][first_person]") {
  // 🔴 L'ÉCHELLE APPARENTE se déduit du recul et du champ : celle de la vue
  // doit valoir celle de la caméra du jeu, sans quoi tout ce qui se peint
  // par-dessus le monde enflerait en entrant dans la vue.
  ro::render::Camera camera;
  moonclient::AimFirstPersonCamera(camera, glm::vec3(0.0F), 0.0F, 0.0F);
  CHECK(ro::render::ApparentScale(camera) == Approx(1.0F));
}

TEST_CASE("Première personne : Z avance vers le nord quand on regarde au nord",
          "[app][first_person]") {
  const ro::sim::WalkMap map = ro::sim::WalkMap::FromPicture(kOpenField);
  const std::optional<ro::sim::CellPosition> ahead =
      FirstPersonStride(map, 4, 4, kNorthYaw, Holding({StrideKey::kForward}));
  REQUIRE(ahead.has_value());
  CHECK(ahead->x == 4);
  CHECK(ahead->y == 4 + moonclient::kFirstPersonStrideCells);
  // D, au nord, va à l'est.
  const std::optional<ro::sim::CellPosition> right =
      FirstPersonStride(map, 4, 4, kNorthYaw, Holding({StrideKey::kRight}));
  REQUIRE(right.has_value());
  CHECK(right->x == 4 + moonclient::kFirstPersonStrideCells);
  CHECK(right->y == 4);
}

TEST_CASE("Première personne : tourner le regard tourne la marche", "[app][first_person]") {
  // ⚠ LE LACET CROÎT VERS LA GAUCHE : un quart de tour regarde à l'ouest.
  const ro::sim::WalkMap map = ro::sim::WalkMap::FromPicture(kOpenField);
  const std::optional<ro::sim::CellPosition> ahead =
      FirstPersonStride(map, 4, 4, kQuarterTurn, Holding({StrideKey::kForward}));
  REQUIRE(ahead.has_value());
  CHECK(ahead->x == 4 - moonclient::kFirstPersonStrideCells);
  CHECK(ahead->y == 4);
}

TEST_CASE("Première personne : deux touches opposées ne demandent rien", "[app][first_person]") {
  const ro::sim::WalkMap map = ro::sim::WalkMap::FromPicture(kOpenField);
  CHECK_FALSE(
      FirstPersonStride(map, 4, 4, kNorthYaw, Holding({StrideKey::kForward, StrideKey::kBack}))
          .has_value());
  CHECK_FALSE(FirstPersonStride(map, 4, 4, kNorthYaw, StrideKeys{}).has_value());
}

TEST_CASE("Première personne : face à un mur, la marche s'en approche", "[app][first_person]") {
  // La première ligne est le nord. Le mur est à deux cases au nord du centre.
  constexpr std::array<std::string_view, 9> kWallNorth = {".........", ".........", "#########",
                                                          ".........", ".........", ".........",
                                                          ".........", ".........", "........."};
  const ro::sim::WalkMap map = ro::sim::WalkMap::FromPicture(kWallNorth);
  const std::optional<ro::sim::CellPosition> ahead =
      FirstPersonStride(map, 4, 4, kNorthYaw, Holding({StrideKey::kForward}));
  REQUIRE(ahead.has_value());
  CHECK(ahead->x == 4);
  CHECK(ahead->y == 5);
}

TEST_CASE("Première personne : en sortant, la caméra retrouve sa pose", "[app][first_person]") {
  ro::render::Camera camera;
  camera.distance = 1224.0F;
  camera.pitch_degrees = 50.0F;
  camera.yaw_degrees = 30.0F;
  camera.fov_degrees = 15.0F;
  FirstPersonView view;
  view.Toggle(camera);
  REQUIRE(view.active());
  view.Look(40.0F, 10.0F);
  view.Aim(camera, glm::vec3(10.0F, 0.0F, 10.0F));
  CHECK(camera.fov_degrees == Approx(moonclient::kFirstPersonFovDegrees));
  view.Toggle(camera);
  CHECK_FALSE(view.active());
  CHECK(camera.distance == Approx(1224.0F));
  CHECK(camera.pitch_degrees == Approx(50.0F));
  CHECK(camera.yaw_degrees == Approx(30.0F));
  CHECK(camera.fov_degrees == Approx(15.0F));
}

TEST_CASE("Première personne : la marche se redemande à sa cadence, et s'arrête une fois",
          "[app][first_person]") {
  constexpr std::uint32_t kRepeatMs = 100;
  FirstPersonView view;
  CHECK(view.StrideDue(1'000, kRepeatMs));
  CHECK_FALSE(view.StrideDue(1'100, kRepeatMs));
  CHECK(view.StrideDue(1'101, kRepeatMs));
  CHECK(view.TakeStrideEnd());
  CHECK_FALSE(view.TakeStrideEnd());
}

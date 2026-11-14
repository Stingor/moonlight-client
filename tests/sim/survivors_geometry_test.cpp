#include "ro/sim/survivors_geometry.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/packed_position.h"
#include "ro/sim/walk_field.h"
#include "ro/sim/walk_map.h"

using ro::sim::CellCenter;
using ro::sim::CellOf;
using ro::sim::Cells;
using ro::sim::CellTenths;
using ro::sim::DistanceSquared;
using ro::sim::FieldPoint;
using ro::sim::HeadingOf;
using ro::sim::IntegerSqrt;
using ro::sim::kHeadingsPerDirection;
using ro::sim::kHeadingSteps;
using ro::sim::StepsOf;
using ro::sim::Toward;
using ro::sim::TurnVector;
using ro::sim::UnitsPerStep;

namespace {

[[nodiscard]] int Sign(std::int32_t value) { return (value > 0) - (value < 0); }

}  // namespace

TEST_CASE("Géométrie survivors : racine entière", "[sim][survivors]") {
  for (std::uint64_t value = 0; value <= 65535; ++value) {
    const std::uint64_t root = IntegerSqrt(value);
    REQUIRE(root * root <= value);
    REQUIRE((root + 1) * (root + 1) > value);
  }
  CHECK(IntegerSqrt(std::uint64_t{1} << 62U) == (std::uint32_t{1} << 31U));
  CHECK(IntegerSqrt(std::numeric_limits<std::uint64_t>::max()) == 4294967295U);
  static_assert(IntegerSqrt(1048576) == 1024);
}

TEST_CASE("Géométrie survivors : cases, unités et pas", "[sim][survivors]") {
  CHECK(CellOf(-1025) == -2);
  CHECK(CellOf(-1024) == -1);
  CHECK(CellOf(-1) == -1);
  CHECK(CellOf(0) == 0);
  CHECK(CellOf(1023) == 0);
  CHECK(CellOf(1024) == 1);
  CHECK(CellCenter(2, 3) == FieldPoint{2560, 3584});
  CHECK(Cells(3) == 3072);
  CHECK(CellTenths(4) == 409);
  CHECK(CellTenths(25) == 2560);

  CHECK(UnitsPerStep(150) == 136);
  CHECK(UnitsPerStep(400) == 51);
  CHECK(UnitsPerStep(640) == 32);
  CHECK(UnitsPerStep(0) == 0);

  CHECK(StepsOf(0) == 0U);
  CHECK(StepsOf(1) == 1U);
  CHECK(StepsOf(20) == 1U);
  CHECK(StepsOf(21) == 2U);
  CHECK(StepsOf(1100) == 55U);
  CHECK(StepsOf(std::numeric_limits<std::uint32_t>::max()) == 214748365U);

  CHECK(DistanceSquared({0, 0}, {3, 4}) == 25);
  CHECK(FieldPoint{1, 2} + FieldPoint{3, 4} == FieldPoint{4, 6});
  CHECK(FieldPoint{1, 2} - FieldPoint{3, 5} == FieldPoint{-2, -3});
}

TEST_CASE("Géométrie survivors : les 64 caps", "[sim][survivors]") {
  constexpr std::int64_t kNorm = 1048576;
  constexpr std::int64_t kNormTolerance = 1200;
  for (std::uint8_t heading = 0; heading < kHeadingSteps; ++heading) {
    const FieldPoint turn = TurnVector(heading, 1024);
    const std::int64_t norm = DistanceSquared(turn, {0, 0});
    CHECK(norm >= kNorm - kNormTolerance);
    CHECK(norm <= kNorm + kNormTolerance);

    const FieldPoint opposite = TurnVector(static_cast<std::uint8_t>(heading + 32), 1024);
    CHECK(opposite == FieldPoint{-turn.x, -turn.y});
    // Le cap est pris modulo 64.
    CHECK(TurnVector(static_cast<std::uint8_t>(heading + kHeadingSteps), 1024) == turn);

    const double angle = heading * 2.0 * std::numbers::pi / kHeadingSteps;
    const auto west = static_cast<std::int32_t>(std::lround(-std::sin(angle) * 1024.0));
    const auto north = static_cast<std::int32_t>(std::lround(std::cos(angle) * 1024.0));
    CHECK(std::abs(turn.x - west) <= 1);
    CHECK(std::abs(turn.y - north) <= 1);

    CHECK(HeadingOf(turn, 0) == heading);
  }
  CHECK(TurnVector(0, 1024) == FieldPoint{0, 1024});
  CHECK(TurnVector(16, 1024) == FieldPoint{-1024, 0});
  CHECK(TurnVector(8, 1024) == FieldPoint{-724, 724});
  CHECK(TurnVector(0, 50) == FieldPoint{0, 50});
  CHECK(HeadingOf({0, 0}, 7) == 7);
}

TEST_CASE("Géométrie survivors : les directions serveur sont celles de WalkField",
          "[sim][survivors]") {
  // Depuis chaque voisin d'une source, le pas suivant porte une direction
  // serveur d : le cap 8 d pointe vers la source.
  constexpr std::array<std::string_view, 3> kRows{"...", "...", "..."};
  const ro::sim::WalkMap map = ro::sim::WalkMap::FromPicture(kRows);
  const std::array<ro::sim::CellPosition, 1> source{ro::sim::CellPosition{1, 1, 0}};
  const ro::sim::WalkField field = ro::sim::WalkField::Flood(map, source);
  int directions_seen = 0;
  for (std::int32_t y = 0; y < 3; ++y) {
    for (std::int32_t x = 0; x < 3; ++x) {
      const auto next = field.NextCellFrom(x, y);
      if (!next) continue;
      ++directions_seen;
      const FieldPoint turn =
          TurnVector(static_cast<std::uint8_t>(next->direction * kHeadingsPerDirection), 1024);
      CHECK(Sign(turn.x) == Sign(next->x - x));
      CHECK(Sign(turn.y) == Sign(next->y - y));
    }
  }
  CHECK(directions_seen == 8);
}

TEST_CASE("Géométrie survivors : un pas vers un point", "[sim][survivors]") {
  CHECK(Toward({5, 5}, {5, 5}, 1024) == FieldPoint{0, 0});
  CHECK(Toward({0, 0}, {0, 10000}, 136) == FieldPoint{0, 136});
  CHECK(Toward({0, 0}, {-10000, 0}, 51) == FieldPoint{-51, 0});
  for (std::uint8_t heading = 0; heading < kHeadingSteps; ++heading) {
    const FieldPoint from{Cells(40), Cells(40)};
    const FieldPoint to = from + TurnVector(heading, Cells(7));
    for (const std::int32_t length : {1, 64, 136, 1024}) {
      const FieldPoint step = Toward(from, to, length);
      const double norm = std::sqrt(static_cast<double>(DistanceSquared(step, {0, 0})));
      CHECK(std::abs(norm - length) <= 1.0);
    }
  }
}

TEST_CASE("HeadingOf : le meilleur produit scalaire, le premier cap en cas d'égalité",
          "[sim][survivors]") {
  const auto by_turning = [](FieldPoint delta) {
    std::uint8_t best = 0;
    std::int64_t best_dot = 0;
    for (std::uint8_t heading = 0; heading < kHeadingSteps; ++heading) {
      const FieldPoint unit = TurnVector(heading, ro::sim::kSubunitsPerCell);
      const std::int64_t dot = std::int64_t{delta.x} * unit.x + std::int64_t{delta.y} * unit.y;
      if (heading == 0 || dot > best_dot) {
        best = heading;
        best_dot = dot;
      }
    }
    return best;
  };
  constexpr std::int32_t kSpan = 40;
  constexpr std::int32_t kStretch = 311;
  for (std::int32_t x = -kSpan; x <= kSpan; ++x) {
    for (std::int32_t y = -kSpan; y <= kSpan; ++y) {
      if (x == 0 && y == 0) continue;
      const FieldPoint delta{x * kStretch, y * kStretch};
      REQUIRE(HeadingOf(delta, 0) == by_turning(delta));
    }
  }
}

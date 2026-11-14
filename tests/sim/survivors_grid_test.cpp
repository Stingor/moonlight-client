#include "ro/sim/survivors_grid.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/pcg32.h"
#include "ro/sim/survivors_geometry.h"

using ro::core::Pcg32;
using ro::sim::Cells;
using ro::sim::DistanceSquared;
using ro::sim::FieldPoint;
using ro::sim::SurvivorsGrid;

namespace {

constexpr std::int32_t kAreaCells = 64;

// Les index à distance <= radius, par le parcours de tous les points.
[[nodiscard]] std::vector<std::uint32_t> BruteNear(std::span<const FieldPoint> points,
                                                   FieldPoint center, std::int32_t radius) {
  std::vector<std::uint32_t> found;
  const std::int64_t radius_squared = static_cast<std::int64_t>(radius) * radius;
  for (std::size_t index = 0; index < points.size(); ++index) {
    if (DistanceSquared(points[index], center) <= radius_squared) {
      found.push_back(static_cast<std::uint32_t>(index));
    }
  }
  return found;
}

// Des points dans l'aire, et quelques-uns au-delà de chaque bord.
[[nodiscard]] std::vector<FieldPoint> DrawPoints(Pcg32& random, std::size_t count) {
  std::vector<FieldPoint> points;
  points.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    points.push_back({random.Between(-Cells(4), Cells(kAreaCells + 4)),
                      random.Between(-Cells(4), Cells(kAreaCells + 4))});
  }
  return points;
}

}  // namespace

TEST_CASE("Grille survivors : Near rend la force brute", "[sim][survivors]") {
  Pcg32 random(2026, 7);
  const std::vector<FieldPoint> points = DrawPoints(random, 2000);
  SurvivorsGrid grid;
  grid.Reset(kAreaCells, kAreaCells);
  grid.Rebuild(points);

  std::vector<std::uint32_t> nearby;
  for (const std::int32_t radius_cells : {0, 1, 3, 10}) {
    for (int query = 0; query < 60; ++query) {
      const FieldPoint center{random.Between(-Cells(6), Cells(kAreaCells + 6)),
                              random.Between(-Cells(6), Cells(kAreaCells + 6))};
      const std::int32_t radius = Cells(radius_cells);
      grid.Near(center, radius, points, nearby);
      CHECK(nearby == BruteNear(points, center, radius));
      CHECK(std::is_sorted(nearby.begin(), nearby.end()));
    }
    // Un centre posé sur un point le trouve, même à rayon nul.
    grid.Near(points[17], Cells(radius_cells), points, nearby);
    CHECK(std::find(nearby.begin(), nearby.end(), 17U) != nearby.end());
  }
}

TEST_CASE("Grille survivors : les points hors de l'aire restent trouvables", "[sim][survivors]") {
  const std::vector<FieldPoint> points = {
      {-Cells(30), -Cells(30)}, {Cells(500), Cells(2)}, {Cells(5), Cells(5)}, {Cells(2), -1}};
  SurvivorsGrid grid;
  grid.Reset(16, 16);
  grid.Rebuild(points);
  std::vector<std::uint32_t> nearby;
  grid.Near({-Cells(30), -Cells(30)}, Cells(1), points, nearby);
  CHECK(nearby == std::vector<std::uint32_t>{0});
  grid.Near({Cells(499), Cells(2)}, Cells(2), points, nearby);
  CHECK(nearby == std::vector<std::uint32_t>{1});
  grid.Near({Cells(2), 0}, 1, points, nearby);
  CHECK(nearby == std::vector<std::uint32_t>{3});
  grid.Near({Cells(5), Cells(5)}, -1, points, nearby);
  CHECK(nearby.empty());
}

TEST_CASE("Grille survivors : reconstruire ne réalloue pas", "[sim][survivors]") {
  Pcg32 random(9, 9);
  const std::vector<FieldPoint> first = DrawPoints(random, 300);
  const std::vector<FieldPoint> second = DrawPoints(random, 300);
  SurvivorsGrid grid;
  grid.Reset(kAreaCells, kAreaCells);
  grid.Rebuild(first);
  const std::size_t capacity = grid.capacity();
  grid.Rebuild(second);
  CHECK(grid.capacity() == capacity);

  // La grille reconstruite répond pour les nouveaux points seulement.
  std::vector<std::uint32_t> nearby;
  grid.Near(second[3], Cells(2), second, nearby);
  CHECK(nearby == BruteNear(second, second[3], Cells(2)));
}

TEST_CASE("Grille survivors : sans Reset, un seul seau", "[sim][survivors]") {
  const std::vector<FieldPoint> points = {{0, 0}, {Cells(100), 0}};
  SurvivorsGrid grid;
  grid.Rebuild(points);
  std::vector<std::uint32_t> nearby;
  grid.Near({0, 0}, Cells(200), points, nearby);
  CHECK(nearby == std::vector<std::uint32_t>{0, 1});
}

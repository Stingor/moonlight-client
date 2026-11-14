#include "ro/formats/water_field.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"

using ro::core::ErrorCode;
using ro::formats::WaterField;

TEST_CASE("WaterField::None ne rend jamais de niveau", "[formats][water]") {
  const WaterField field = WaterField::None();
  CHECK_FALSE(field.has_water());
  CHECK_FALSE(field.LevelAt(0, 0).has_value());
  CHECK_FALSE(field.LevelAt(999, 999).has_value());
}

TEST_CASE("WaterField::Uniform rend le même niveau partout", "[formats][water]") {
  const WaterField field = WaterField::Uniform(-0.5F);
  CHECK(field.has_water());
  CHECK(field.zone_count() == 1);
  CHECK(field.LevelAt(0, 0) == -0.5F);
  CHECK(field.LevelAt(123, 456) == -0.5F);
}

TEST_CASE("WaterField::Zoned associe chaque cellule GAT à sa zone", "[formats][water]") {
  // Grille GND de 4 × 4 cubes découpée en 2 × 2 zones : deux cubes par zone,
  // donc QUATRE cellules GAT par zone et par axe.
  //
  //   zone 0 | zone 1     cellules GAT 0-3 | 4-7
  //   -------+-------
  //   zone 2 | zone 3     cellules GAT 0-3 | 4-7
  auto field = WaterField::Zoned({10.0F, 20.0F, 30.0F, 40.0F}, 2, 2, 4, 4);
  REQUIRE(field.has_value());
  CHECK(field->zone_count() == 4);

  CHECK(field->LevelAt(0, 0) == 10.0F);
  CHECK(field->LevelAt(3, 3) == 10.0F);
  CHECK(field->LevelAt(4, 0) == 20.0F);
  CHECK(field->LevelAt(7, 3) == 20.0F);
  CHECK(field->LevelAt(0, 4) == 30.0F);
  CHECK(field->LevelAt(3, 7) == 30.0F);
  CHECK(field->LevelAt(4, 4) == 40.0F);
  CHECK(field->LevelAt(7, 7) == 40.0F);
}

TEST_CASE("WaterField::Zoned plafonne au-delà de la dernière zone", "[formats][water]") {
  // Une découpe qui ne divise pas exactement la grille laisse un reste : il
  // revient à la dernière zone plutôt que de sortir du tableau.
  auto field = WaterField::Zoned({1.0F, 2.0F, 3.0F}, 3, 1, 5, 1);
  REQUIRE(field.has_value());

  CHECK(field->LevelAt(0, 0) == 1.0F);
  // Cellule GAT très au-delà de la grille : plafonnée sur la dernière zone.
  CHECK(field->LevelAt(1000, 0) == 3.0F);
}

TEST_CASE("WaterField::Zoned refuse une découpe incohérente", "[formats][water]") {
  SECTION("nombre de niveaux différent du nombre de zones") {
    auto field = WaterField::Zoned({1.0F, 2.0F}, 2, 2, 4, 4);
    REQUIRE_FALSE(field.has_value());
    CHECK(field.error().code == ErrorCode::kCorruptData);
  }

  SECTION("découpe nulle") {
    auto field = WaterField::Zoned({1.0F}, 0, 1, 4, 4);
    REQUIRE_FALSE(field.has_value());
    CHECK(field.error().code == ErrorCode::kCorruptData);
  }

  SECTION("dimensions GND nulles") {
    auto field = WaterField::Zoned({1.0F}, 1, 1, 0, 0);
    REQUIRE_FALSE(field.has_value());
    CHECK(field.error().code == ErrorCode::kCorruptData);
  }
}

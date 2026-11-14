// LA CARTE VISÉE, LUE DANS L'ARCHIVE : sa grille depuis son .gat.

#include "world/navi_goal_terrain.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/walk_map.h"
#include "ro/vfs/vfs.h"

#include "support/map_builder.h"
#include "support/temp_directory.h"

using moonclient::NaviGoalLoader;
using ro::test_support::GatCellSpec;
using ro::test_support::TempDirectory;

namespace {

constexpr std::int32_t kFieldWidth = 6;
constexpr std::int32_t kFieldHeight = 4;
constexpr std::int32_t kGatWall = 1;

// Un champ dont la colonne x = 2 est un mur.
std::vector<std::uint8_t> FieldGat() {
  std::vector<GatCellSpec> cells(static_cast<std::size_t>(kFieldWidth * kFieldHeight));
  for (std::int32_t y = 0; y < kFieldHeight; ++y)
    cells[static_cast<std::size_t>(y * kFieldWidth + 2)].type = kGatWall;
  return ro::test_support::BuildGat(kFieldWidth, kFieldHeight, cells);
}

}  // namespace

TEST_CASE("Carte visée : la grille se lit dans l'archive, et se garde",
          "[app][navi_goal_terrain]") {
  TempDirectory client;
  const std::vector<std::uint8_t> gat = FieldGat();
  client.WriteFile("data/field.gat", gat);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());

  NaviGoalLoader loader;
  // Sans archive, rien ne se lit.
  CHECK(loader.GridOf("field") == nullptr);

  loader.Attach(&*vfs);
  const ro::sim::WalkMap* field = loader.GridOf("field");
  REQUIRE(field != nullptr);
  CHECK(field->width() == kFieldWidth);
  CHECK(field->height() == kFieldHeight);
  CHECK(field->Passable(1, 1));
  CHECK_FALSE(field->Passable(2, 1));
  // La même carte, quelle que soit la casse, ne se relit pas.
  CHECK(loader.GridOf("FIELD") == field);
  // Une carte sans .gat n'a pas de grille.
  CHECK(loader.GridOf("town") == nullptr);
}

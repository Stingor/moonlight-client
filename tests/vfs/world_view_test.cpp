// LA CARTE DU MONDE : ses mondes, leurs cartes et leurs donjons, lus dans
// `world_map.yml`.

#include "ro/vfs/world_view.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using ro::vfs::WorldViewTables;

namespace {

constexpr std::string_view kSource = R"(
version: 1
worlds:
  - title: "Midgard"
    image: "worldmap.jpg"
    areas:
      - [108, "prontera", 812, 587, 870, 643, "Prontera", ""]
      - [108, "prt_fild08", 812, 644, 870, 701, "Prontera Field", "1"]
      - [1, "tha_t01", 552, 5, 646, 45, "Thanatos Tower F1", "110"]
    dungeons:
      - [1, 725, 17, 745, 37, "Thanatos Tower", "110~130"]
  - title: "Midgard North"
    image: "midgard_north.jpg"
    areas:
      - [200, "north", 10, 10, 20, 20, "Nord", ""]
    dungeons: []
colors:
  dungeon_box: {red: 200, alpha: 90}
  route: {blue: 250}
  field_level: {letter: {green: 7}, sizes: [null, 15, null, null]}
  search_monsters: [{}, {alpha: 33}, {}, {}, {}]
)";

[[nodiscard]] WorldViewTables Read(std::string_view text = kSource) {
  auto tables = WorldViewTables::Parse(text);
  REQUIRE(tables.has_value());
  return std::move(*tables);
}

}  // namespace

TEST_CASE("Carte du monde : chaque monde, son image et ses cartes", "[vfs][worldmap]") {
  const WorldViewTables tables = Read();
  REQUIRE(tables.worlds().size() == 2);
  const ro::vfs::WorldView& midgard = tables.worlds()[0];
  CHECK(midgard.title == "Midgard");
  CHECK(midgard.image == "worldmap.jpg");
  REQUIRE(midgard.areas.size() == 3);
  CHECK(midgard.areas[0].map_name == "prontera");
  CHECK(midgard.areas[0].title == "Prontera");
  CHECK(midgard.areas[0].group == 108);
  CHECK(midgard.areas[0].rect.left == 812);
  CHECK(midgard.areas[0].rect.top == 587);
  CHECK(midgard.areas[0].rect.right == 870);
  CHECK(midgard.areas[0].rect.bottom == 643);
  CHECK(midgard.areas[0].level.empty());
  CHECK(midgard.areas[1].map_name == "prt_fild08");
  CHECK(midgard.areas[1].level == "1");
  REQUIRE(midgard.dungeons.size() == 1);
  CHECK(midgard.dungeons[0].group == 1);
  CHECK(midgard.dungeons[0].rect.left == 725);
  CHECK(midgard.dungeons[0].rect.bottom == 37);
  CHECK(midgard.dungeons[0].title == "Thanatos Tower");
  CHECK(midgard.dungeons[0].level == "110~130");
  CHECK(tables.worlds()[1].dungeons.empty());
}

TEST_CASE("Carte du monde : le monde d'une carte, et les teintes du fichier", "[vfs][worldmap]") {
  const WorldViewTables tables = Read();
  CHECK(tables.WorldOf("prontera") == 0U);
  CHECK(tables.WorldOf("NORTH") == 1U);
  CHECK_FALSE(tables.WorldOf("nulle_part").has_value());
  // Ce que le fichier dit remplace le défaut ; le reste le garde.
  const ro::vfs::WorldViewColors& colors = tables.colors();
  const ro::vfs::WorldViewColors defaults;
  CHECK(colors.dungeon_box.red == 200);
  CHECK(colors.dungeon_box.alpha == 90);
  CHECK(colors.dungeon_box.green == defaults.dungeon_box.green);
  CHECK(colors.route.blue == 250);
  CHECK(colors.route.red == defaults.route.red);
  CHECK(colors.field_level.letter.green == 7);
  CHECK(colors.field_level.letter.red == defaults.field_level.letter.red);
  CHECK(colors.field_level.sizes[1] == 15);
  CHECK(colors.field_level.sizes[0] == defaults.field_level.sizes[0]);
  CHECK(colors.search_monsters[1].alpha == 33);
  CHECK(colors.search_monsters[1].red == defaults.search_monsters[1].red);
  CHECK(colors.search_monsters[0].alpha == defaults.search_monsters[0].alpha);
}

TEST_CASE("Carte du monde : sans teintes, les défauts du client", "[vfs][worldmap]") {
  const WorldViewTables tables =
      Read("version: 1\nworlds: [{title: \"A\", image: \"a.jpg\", areas: [], dungeons: []}]\n");
  const ro::vfs::WorldViewColors defaults;
  CHECK(tables.colors().selected.red == defaults.selected.red);
  CHECK(tables.colors().field_name.sizes == defaults.field_name.sizes);
}

TEST_CASE("Carte du monde : un texte mal formé est refusé", "[vfs][worldmap]") {
  const auto refused = [](std::string_view text) {
    return !WorldViewTables::Parse(text).has_value();
  };
  CHECK(refused(""));
  CHECK(refused("- 1"));
  CHECK(refused("version: 2\nworlds: [{image: \"a.jpg\"}]\n"));
  // Sans monde, ou un monde sans image.
  CHECK(refused("version: 1\nworlds: []\n"));
  CHECK(refused("version: 1\nworlds: [{title: \"A\"}]\n"));
  // Une ligne qui n'a pas ses colonnes, ou une carte sans nom.
  CHECK(refused("version: 1\nworlds: [{image: \"a.jpg\", areas: [[1, \"a\", 1, 2, 3]]}]\n"));
  CHECK(refused(
      "version: 1\nworlds: [{image: \"a.jpg\", areas: [[1, \"\", 1, 2, 3, 4, \"\", \"\"]]}]\n"));
  CHECK(
      refused("version: 1\nworlds: [{image: \"a.jpg\", dungeons: [[1, 2, 3, 4, \"x\", \"\"]]}]\n"));
  // Une composante hors d'un octet, une teinte qui n'est pas une table, des
  // paliers incomplets.
  CHECK(refused("version: 1\nworlds: [{image: \"a.jpg\"}]\ncolors: {route: {red: 256}}\n"));
  CHECK(refused("version: 1\nworlds: [{image: \"a.jpg\"}]\ncolors: {route: 3}\n"));
  CHECK(
      refused("version: 1\nworlds: [{image: \"a.jpg\"}]\ncolors: {field_name: {sizes: [1, 2]}}\n"));
  CHECK(refused("version: 1\nworlds: [{image: \"a.jpg\"}]\ncolors: {search_monsters: [{}]}\n"));
  CHECK_FALSE(refused("version: 1\nworlds: [{image: \"a.jpg\"}]\n"));
}

TEST_CASE("Carte du monde : celle que le dépôt livre se lit", "[vfs][worldmap]") {
  // Le dépôt, à deux dossiers au-dessus de cet essai : CMake compile chaque
  // source sous son chemin absolu.
  const std::filesystem::path repo = std::filesystem::path(__FILE__).parent_path() / ".." / "..";
  std::ifstream shipped(repo / "dist" / "data" / "yaml_files" / "world_map.yml", std::ios::binary);
  REQUIRE(shipped.is_open());
  const std::string text((std::istreambuf_iterator<char>(shipped)),
                         std::istreambuf_iterator<char>());
  const WorldViewTables tables = Read(text);
  REQUIRE_FALSE(tables.worlds().empty());
  CHECK(tables.worlds()[0].image == "worldmap.jpg");
  CHECK(tables.WorldOf("prontera") == 0U);
}

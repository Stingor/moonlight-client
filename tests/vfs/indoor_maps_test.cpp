// LES CARTES INTÉRIEURES, lues dans leur table convertie.

#include "ro/vfs/indoor_maps.h"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::vfs::ParseIndoorMaps;

TEST_CASE("Intérieurs : les clés de table se lisent telles quelles", "[vfs][camera]") {
  // Ce que la conversion tire de « // un commentaire\nvr_gen.rsw#\r\n
  // Moc_Para0a.rsw#\n0011@infi#\n » : chaque champ fermé d'un dièse est une
  // carte, abaissée, privée de son numéro d'instance et de son extension.
  auto maps = ParseIndoorMaps("version: 1\nmaps:\n- 1@infi\n- moc_para0a\n- vr_gen\n");
  REQUIRE(maps.has_value());
  CHECK(*maps == std::vector<std::string>{"1@infi", "moc_para0a", "vr_gen"});
}

TEST_CASE("Intérieurs : une table mal formée est refusée", "[vfs][camera]") {
  CHECK_FALSE(ParseIndoorMaps("version: 1\nmaps: {a: b}\n").has_value());
  CHECK_FALSE(ParseIndoorMaps("version: 2\nmaps: []\n").has_value());
  CHECK(ParseIndoorMaps("version: 1\nmaps: []\n")->empty());
}

#include "ro/formats/navi_tables.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using ro::formats::kNoNaviMap;
using ro::formats::NaviData;
using ro::formats::ReadNaviTable;
namespace navi_link_kind = ro::formats::navi_link_kind;
namespace navi_map_kind = ro::formats::navi_map_kind;

namespace {

std::span<const std::uint8_t> Bytes(std::string_view text) {
  return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

// Une table telle que `navigation.py` l'écrit, commentaires compris.
constexpr std::string_view kTable =
    "# en-tête\n"
    "version: 1\n"
    "exact_steps: true\n"
    "maps: {\n"
    "  \"alb_ship\": {name: \"Alberta Ship\", kind: segmented_with_monsters, width: 200, "
    "height: 200},\n"
    "  \"alberta\": {name: \"Alberta\", kind: segmented, width: 280, height: 280},\n"
    "  \"1@tower\": {name: \"\", kind: plain, width: 100, height: 120},\n"
    "}\n"
    "npcs: {\n"
    "  11988: {map: \"alberta\", sprite: 98, name: \"Necko\", x: 83, y: 96},\n"
    "  11989: {map: \"ALBERTA\", shop: true, sprite: 83, name: \"Tool \\\"Dealer\\\"\", x: 87, "
    "y: 65},\n"
    "}\n"
    "spawns: {\n"
    "  17104: {map: \"alb_ship\", amount: 25, sprite: 2057, name: \"Précieux\", sprite_name: "
    "\"E_CRAMP\", level: 1, element: 1, element_level: 1, size: 1, race: 3},\n"
    "  17105: {map: \"alberta\", boss: true, amount: 1, sprite: 3, name: \"Boss\", sprite_name: "
    "\"BOSS\", level: 99, element: 0, element_level: 0, size: 0, race: 0},\n"
    "}\n"
    "links: {\n"
    "  13350: {map: \"alb_ship\", kind: portal, sprite: 99999, name: \"alb_ship_alberta_13350\", "
    "x: 26, y: 166, to: \"alberta\", to_x: 170, to_y: 168},\n"
    "  13351: {map: \"alberta\", kind: service, sprite: 4000, name: \"Warp Agent\", x: 1, y: 1, "
    "to: \"1@tower\", to_x: 2, to_y: 2},\n"
    "  13352: {map: \"alberta\", kind: portal, sprite: 99999, name: \"w\", x: 5, y: 5, to: "
    "\"1@tower\", to_x: 3, to_y: 3},\n"
    "  13353: {map: \"alberta\", kind: portal, sprite: 99999, name: \"w\", x: 6, y: 6, to: "
    "\"1@tower\", to_x: 4, to_y: 4},\n"
    "}\n"
    "passes: {\n"
    "  13350: [[13351, 8], [13352, 300, 2]],\n"
    "}\n";

// La table, une sous-chaîne remplacée.
std::string With(std::string_view from, std::string_view to) {
  std::string text(kTable);
  const std::size_t at = text.find(from);
  REQUIRE(at != std::string::npos);
  text.replace(at, from.size(), to);
  return text;
}

}  // namespace

TEST_CASE("Table de navigation : les cartes, les PNJ et les apparitions", "[formats][navi]") {
  const auto read = ReadNaviTable(Bytes(kTable));
  REQUIRE(read.has_value());
  const NaviData& data = *read;
  CHECK(data.exact_steps);
  REQUIRE(data.maps.size() == 3);
  CHECK(data.maps[1].display_name == "Alberta");
  CHECK(data.maps[0].map_kind == navi_map_kind::kSegmentedWithMonsters);
  CHECK(data.maps[2].height_cells == 120);
  // Un nom affiché vide retombe sur le nom interne.
  CHECK(data.maps[2].display_name == "1@tower");
  // La casse ne compte pas : le nom vient tantôt de la table, tantôt d'une saisie.
  CHECK(data.FindMap("ALBERTA") == 1);
  CHECK(data.FindMap("morocc") == kNoNaviMap);

  REQUIRE(data.npcs.size() == 2);
  CHECK_FALSE(data.npcs[0].is_shop());
  CHECK(data.npcs[1].is_shop());
  CHECK(data.npcs[1].name == "Tool \"Dealer\"");
  CHECK(data.npcs[0].cell_x == 83);
  CHECK(data.NpcsOn(1).size() == 2);
  CHECK(data.FindNpc(11989) == &data.npcs[1]);

  REQUIRE(data.spawns.size() == 2);
  CHECK(data.spawns[0].amount == 25);
  CHECK(data.spawns[0].sprite_class == 2057);
  CHECK(data.spawns[0].name == "Précieux");
  CHECK(data.spawns[0].element() == 1);
  CHECK(data.spawns[0].element_level() == 1);
  CHECK(data.spawns[0].size() == 1);
  CHECK(data.spawns[0].race() == 3);
  CHECK_FALSE(data.spawns[0].is_boss());
  CHECK(data.spawns[1].is_boss());
  CHECK(data.SpawnsOn(0).size() == 1);
}

TEST_CASE("Table de navigation : les liaisons et leurs distances groupées", "[formats][navi]") {
  const auto read = ReadNaviTable(Bytes(kTable));
  REQUIRE(read.has_value());
  const NaviData& data = *read;
  REQUIRE(data.links.size() == 4);
  CHECK(data.FindLink(13351)->kind == navi_link_kind::kTransportService);
  CHECK(data.FindLink(13351)->dest_map == 2);
  CHECK(data.FindLink(13350)->sprite_class == ro::formats::kNaviWarpPortalSprite);
  CHECK(data.LinksLeaving(1).size() == 3);

  // Un troisième nombre groupe des identifiants CONSÉCUTIFS à la même distance.
  const auto passes = data.PassesAfter(13350);
  REQUIRE(passes.size() == 2);
  CHECK(passes[0].first_link_id == 13351);
  CHECK(passes[0].link_count == 1);
  CHECK(passes[0].distance_cells == 8);
  CHECK(passes[1].first_link_id == 13352);
  CHECK(passes[1].link_count == 2);
  CHECK(passes[1].distance_cells == 300);
  CHECK(data.PassesAfter(13351).empty());
}

TEST_CASE("Table de navigation : des longueurs redressées se signalent", "[formats][navi]") {
  const auto read = ReadNaviTable(Bytes(With("exact_steps: true", "exact_steps: false")));
  REQUIRE(read.has_value());
  CHECK_FALSE(read->exact_steps);
}

TEST_CASE("Table de navigation : un écart refuse le fichier, en nommant la ligne",
          "[formats][navi]") {
  // Une carte, une liaison ou un champ que la table ne connaît pas.
  const std::string moved = With("map: \"alberta\", sprite: 98", "map: \"morocc\", sprite: 98");
  const auto unknown_map = ReadNaviTable(Bytes(moved));
  REQUIRE_FALSE(unknown_map.has_value());
  CHECK(unknown_map.error().detail.find("navigation.yml, ligne 10") != std::string::npos);
  CHECK_FALSE(ReadNaviTable(Bytes(With("[13352, 300, 2]", "[13352, 300, 3]"))));
  CHECK_FALSE(ReadNaviTable(Bytes(With("  13350: [[", "  13399: [["))));
  CHECK_FALSE(ReadNaviTable(Bytes(With("x: 83,", "x: 83, z: 1,"))));
  // Un champ manquant, deux fois écrit, hors bornes, ou d'une nature inconnue.
  CHECK_FALSE(ReadNaviTable(Bytes(With("name: \"Necko\", ", ""))));
  CHECK_FALSE(ReadNaviTable(Bytes(With("x: 83,", "x: 83, x: 84,"))));
  CHECK_FALSE(ReadNaviTable(Bytes(With("x: 83,", "x: 40000,"))));
  CHECK_FALSE(ReadNaviTable(Bytes(With("kind: service", "kind: teleport"))));
  CHECK_FALSE(ReadNaviTable(Bytes(With("element: 1,", "element: 20,"))));
  // Deux entrées sous la même clé.
  CHECK_FALSE(ReadNaviTable(Bytes(With("  11989:", "  11988:"))));
  CHECK_FALSE(ReadNaviTable(Bytes(With("  \"1@tower\":", "  \"Alberta\":"))));
  // Une version inconnue, une section manquante, une chaîne non fermée, du
  // texte en trop.
  CHECK_FALSE(ReadNaviTable(Bytes(With("version: 1", "version: 2"))));
  CHECK_FALSE(ReadNaviTable(Bytes(With("exact_steps: true\n", ""))));
  CHECK_FALSE(ReadNaviTable(Bytes(With("\"Necko\"", "\"Necko"))));
  CHECK_FALSE(ReadNaviTable(Bytes(std::string(kTable) + "extra: {}\n")));
  CHECK_FALSE(ReadNaviTable(Bytes(With("\\\"Dealer\\\"", "\\nDealer"))));
}

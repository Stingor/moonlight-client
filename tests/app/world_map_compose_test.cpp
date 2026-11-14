// LA VUE DE LA CARTE DU MONDE : le monde où l'on est, ses cartes, et les étages
// rangés avec leur donjon.

#include "world/world_map_compose.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/lua_table.h"
#include "ro/formats/navi_tables.h"
#include "ro/vfs/warp_agents.h"

#include "world/world_map_routes.h"
#include "world/world_map_warp_agent.h"

using moonclient::WorldMapComposer;
using moonclient::WorldMapSources;

namespace {

constexpr std::string_view kSource = R"(
version: 1
worlds:
  - title: "Midgard"
    image: "worldmap.jpg"
    areas:
      - [108, "prontera", 812, 587, 870, 643, "Prontera", ""]
      - [1, "tha_t01", 552, 5, 646, 45, "Thanatos F1", "110"]
      - [1, "tha_t02", 552, 5, 646, 45, "Thanatos F2", "110"]
      - [2, "mag_dun01", 450, 45, 550, 85, "Magma F1", "100"]
    dungeons:
      - [1, 725, 17, 745, 37, "Thanatos Tower", "110~130"]
      - [2, 704, 141, 724, 161, "Magma Dungeon", "100~110"]
  - title: "Midgard North"
    image: "midgard_north.jpg"
    areas:
      - [200, "north", 10, 10, 20, 20, "Nord", ""]
)";

[[nodiscard]] ro::vfs::WorldViewTables Tables() {
  auto tables = ro::vfs::WorldViewTables::Parse(kSource);
  REQUIRE(tables.has_value());
  return std::move(*tables);
}

}  // namespace

TEST_CASE("Carte du monde : les étages se rangent avec leur donjon", "[app][worldmap]") {
  const ro::vfs::WorldViewTables tables = Tables();
  WorldMapComposer composer;
  const ro::ui::WorldMapView& view =
      composer.Compose(WorldMapSources{.tables = &tables, .here_map = "tha_t02"});
  CHECK(view.world == 0);
  REQUIRE(view.worlds.size() == 2);
  // Prontera seule est une carte à part ; les étages vont à leur donjon.
  REQUIRE(view.areas.size() == 1);
  CHECK(view.areas[0].map_name == "prontera");
  REQUIRE(view.dungeons.size() == 2);
  REQUIRE(view.dungeons[0].floors.size() == 2);
  CHECK(view.dungeons[0].floors[1].map_name == "tha_t02");
  // L'étiquette d'un donjon est le rectangle de son premier étage ; le point
  // reste là où le donjon est.
  CHECK(view.dungeons[0].label.left == 552.0F);
  CHECK(view.dungeons[0].rect.left == 725.0F);
  REQUIRE(view.dungeons[1].floors.size() == 1);
  CHECK(view.dungeons[1].floors[0].map_name == "mag_dun01");
}

TEST_CASE("Carte du monde : le monde de la carte où l'on est, sauf choix", "[app][worldmap]") {
  const ro::vfs::WorldViewTables tables = Tables();
  WorldMapComposer composer;
  CHECK(composer.Compose(WorldMapSources{.tables = &tables, .here_map = "north"}).world == 1);
  // Une carte d'aucun monde montre le premier.
  CHECK(composer.Compose(WorldMapSources{.tables = &tables, .here_map = "ailleurs"}).world == 0);
  // Le choix de la liste l'emporte.
  const ro::ui::WorldMapView& chosen = composer.Compose(
      WorldMapSources{.tables = &tables, .here_map = "prontera", .chosen_world = 1});
  CHECK(chosen.world == 1);
  REQUIRE(chosen.areas.size() == 1);
  CHECK(chosen.areas[0].map_name == "north");
  // Sans image, la taille de référence de l'installation.
  CHECK(chosen.image_size.x == 1280.0F);
}

TEST_CASE("Carte du monde : les monstres d'une carte, boss en tête puis par niveau",
          "[app][worldmap]") {
  const ro::vfs::WorldViewTables tables = Tables();
  ro::formats::NaviData navi;
  navi.maps = {ro::formats::NaviMap{"prontera", "Prontera", 5001, 312, 392}};
  ro::formats::NaviSpawn poring{
      .map = 0, .spawn_id = 1, .amount = 50, .name = "Poring", .level = 1};
  ro::formats::NaviSpawn lunatic = poring;
  lunatic.spawn_id = 2;
  lunatic.name = "Lunatic";
  lunatic.level = 3;
  ro::formats::NaviSpawn baphomet = poring;
  baphomet.spawn_id = 3;
  baphomet.name = "Baphomet";
  baphomet.level = 81;
  baphomet.amount = 1;
  baphomet.subtype = ro::formats::navi_subtype::kBossSpawn;
  navi.spawns = {lunatic, baphomet, poring};
  navi.BuildIndexes();

  WorldMapComposer composer;
  const ro::ui::WorldMapView& view =
      composer.Compose(WorldMapSources{.tables = &tables, .here_map = "prontera", .navi = &navi});
  REQUIRE(view.areas.size() == 1);
  const auto monsters = view.areas[0].monsters;
  REQUIRE(monsters.size() == 3);
  CHECK(monsters[0].name == "Baphomet");
  CHECK(monsters[0].boss);
  CHECK(monsters[1].name == "Poring");
  CHECK(monsters[1].amount == 50);
  CHECK(monsters[2].name == "Lunatic");
}

TEST_CASE("Carte du monde : le palier d'une carte trouvée, selon son compte", "[app][worldmap]") {
  using moonclient::WorldSearchShade;
  // La carte seule, puis les cinq paliers de monstres.
  CHECK(WorldSearchShade(0) == 0);
  CHECK(WorldSearchShade(1) == 1);
  CHECK(WorldSearchShade(4) == 1);
  CHECK(WorldSearchShade(5) == 2);
  CHECK(WorldSearchShade(19) == 2);
  CHECK(WorldSearchShade(20) == 3);
  CHECK(WorldSearchShade(49) == 3);
  CHECK(WorldSearchShade(50) == 4);
  CHECK(WorldSearchShade(99) == 4);
  CHECK(WorldSearchShade(100) == 5);
  CHECK(WorldSearchShade(400) == 5);
  // Un PNJ seul, puis un PNJ avec des monstres.
  CHECK(WorldSearchShade(10000) == 10);
  CHECK(WorldSearchShade(10003) == 11);
  CHECK(WorldSearchShade(10120) == 15);
}

TEST_CASE("Carte du monde : plusieurs résultats sur une carte", "[app][worldmap]") {
  using moonclient::AddWorldSearchHit;
  using moonclient::kWorldSearchNpcMark;
  std::int32_t kept = 3;
  // Le plus grand nombre l'emporte.
  AddWorldSearchHit(kept, 30);
  CHECK(kept == 30);
  AddWorldSearchHit(kept, 10);
  CHECK(kept == 30);
  // La marque PNJ s'ajoute une fois.
  AddWorldSearchHit(kept, kWorldSearchNpcMark);
  CHECK(kept == 10030);
  AddWorldSearchHit(kept, kWorldSearchNpcMark);
  CHECK(kept == 10030);
  // Et le nombre suit sous la marque.
  AddWorldSearchHit(kept, 60);
  CHECK(kept == 10060);
  AddWorldSearchHit(kept, 40);
  CHECK(kept == 10060);
}

TEST_CASE("Carte du monde : la recherche teinte les cartes qu'elle trouve", "[app][worldmap]") {
  const ro::vfs::WorldViewTables tables = Tables();
  ro::formats::NaviData navi;
  navi.maps = {ro::formats::NaviMap{"prontera", "Prontera", 5001, 312, 392},
               ro::formats::NaviMap{"tha_t01", "Thanatos F1", 5001, 100, 100}};
  ro::formats::NaviSpawn spawn{.map = 1, .spawn_id = 1, .amount = 60, .name = "Ghost", .level = 90};
  navi.spawns = {spawn};
  navi.BuildIndexes();
  const std::vector<ro::sim::NaviHitGroup> search = {
      ro::sim::NaviHitGroup{
          .kind = ro::sim::NaviHitKind::kMap,
          .name = "Prontera",
          .hits = {ro::sim::NaviHit{.kind = ro::sim::NaviHitKind::kMap, .rank = 0}}},
      ro::sim::NaviHitGroup{
          .kind = ro::sim::NaviHitKind::kSpawn,
          .name = "Ghost",
          .hits = {ro::sim::NaviHit{.kind = ro::sim::NaviHitKind::kSpawn, .rank = 0}}},
  };
  std::vector<std::string> asked;
  const moonclient::MinimapImageAt image_at = [&asked](std::string_view path) {
    asked.emplace_back(path);
    return ro::ui::SkinImage{};
  };
  WorldMapComposer composer;
  const ro::ui::WorldMapView& view = composer.Compose(WorldMapSources{.tables = &tables,
                                                                      .here_map = "prontera",
                                                                      .navi = &navi,
                                                                      .search = search,
                                                                      .image_at = image_at});
  REQUIRE(view.areas.size() == 1);
  // Chaque palier a son image, étirée sur la carte.
  CHECK(std::ranges::find(asked, "worldview_interface\\wv_sreachmap.bmp") != asked.end());
  CHECK(std::ranges::find(asked, "worldview_interface\\wv_sreachmob4.bmp") != asked.end());
  const ro::vfs::WorldViewColors& colors = tables.colors();
  CHECK(view.areas[0].search_color == IM_COL32(colors.search_map.red, colors.search_map.green,
                                               colors.search_map.blue, colors.search_map.alpha));
  // Soixante fantômes : le quatrième palier, sur l'étage du donjon.
  const ro::vfs::WorldViewColor& fourth = colors.search_monsters[3];
  CHECK(view.dungeons[0].floors[0].search_color ==
        IM_COL32(fourth.red, fourth.green, fourth.blue, fourth.alpha));
  CHECK(view.dungeons[0].floors[1].search_color == 0U);
}

namespace {

constexpr std::string_view kRouteSource = R"(
version: 1
worlds:
  - title: "Midgard"
    image: "worldmap.jpg"
    areas:
      - [108, "izlude", 871, 644, 902, 676, "Izlude", ""]
      - [108, "prontera", 812, 587, 870, 643, "Prontera", ""]
      - [100, "yuno", 700, 300, 740, 340, "Juno", ""]
      - [3, "ice_dun01", 375, 110, 452, 150, "Ice F1", "100"]
    dungeons:
      - [3, 400, 120, 420, 140, "Ice Cave", "100"]
  - title: "Midgard North"
    image: "midgard_north.jpg"
    areas:
      - [200, "north", 10, 10, 20, 20, "Nord", ""]
)";

[[nodiscard]] ro::vfs::WorldViewTables RouteTables() {
  auto tables = ro::vfs::WorldViewTables::Parse(kRouteSource);
  REQUIRE(tables.has_value());
  return std::move(*tables);
}

}  // namespace

TEST_CASE("Carte du monde : l'itinéraire passe par les champs et les villes", "[app][worldmap]") {
  const ro::vfs::WorldViewTables tables = RouteTables();
  // Un étage de donjon n'est pas une étape ; une carte d'un autre monde en est
  // une, qui ne se montre pas ici.
  const std::vector<std::string> route = {"north", "prontera", "ice_dun01", "izlude"};
  const std::vector<moonclient::WorldRouteStep> steps = moonclient::WorldRouteSteps(tables, route);
  REQUIRE(steps.size() == 3U);
  CHECK(steps[0].world == 1U);
  CHECK(steps[2].map_name == "izlude");

  WorldMapComposer composer;
  const ro::ui::WorldMapView& view =
      composer.Compose(WorldMapSources{.tables = &tables, .here_map = "prontera", .route = route});
  REQUIRE(view.route.size() == 3U);
  CHECK_FALSE(view.route[0].shown);
  CHECK(view.route[1].shown);
  CHECK(view.route_reaches_goal);

  // Une destination au fond d'un donjon : pas de marque d'arrivée.
  const std::vector<std::string> to_dungeon = {"prontera", "ice_dun01"};
  CHECK_FALSE(
      composer
          .Compose(WorldMapSources{.tables = &tables, .here_map = "prontera", .route = to_dungeon})
          .route_reaches_goal);
}

TEST_CASE("Carte du monde : un circuit de dirigeable par escale, sans ce que le monde n'a pas",
          "[app][worldmap]") {
  const ro::vfs::WorldViewTables tables = RouteTables();
  WorldMapComposer composer;
  const ro::ui::WorldMapView& view =
      composer.Compose(WorldMapSources{.tables = &tables, .here_map = "prontera"});
  REQUIRE(view.air_lines.size() == ro::ui::kWorldMapAirLineCount);
  // La ligne intérieure : Izlude, Juno, et ra_fild12, absent de ce monde.
  const ro::ui::WorldMapAirLine& home = view.air_lines[1];
  REQUIRE(home.tours.size() == 3U);
  CHECK(home.tours[0].start == "izlude");
  REQUIRE(home.tours[0].stops.size() == 3U);
  CHECK(home.tours[0].stops[1].map_name == "yuno");
  CHECK(home.tours[0].stops[1].title == "Juno");
  CHECK(home.tours[0].stops[2].map_name == "izlude");
}

TEST_CASE("Carte du monde : la coche de l'étape en cours, sur un champ ou une ville",
          "[app][worldmap]") {
  const ro::vfs::WorldViewTables tables = RouteTables();
  WorldMapComposer composer;
  CHECK(
      composer
          .Compose(WorldMapSources{.tables = &tables, .here_map = "prontera", .step_map = "izlude"})
          .step_mark.has_value());
  CHECK_FALSE(composer
                  .Compose(WorldMapSources{
                      .tables = &tables, .here_map = "prontera", .step_map = "ice_dun01"})
                  .step_mark.has_value());
}

TEST_CASE("Carte du monde : le groupe, chacun sur sa carte", "[app][worldmap]") {
  ro::sim::PartyRoster roster;
  std::vector<std::string> maps;
  // Hors groupe, soi-même seul, sur la carte où l'on est.
  const std::vector<moonclient::WorldMapMember> alone =
      moonclient::WorldMapPartyMembers(roster, 7, "Moi", "prontera", maps);
  REQUIRE(alone.size() == 1U);
  CHECK(alone[0].self);
  CHECK(alone[0].map_name == "prontera");

  roster.Replace("Groupe",
                 {ro::sim::PartyMember{.account_id = 7, .name = "Moi", .map = "prontera.gat"},
                  ro::sim::PartyMember{
                      .account_id = 8, .name = "Absent", .map = "izlude.gat", .offline = true},
                  ro::sim::PartyMember{.account_id = 9, .name = "Ami", .map = "Izlude.gat"},
                  ro::sim::PartyMember{.account_id = 10, .name = "Voisin", .map = "prontera.gat"}});
  const std::vector<moonclient::WorldMapMember> members =
      moonclient::WorldMapPartyMembers(roster, 7, "Moi", "prontera", maps);
  // Les absents ne paraissent pas ; l'extension tombe, la casse aussi.
  REQUIRE(members.size() == 3U);
  CHECK(members[1].map_name == "izlude");

  const ro::vfs::WorldViewTables tables = RouteTables();
  WorldMapComposer composer;
  const ro::ui::WorldMapView& view = composer.Compose(
      WorldMapSources{.tables = &tables, .here_map = "prontera", .party = members});
  REQUIRE(view.party.size() == 2U);
  CHECK(view.party[0].map_name == "prontera");
  CHECK(view.party[0].here);
  REQUIRE(view.party[0].members.size() == 2U);
  CHECK(view.party[0].members[0].self);
  CHECK(view.party[0].members[1].name == "Voisin");
  CHECK_FALSE(view.party[1].here);
}

TEST_CASE("Carte du monde : l'aperçu de la carte désignée, sa minimap et son nom",
          "[app][worldmap]") {
  const ro::vfs::WorldViewTables tables = RouteTables();
  std::vector<std::string> asked;
  const moonclient::MinimapImageAt image_at = [&asked](std::string_view path) {
    asked.emplace_back(path);
    return ro::ui::SkinImage{};
  };
  WorldMapComposer composer;
  const ro::ui::WorldMapView& view = composer.Compose(WorldMapSources{
      .tables = &tables, .here_map = "prontera", .preview_map = "izlude", .image_at = image_at});
  REQUIRE(view.preview.has_value());
  CHECK(view.preview->map_name == "izlude");
  CHECK(view.preview->title == "Izlude");
  CHECK(std::ranges::find(asked, "map\\izlude.bmp") != asked.end());
  CHECK_FALSE(composer.Compose(WorldMapSources{.tables = &tables, .here_map = "prontera"})
                  .preview.has_value());
}

TEST_CASE("Carte du monde : le Warp Agent tourne son attente en quatre secondes",
          "[app][worldmap]") {
  CHECK(moonclient::WorldMapWarpAgentFrame(4, 0) == 0U);
  CHECK(moonclient::WorldMapWarpAgentFrame(4, 1000) == 1U);
  CHECK(moonclient::WorldMapWarpAgentFrame(4, 7999) == 3U);
  CHECK(moonclient::WorldMapWarpAgentFrame(3, 3999) == 2U);
  CHECK(moonclient::WorldMapWarpAgentFrame(0, 1234) == 0U);
  CHECK(moonclient::WorldMapWarpAgentScale(3) == Catch::Approx(1.1F));
}

TEST_CASE("Carte du monde : le Warp Agent de la ville survolée, sa figure et rien d'autre",
          "[app][worldmap]") {
  constexpr std::string_view kAgents = R"yaml(
version: 1
agents:
  prontera: {sprite: "4_F_01", class: 90}
  ailleurs: {sprite: "4_F_01", class: 90}
)yaml";
  auto parsed = ro::vfs::WarpAgents::Parse(kAgents);
  REQUIRE(parsed.has_value());
  const ro::vfs::WarpAgents agents = std::move(*parsed);
  const moonclient::WorldMapWarpAgent::Shown figure{
      .image = ro::ui::SkinImage{.id = 1, .width = 40.0F, .height = 60.0F},
      .feet = ImVec2(20.0F, 55.0F),
      .height = 50.0F};

  const ro::vfs::WorldViewTables tables = RouteTables();
  WorldMapComposer composer;
  const ro::ui::WorldMapView& view =
      composer.Compose(WorldMapSources{.tables = &tables,
                                       .here_map = "prontera",
                                       .preview_map = "prontera",
                                       .warp_agents = &agents,
                                       .warp_agent_sprite = &figure});
  REQUIRE(view.warp_agent.has_value());
  CHECK(view.warp_agent->town_map == "prontera");
  CHECK(view.warp_agent->town.left == 812.0F);
  CHECK(view.warp_agent->town.right == 870.0F);
  CHECK(view.warp_agent->sprite.width == 40.0F);
  CHECK(view.warp_agent->sprite_feet.y == 55.0F);
  CHECK(view.warp_agent->sprite_height == 50.0F);

  const auto agent_on = [&](std::string_view preview_map) {
    return composer
        .Compose(WorldMapSources{.tables = &tables,
                                 .here_map = "prontera",
                                 .preview_map = preview_map,
                                 .warp_agents = &agents,
                                 .warp_agent_sprite = &figure})
        .warp_agent.has_value();
  };
  // Une ville sans Warp Agent, une carte que le monde ne montre pas, aucune
  // carte survolée : rien.
  CHECK_FALSE(agent_on("izlude"));
  CHECK_FALSE(agent_on("ailleurs"));
  CHECK_FALSE(agent_on(""));
  // Sans table, rien non plus.
  CHECK_FALSE(composer
                  .Compose(WorldMapSources{
                      .tables = &tables, .here_map = "prontera", .preview_map = "prontera"})
                  .warp_agent.has_value());
}

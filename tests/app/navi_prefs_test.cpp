// LES PRÉFÉRENCES DE LA NAVIGATION ET LA VUE DE LA MINIMAP.
//
// 🔴 CE QUE CE BANC TIENT : les réglages et les marqueurs survivent à un aller-
// retour sur le disque, un fichier abîmé ou hors bornes rend les défauts plutôt
// qu'un client qui refuse de démarrer, et la vue de la minimap montre les
// portails du graphe, le groupe sur la carte courante seulement, les marques de
// quête par leur couleur, les coéquipiers du champ de bataille, et oriente la flèche du joueur dans
// la convention que la fenêtre attend.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/navi_tables.h"
#include "ro/sim/quest_markers.h"
#include "ro/sim/world.h"

#include "game/save_folder.h"
#include "world/minimap_compose.h"
#include "world/navi_file.h"

using moonclient::LoadNaviPrefs;
using moonclient::MinimapComposer;
using moonclient::MinimapFacingDegrees;
using moonclient::MinimapSources;
using moonclient::NaviMemo;
using moonclient::NaviPrefs;
using moonclient::SaveNaviPrefs;

namespace {

std::filesystem::path FreshRoot(const char* name) {
  const std::filesystem::path root = std::filesystem::temp_directory_path() / name;
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / moonclient::kSaveFolder);
  return root;
}

}  // namespace

TEST_CASE("Navigation : réglages et marqueurs font l'aller-retour", "[app][navigation]") {
  const std::filesystem::path root = FreshRoot("moonclient_navi_prefs");
  NaviPrefs written;
  ro::ui::MinimapLayout& legacy = written.legacy_minimap.emplace();
  legacy.settings.offset_x_px = 123.0F;
  legacy.settings.side_px = 300;
  legacy.settings.zoom = 2.5F;
  legacy.settings.click_through = true;
  legacy.settings.show_guild = false;
  legacy.settings.show_portals = false;
  legacy.shown = false;
  written.trail_icon = 5;
  written.memos["prontera"] = {NaviMemo{.cell_x = 150, .cell_y = 180, .name = "Fontaine « é »"}};
  REQUIRE(SaveNaviPrefs(root, written));

  NaviPrefs read;
  LoadNaviPrefs(root, read);
  // La présentation d'un ancien fichier se relit et se réécrit telle quelle.
  REQUIRE(read.legacy_minimap.has_value());
  CHECK(*read.legacy_minimap == legacy);
  CHECK(read.trail_icon == 5);
  REQUIRE(read.memos.count("prontera") == 1);
  CHECK(read.memos["prontera"][0].name == "Fontaine « é »");
  CHECK(read.memos["prontera"][0].cell_y == 180);
  std::filesystem::remove_all(root);
}

TEST_CASE("Navigation : un fichier abîmé ou hors bornes rend les défauts", "[app][navigation]") {
  const std::filesystem::path root = FreshRoot("moonclient_navi_broken");
  {
    std::ofstream out(moonclient::NaviPrefsPath(root));
    out << "minimap:\n  zoom: 99\n  backdrop_opacity: -40\ntrail_icon: 42\n";
  }
  NaviPrefs read;
  LoadNaviPrefs(root, read);
  REQUIRE(read.legacy_minimap.has_value());
  CHECK(read.legacy_minimap->settings.zoom == ro::ui::kMinimapZoomMost);
  CHECK(read.legacy_minimap->settings.backdrop_percent == 0);
  CHECK(read.trail_icon == ro::ui::kNavigationTrailIconCount);
  {
    std::ofstream out(moonclient::NaviPrefsPath(root));
    out << "{ ceci n'est pas : du yaml";
  }
  NaviPrefs defaults;
  LoadNaviPrefs(root, defaults);
  CHECK(defaults.trail_icon == 1);
  std::filesystem::remove_all(root);
}

TEST_CASE("Navigation : sans présentation ancienne, rien ne s'écrit et l'usine hérite",
          "[app][navigation][disposition]") {
  const std::filesystem::path root = FreshRoot("moonclient_navi_no_legacy");
  // Aucun fichier : la présentation héritée est celle d'usine.
  CHECK(moonclient::InheritedMinimap(root) == ro::ui::MinimapLayout{});

  NaviPrefs written;
  written.memos["payon"] = {NaviMemo{.cell_x = 1, .cell_y = 2, .name = "a"}};
  REQUIRE(SaveNaviPrefs(root, written));
  NaviPrefs read;
  LoadNaviPrefs(root, read);
  CHECK_FALSE(read.legacy_minimap.has_value());
  CHECK(read.memos.count("payon") == 1);
  CHECK(moonclient::InheritedMinimap(root) == ro::ui::MinimapLayout{});

  // Un fichier d'un joueur existant : sa présentation est celle qu'on hérite.
  {
    std::ofstream out(moonclient::NaviPrefsPath(root));
    out << "minimap:\n  shown: false\n  side: 333\n  zoom: 2\n";
  }
  const ro::ui::MinimapLayout inherited = moonclient::InheritedMinimap(root);
  CHECK_FALSE(inherited.shown);
  CHECK(inherited.settings.side_px == 333);
  CHECK(inherited.settings.zoom == 2.0F);
  CHECK(inherited.settings.show_party);
  std::filesystem::remove_all(root);
}

TEST_CASE("Minimap : la flèche suit l'angle de visée du client", "[app][navigation]") {
  // Nord cent quatre-vingts, ouest deux cent soixante-dix, sud zéro, est
  // quatre-vingt-dix : la fenêtre tourne alors de 180 moins cet angle.
  CHECK(MinimapFacingDegrees(0) == 180.0F);
  CHECK(MinimapFacingDegrees(2) == 270.0F);
  CHECK(MinimapFacingDegrees(4) == 0.0F);
  CHECK(MinimapFacingDegrees(6) == 90.0F);
}

TEST_CASE("Minimap : les portails du graphe sont des icônes libellées de leur destination",
          "[app][navigation]") {
  ro::sim::World world;
  world.EnterMap("prontera", 150, 150);
  ro::formats::NaviData navi;
  navi.maps = {ro::formats::NaviMap{"prontera", "Prontera", 5003, 312, 392},
               ro::formats::NaviMap{"prt_fild08", "Prontera Field", 5001, 400, 400}};
  ro::formats::NaviLink portal;
  portal.map = 0;
  portal.link_id = 1;
  portal.cell_x = 150;
  portal.cell_y = 20;
  portal.dest_map = 1;
  ro::formats::NaviLink agent = portal;
  agent.link_id = 2;
  agent.sprite_class = 874;
  navi.links = {portal, agent};
  navi.BuildIndexes();

  MinimapSources sources;
  sources.world = &world;
  sources.cells_width = 312;
  sources.cells_height = 392;
  sources.navi = &navi;
  sources.image_at = [](std::string_view) { return ro::ui::SkinImage{}; };
  MinimapComposer composer;
  const ro::ui::MinimapView& view = composer.Compose(sources);
  CHECK(view.map_name == "prontera");
  CHECK(view.cells_width == 312);
  // Le portail seul, dans SA couche : un PNJ qui transporte n'est pas un portail,
  // et un portail n'est pas une commodité.
  CHECK(view.town.empty());
  REQUIRE(view.portals.size() == 1);
  CHECK(view.portals[0].cell.y == 20);
  CHECK(view.portals[0].label.find("Prontera Field") != std::string_view::npos);
}

TEST_CASE("Minimap : une marque de quête prend l'icône de sa COULEUR, pas de son effet",
          "[app][navigation]") {
  constexpr std::uint32_t kYellowNpc = 3'011'809;
  constexpr std::uint32_t kPurpleNpc = 3'011'810;
  constexpr std::uint32_t kUncolouredNpc = 3'011'811;
  constexpr std::uint32_t kRemovedNpc = 3'011'812;
  constexpr std::uint16_t kYellow = 1;
  constexpr std::uint16_t kPurple = 3;
  ro::sim::World world;
  world.EnterMap("payon", 150, 150);
  world.quest_markers().Tell(kYellowNpc,
                             {.cell_x = 154, .cell_y = 101, .effect = 0, .color = kYellow});
  world.quest_markers().Tell(kPurpleNpc,
                             {.cell_x = 40, .cell_y = 60, .effect = 7, .color = kPurple});
  // Sans couleur : au-dessus du PNJ seulement. Retirée : nulle part.
  world.quest_markers().Tell(kUncolouredNpc, {.cell_x = 80, .cell_y = 80, .effect = 2, .color = 0});
  world.quest_markers().Tell(
      kRemovedNpc,
      {.cell_x = 90, .cell_y = 90, .effect = ro::sim::kQuestMarkerRemovedEffect, .color = kYellow});

  // Chaque image demandée reçoit pour identifiant son rang de demande.
  std::vector<std::string> asked;
  MinimapSources sources;
  sources.world = &world;
  sources.cells_width = 300;
  sources.cells_height = 300;
  sources.image_at = [&asked](std::string_view path) {
    asked.emplace_back(path);
    return ro::ui::SkinImage{
        .id = static_cast<ImTextureID>(asked.size()), .width = 8.0F, .height = 8.0F};
  };
  MinimapComposer composer;
  const ro::ui::MinimapView& view = composer.Compose(sources);
  const auto path_of = [&asked](const ro::ui::SkinImage& image) {
    return asked.at(static_cast<std::size_t>(image.id) - 1);
  };
  REQUIRE(view.quests.size() == 2);
  // Rangées par PNJ : le jaune, puis le violet.
  CHECK(view.quests[0].cell.x == 154);
  CHECK(view.quests[0].cell.y == 101);
  CHECK(path_of(view.quests[0].image) == "basic_interface\\quest_1.bmp");
  CHECK(path_of(view.quests[1].image) == "basic_interface\\quest_3.bmp");
  // Et l'icône de la grande carte : la sorte de quête, puis la couleur.
  CHECK(path_of(view.quests[0].kind_image) == "minimap\\Quest_0_1.bmp");
  CHECK(path_of(view.quests[1].kind_image) == "minimap\\Quest_7_3.bmp");
  // Les quatre flèches du collage au bord : gauche, droite, haut, bas.
  CHECK(path_of(view.quest_arrows[0]) == "basic_interface\\quest_arrow1.bmp");
  CHECK(path_of(view.quest_arrows[1]) == "basic_interface\\quest_arrow2.bmp");
  CHECK(path_of(view.quest_arrows[2]) == "basic_interface\\quest_arrow3.bmp");
  CHECK(path_of(view.quest_arrows[3]) == "basic_interface\\quest_arrow4.bmp");

  // À l'entrée sur une autre carte, plus rien.
  world.EnterMap("prontera", 150, 150);
  CHECK(composer.Compose(sources).quests.empty());
}

TEST_CASE("Minimap : les coéquipiers du champ, nous exceptés, dans la couleur du camp",
          "[app][navigation][battleground]") {
  constexpr std::uint32_t kSelf = 2'000'001;
  constexpr std::uint32_t kFirstMate = 2'000'002;
  constexpr std::uint32_t kSecondMate = 2'000'003;
  constexpr std::uint16_t kKnightJob = 7;
  constexpr ImU32 kCampColor = IM_COL32(40, 60, 210, 255);
  ro::sim::World world;
  world.EnterMap("bat_a01", 50, 50);
  world.set_player_id(kSelf);
  ro::sim::Battlefield& field = world.battlefield();
  field.TellMate(kSelf, "Moi", kKnightJob, 50, 50);
  field.TellMate(kFirstMate, "Brunhild", kKnightJob, 30, 70);
  field.TellMate(kSecondMate, "Sigrun", kKnightJob, 70, 30);

  MinimapSources sources;
  sources.world = &world;
  sources.cells_width = 100;
  sources.cells_height = 100;
  sources.battle_mate_color = kCampColor;
  MinimapComposer composer;

  // Le réglage éteint : aucun coéquipier.
  CHECK(composer.Compose(sources).battle_mates.empty());

  sources.battle_mates_shown = true;
  const ro::ui::MinimapView& view = composer.Compose(sources);
  REQUIRE(view.battle_mates.size() == 2);
  CHECK(view.battle_mates[0].name == "Brunhild");
  CHECK(view.battle_mates[0].cell.x == 30);
  CHECK(view.battle_mates[0].cell.y == 70);
  CHECK(view.battle_mates[0].color == kCampColor);
  CHECK(view.battle_mates[1].name == "Sigrun");
  CHECK(view.battle_mates[1].cell.x == 70);
  CHECK(view.battle_mates[1].color == kCampColor);

  // LES NOMS SONT RECOPIÉS : la vue tient jusqu'à la composition suivante,
  // même si le monde retire le coéquipier entre-temps.
  field.TellMate(kFirstMate, "", 0, -1, -1);
  CHECK(view.battle_mates[0].name == "Brunhild");
  const ro::ui::MinimapView& next = composer.Compose(sources);
  REQUIRE(next.battle_mates.size() == 1);
  CHECK(next.battle_mates[0].name == "Sigrun");
}

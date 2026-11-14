// LES PROFILS DE DISPOSITION SUR LE DISQUE : un aller-retour fidèle, le profil
// de chaque personnage, et un fichier d'une autre version qu'on ne touche pas.

#include "game/ui_layout_file.h"

#include <catch2/catch_test_macros.hpp>

#include "support/temp_directory.h"

using moonclient::kDefaultProfileName;
using moonclient::UiLayoutBook;
using ro::test_support::TempDirectory;

namespace {

[[nodiscard]] ro::ui::LayoutProfile Sample() {
  ro::ui::LayoutProfile layout;
  layout.places.emplace("inventory", ro::ui::WindowPlace{.across = ro::ui::AnchorSide::kEnd,
                                                         .down = ro::ui::AnchorSide::kCenter,
                                                         .offset_x = 17.5F,
                                                         .offset_y = -40.0F,
                                                         .width = 301.0F,
                                                         .height = 203.0F,
                                                         .collapsed = true});
  layout.places.emplace(
      "Chat",
      ro::ui::WindowPlace{.offset_x = 3.0F, .offset_y = 4.0F, .width = 400.0F, .height = 180.0F});
  layout.toggles.emplace("log", true);
  layout.toggles.emplace("inventory_pin", false);
  ro::ui::MinimapLayout& minimap = layout.minimap.emplace();
  minimap.shown = false;
  minimap.settings.across = ro::ui::AnchorSide::kEnd;
  minimap.settings.down = ro::ui::AnchorSide::kCenter;
  minimap.settings.offset_x_px = 40.0F;
  minimap.settings.offset_y_px = 72.5F;
  minimap.settings.side_px = 260;
  minimap.settings.zoom = 2.5F;
  minimap.settings.click_through = true;
  minimap.settings.backdrop_percent = 35;
  minimap.settings.arrow_tint = 0xFF00'80FFU;
  minimap.settings.show_quests = false;
  return layout;
}

}  // namespace

TEST_CASE("Dispositions : ce qu'on écrit se relit tel quel", "[app][disposition]") {
  UiLayoutBook written;
  written.Put(kDefaultProfileName, Sample());
  written.Put("Farm", ro::ui::LayoutProfile{});
  written.Bind("Stingor", "Farm");

  UiLayoutBook read;
  REQUIRE(moonclient::ReadUiLayouts(moonclient::WriteUiLayouts(written), read));
  CHECK(read == written);
}

TEST_CASE("Dispositions : et sur le disque aussi", "[app][disposition]") {
  const TempDirectory folder;
  UiLayoutBook written;
  written.Put(kDefaultProfileName, Sample());
  REQUIRE(moonclient::SaveUiLayouts(folder.path(), written));

  UiLayoutBook read;
  REQUIRE(moonclient::LoadUiLayouts(folder.path(), read));
  CHECK(read == written);
}

TEST_CASE("Dispositions : un fichier absent est un livre vide", "[app][disposition]") {
  const TempDirectory folder;
  UiLayoutBook read;
  CHECK(moonclient::LoadUiLayouts(folder.path(), read));
  CHECK(read.profiles.empty());
}

TEST_CASE("Dispositions : une autre version ne se relit pas", "[app][disposition]") {
  UiLayoutBook read;
  read.Put("garde", ro::ui::LayoutProfile{});
  CHECK_FALSE(moonclient::ReadUiLayouts("version: 99\nprofiles: []\n", read));
  // Le livre reste tel quel : rien ne doit l'écraser ensuite.
  CHECK(read.Find("garde") != nullptr);
  CHECK_FALSE(moonclient::ReadUiLayouts("[1, 2", read));
}

TEST_CASE("Dispositions : une place incomplète est sautée, le reste se lit", "[app][disposition]") {
  UiLayoutBook read;
  REQUIRE(
      moonclient::ReadUiLayouts("version: 1\n"
                                "profiles:\n"
                                "  - name: Default\n"
                                "    windows:\n"
                                "      - key: a\n"
                                "        anchor: [start, nowhere]\n"
                                "        offset: [1, 2]\n"
                                "        size: [3, 4]\n"
                                "      - key: b\n"
                                "        anchor: [end, end]\n"
                                "        offset: [1, 2]\n"
                                "        size: [3, 4]\n",
                                read));
  const moonclient::Named<ro::ui::LayoutProfile>* named = read.Find(kDefaultProfileName);
  REQUIRE(named != nullptr);
  CHECK(named->profile.places.size() == 1);
  CHECK(named->profile.places.contains("b"));
}

TEST_CASE("Dispositions : l'ouverture des fenêtres et le rangement du compteur se relisent",
          "[app][disposition]") {
  ro::ui::LayoutProfile layout;
  layout.toggles.emplace("inventory", true);
  layout.toggles.emplace("cart", false);
  layout.toggles.emplace("###world_map", true);
  layout.toggles.emplace("quest_log", true);
  layout.toggles.emplace("damage_meter.docked", true);
  UiLayoutBook written;
  written.Put(kDefaultProfileName, layout);

  UiLayoutBook read;
  REQUIRE(moonclient::ReadUiLayouts(moonclient::WriteUiLayouts(written), read));
  CHECK(read == written);
}

TEST_CASE("Dispositions : un profil d'avant ne nomme que ce qu'il connaissait",
          "[app][disposition]") {
  // ⚠ RIEN N'Y EST AJOUTÉ À LA LECTURE : une clé absente laisse la fenêtre, et
  // le compteur, tels qu'ils sont au moment de poser le profil.
  UiLayoutBook read;
  REQUIRE(
      moonclient::ReadUiLayouts("version: 1\n"
                                "profiles:\n"
                                "  - name: Default\n"
                                "    toggles:\n"
                                "      chat: true\n"
                                "      inventory.pinned: true\n",
                                read));
  const moonclient::Named<ro::ui::LayoutProfile>* named = read.Find(kDefaultProfileName);
  REQUIRE(named != nullptr);
  CHECK(named->profile.toggles.size() == 2);
  CHECK_FALSE(named->profile.toggles.contains("inventory"));
  CHECK_FALSE(named->profile.toggles.contains("damage_meter.docked"));
}

TEST_CASE("Dispositions : le profil d'un personnage, ou « Default »", "[app][disposition]") {
  UiLayoutBook book;
  book.Put(kDefaultProfileName, ro::ui::LayoutProfile{});
  book.Put("Farm", ro::ui::LayoutProfile{});
  book.Bind("Stingor", "Farm");
  CHECK(book.ProfileOf("Stingor") == "Farm");
  CHECK(book.ProfileOf("Inconnu") == kDefaultProfileName);

  // SUPPRIMER UN PROFIL RAMÈNE SES PERSONNAGES À « Default ».
  book.Remove("Farm");
  CHECK(book.Find("Farm") == nullptr);
  CHECK(book.ProfileOf("Stingor") == kDefaultProfileName);

  // ET « Default » NE SE SUPPRIME PAS.
  book.Remove(kDefaultProfileName);
  CHECK(book.Find(kDefaultProfileName) != nullptr);
}

TEST_CASE("Dispositions : un profil sans minimap se relit sans minimap", "[app][disposition]") {
  // Un fichier écrit avant que la minimap n'entre au profil : il hérite.
  UiLayoutBook read;
  REQUIRE(
      moonclient::ReadUiLayouts("version: 1\n"
                                "profiles:\n"
                                "  - name: Default\n"
                                "    windows: []\n"
                                "    toggles: {chat: true}\n",
                                read));
  const moonclient::Named<ro::ui::LayoutProfile>* named = read.Find(kDefaultProfileName);
  REQUIRE(named != nullptr);
  CHECK_FALSE(named->profile.minimap.has_value());
  CHECK(named->profile.toggles.at("chat"));
  // Et l'absence survit à l'aller-retour.
  UiLayoutBook again;
  REQUIRE(moonclient::ReadUiLayouts(moonclient::WriteUiLayouts(read), again));
  CHECK(again == read);
}

TEST_CASE("Dispositions : une minimap abîmée se lit au mieux, sans emporter le profil",
          "[app][disposition]") {
  UiLayoutBook read;
  REQUIRE(
      moonclient::ReadUiLayouts("version: 1\n"
                                "profiles:\n"
                                "  - name: Default\n"
                                "    toggles: {chat: true}\n"
                                "    minimap: {side: [1, 2], zoom: 40, backdrop_opacity: -3}\n",
                                read));
  const moonclient::Named<ro::ui::LayoutProfile>* named = read.Find(kDefaultProfileName);
  REQUIRE(named != nullptr);
  CHECK(named->profile.toggles.at("chat"));
  // Le champ illisible garde son défaut, les autres sont ramenés dans leurs bornes.
  REQUIRE(named->profile.minimap.has_value());
  CHECK(named->profile.minimap->settings.side_px == ro::ui::MinimapSettings{}.side_px);
  CHECK(named->profile.minimap->settings.zoom == ro::ui::kMinimapZoomMost);
  CHECK(named->profile.minimap->settings.backdrop_percent == 0);
}

TEST_CASE("Dispositions : une minimap écrite avant l'ancre garde son coin haut-gauche",
          "[app][disposition]") {
  UiLayoutBook read;
  REQUIRE(
      moonclient::ReadUiLayouts("version: 1\n"
                                "profiles:\n"
                                "  - name: Default\n"
                                "    minimap: {x: 1600, y: 700}\n",
                                read));
  const moonclient::Named<ro::ui::LayoutProfile>* named = read.Find(kDefaultProfileName);
  REQUIRE(named != nullptr);
  REQUIRE(named->profile.minimap.has_value());
  const ro::ui::MinimapSettings& settings = named->profile.minimap->settings;
  CHECK(settings.across == ro::ui::AnchorSide::kStart);
  CHECK(settings.down == ro::ui::AnchorSide::kStart);
  CHECK(settings.offset_x_px == 1600.0F);
  CHECK(settings.offset_y_px == 700.0F);
}

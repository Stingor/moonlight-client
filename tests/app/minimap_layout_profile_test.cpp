// LA MINIMAP SUIT LE PROFIL DE DISPOSITION : un réglage changé s'enregistre
// dans le profil posé, poser un autre profil la change, un profil qui n'en a
// pas hérite de `navigation.yml`, et « Réinitialiser » rend ses défauts
// d'usine — éprouvé par le gardien des profils sur un port réduit à la
// minimap, sans fenêtre.

#include <cstdint>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "game/minimap_layout_slot.h"
#include "game/profile_keeper.h"
#include "game/ui_layout_file.h"
#include "support/temp_directory.h"
#include "world/navi_file.h"

namespace {

using moonclient::kDefaultProfileName;
using moonclient::UiLayoutBook;
using ro::test_support::TempDirectory;

// LE PORT DE LA DISPOSITION, sa minimap seule, et un fichier en mémoire.
class MinimapOnlyPort final : public moonclient::ProfilePort<ro::ui::LayoutProfile> {
 public:
  explicit MinimapOnlyPort(const std::filesystem::path& client_root) : slot(client_root) {}

  moonclient::MinimapLayoutSlot slot;
  UiLayoutBook disk;

  [[nodiscard]] std::uint64_t Revision() override { return slot.Revision(); }
  [[nodiscard]] ro::ui::LayoutProfile Capture() override {
    ro::ui::LayoutProfile layout;
    slot.Capture(layout);
    return layout;
  }
  void Pose(const ro::ui::LayoutProfile& profile) override { slot.Pose(profile); }
  // Ce que la disposition d'usine porte de la minimap : ses défauts.
  [[nodiscard]] ro::ui::LayoutProfile Defaults() override {
    ro::ui::LayoutProfile factory;
    factory.minimap.emplace();
    return factory;
  }
  [[nodiscard]] bool Load(UiLayoutBook& into) override {
    into = disk;
    return true;
  }
  void Save(const UiLayoutBook& from) override { disk = from; }
};

[[nodiscard]] moonclient::ProfileFamilyText Text() {
  return moonclient::ProfileFamilyText{.title = "t", .help = "h", .id = "layout"};
}

// Assez pour que le calme soit revenu.
constexpr std::uint32_t kQuietLater = 5000;

// Un côté qu'aucun défaut ne porte.
constexpr int kLegacySide = 333;
constexpr int kFarmSide = 410;
constexpr int kDraggedSide = 280;

[[nodiscard]] ro::ui::LayoutProfile WithMinimap(int side_px, bool shown) {
  ro::ui::LayoutProfile layout;
  layout.minimap.emplace();
  layout.minimap->settings.side_px = side_px;
  layout.minimap->shown = shown;
  return layout;
}

[[nodiscard]] const ro::ui::LayoutProfile* Stored(const MinimapOnlyPort& port,
                                                  std::string_view name) {
  const moonclient::Named<ro::ui::LayoutProfile>* named = port.disk.Find(name);
  return named != nullptr ? &named->profile : nullptr;
}

// LE `navigation.yml` D'UN JOUEUR EXISTANT : sa minimap réglée avant les profils.
void WriteLegacyNavigation(const TempDirectory& root) {
  moonclient::NaviPrefs prefs;
  ro::ui::MinimapLayout& legacy = prefs.legacy_minimap.emplace();
  legacy.settings.side_px = kLegacySide;
  legacy.shown = false;
  REQUIRE(moonclient::SaveNaviPrefs(root.path(), prefs));
}

}  // namespace

TEST_CASE("Minimap au profil : un réglage changé s'enregistre dans le profil posé",
          "[app][disposition][navigation]") {
  const TempDirectory root;
  MinimapOnlyPort port(root.path());
  port.disk.Put(kDefaultProfileName, WithMinimap(kFarmSide, true));
  moonclient::ProfileKeeper<ro::ui::LayoutProfile> keeper(port, &Text);
  keeper.Start(true);
  CHECK(port.slot.minimap().settings.side_px == kFarmSide);

  // Le joueur tire la poignée, puis masque la minimap au raccourci.
  port.slot.minimap().settings.side_px = kDraggedSide;
  port.slot.minimap().shown = false;
  keeper.Tick(1);
  keeper.Tick(kQuietLater);
  const ro::ui::LayoutProfile* stored = Stored(port, kDefaultProfileName);
  REQUIRE(stored != nullptr);
  REQUIRE(stored->minimap.has_value());
  CHECK(stored->minimap->settings.side_px == kDraggedSide);
  CHECK_FALSE(stored->minimap->shown);
}

TEST_CASE("Minimap au profil : poser un autre profil change la minimap",
          "[app][disposition][navigation]") {
  const TempDirectory root;
  MinimapOnlyPort port(root.path());
  port.disk.Put(kDefaultProfileName, WithMinimap(kDraggedSide, true));
  port.disk.Put("Farm", WithMinimap(kFarmSide, false));
  port.disk.Bind("Stingor", "Farm");
  moonclient::ProfileKeeper<ro::ui::LayoutProfile> keeper(port, &Text);
  keeper.Start(true);
  CHECK(port.slot.minimap().settings.side_px == kDraggedSide);
  CHECK(port.slot.minimap().shown);

  keeper.EnterCharacter("Stingor");
  CHECK(port.slot.minimap().settings.side_px == kFarmSide);
  CHECK_FALSE(port.slot.minimap().shown);

  // Poser un profil n'est pas un changement du joueur : rien à réécrire.
  keeper.Tick(1);
  keeper.Tick(kQuietLater);
  CHECK(Stored(port, kDefaultProfileName)->minimap->settings.side_px == kDraggedSide);
}

TEST_CASE("Minimap au profil : un profil sans minimap hérite de navigation.yml",
          "[app][disposition][navigation]") {
  const TempDirectory root;
  WriteLegacyNavigation(root);
  MinimapOnlyPort port(root.path());
  // Avant tout profil, c'est déjà elle.
  CHECK(port.slot.minimap().settings.side_px == kLegacySide);

  port.disk.Put(kDefaultProfileName, ro::ui::LayoutProfile{});
  port.disk.Put("Farm", WithMinimap(kFarmSide, true));
  port.disk.Bind("Stingor", "Farm");
  moonclient::ProfileKeeper<ro::ui::LayoutProfile> keeper(port, &Text);
  keeper.Start(true);
  keeper.EnterCharacter("Stingor");
  CHECK(port.slot.minimap().settings.side_px == kFarmSide);

  // Revenir au profil sans minimap rend celle de navigation.yml, pas l'usine.
  keeper.EnterCharacter("Autre");
  CHECK(port.slot.minimap().settings.side_px == kLegacySide);
  CHECK_FALSE(port.slot.minimap().shown);
}

TEST_CASE("Minimap au profil : « Réinitialiser » rend les défauts d'usine",
          "[app][disposition][navigation]") {
  const TempDirectory root;
  WriteLegacyNavigation(root);
  MinimapOnlyPort port(root.path());
  port.disk.Put(kDefaultProfileName, WithMinimap(kFarmSide, false));
  moonclient::ProfileKeeper<ro::ui::LayoutProfile> keeper(port, &Text);
  keeper.Start(true);

  keeper.Ask(ro::ui::ProfilesPageAction{.gesture = ro::ui::ProfileGesture::kReset,
                                        .target = std::string(kDefaultProfileName)});
  keeper.Tick(1);
  CHECK(port.slot.minimap() == ro::ui::MinimapLayout{});
  const ro::ui::LayoutProfile* stored = Stored(port, kDefaultProfileName);
  REQUIRE(stored != nullptr);
  CHECK(stored->minimap == ro::ui::MinimapLayout{});
}

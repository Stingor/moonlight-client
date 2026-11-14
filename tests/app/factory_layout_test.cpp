// LA DISPOSITION D'USINE : ce que le joueur voit au premier lancement — les
// jauges, le portrait de statut et ses étiquettes, les icônes de menu —, valeur
// par valeur, sans chevauchement, et ce que « Rétablir » et « Réinitialiser »
// reposent.

#include "options/factory_layout.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/hud_frame.h"
#include "ro/ui/menu_icons.h"
#include "ro/ui/stat_bars.h"
#include "ro/ui/status_portrait.h"
#include "ro/ui/window_places.h"

#include "game/settings_file.h"
#include "options/status_portrait_options.h"
#include "support/temp_directory.h"

using moonclient::FactoryMenuIcons;
using moonclient::FactoryStatBars;
using moonclient::FactoryStatusPortrait;
using moonclient::PortraitRow;
using moonclient::PortraitRowId;
using moonclient::StatusPortraitOptions;
using moonclient::StatusPortraitSetup;
using ro::ui::AnchorSide;
using ro::ui::HudRect;
using ro::ui::MenuCommand;
using ro::ui::PortraitGroup;
using ro::ui::PortraitPart;
using ro::ui::StatBar;

namespace {

// Un écran ordinaire, pour ramener les places ancrées en pixels.
const ImVec2 kScreen(1280.0F, 720.0F);

[[nodiscard]] bool Overlap(const HudRect& one, const HudRect& other) {
  return one.x < other.x + other.width && other.x < one.x + one.width &&
         one.y < other.y + other.height && other.y < one.y + one.height;
}

[[nodiscard]] bool SameRect(const HudRect& one, const HudRect& other) {
  return one.x == other.x && one.y == other.y && one.width == other.width &&
         one.height == other.height && one.across == other.across && one.down == other.down;
}

struct GaugeRow {
  StatBar bar;
  HudRect rect;
};

struct PortraitRect {
  PortraitPart part;
  HudRect rect;
};

struct IconPlace {
  MenuCommand command;
  float x;
  float y;
};

constexpr int kCenter = static_cast<int>(AnchorSide::kCenter);
constexpr int kEnd = static_cast<int>(AnchorSide::kEnd);

// LA TABLE DU PROFIL « Default » du joueur.
const std::vector<GaugeRow> kGauges = {
    {StatBar::kHealth, {.x = 108.0F, .y = 27.0F, .width = 224.0F, .height = 16.0F}},
    {StatBar::kMagic, {.x = 108.0F, .y = 47.0F, .width = 224.0F, .height = 16.0F}},
    {StatBar::kBaseExp, {.x = 108.0F, .y = 67.0F, .width = 224.0F, .height = 8.0F}},
    {StatBar::kJobExp, {.x = 108.0F, .y = 79.0F, .width = 224.0F, .height = 8.0F}},
    {StatBar::kWeight, {.x = 108.0F, .y = 91.0F, .width = 224.0F, .height = 16.0F}},
    {StatBar::kZeny, {.x = 108.0F, .y = 111.0F, .width = 224.0F, .height = 16.0F}},
    {StatBar::kCast,
     {.x = 0.0F, .y = 210.0F, .width = 192.0F, .height = 24.0F, .across = kCenter, .down = kEnd}},
};

const std::vector<PortraitRect> kPortraitRects = {
    {PortraitPart::kHead, {.x = 4.0F, .y = 3.0F, .width = 100.0F, .height = 100.0F}},
    {PortraitPart::kName, {.x = 108.0F, .y = 3.0F, .width = 112.0F, .height = 20.0F}},
    {PortraitPart::kClass, {.x = 224.0F, .y = 3.0F, .width = 108.0F, .height = 20.0F}},
    {PortraitPart::kLevel, {.x = 4.0F, .y = 103.0F, .width = 100.0F, .height = 24.0F}},
};

const std::vector<IconPlace> kIcons = {
    {MenuCommand::kStatus, 4.0F, 135.0F},        {MenuCommand::kParty, 36.0F, 136.0F},
    {MenuCommand::kItem, 68.0F, 135.0F},         {MenuCommand::kSkill, 100.0F, 135.0F},
    {MenuCommand::kBooking, 132.0F, 135.0F},     {MenuCommand::kEquipment, 4.0F, 170.0F},
    {MenuCommand::kGuild, 36.0F, 169.0F},        {MenuCommand::kBattleground, 68.0F, 169.0F},
    {MenuCommand::kQuest, 100.0F, 169.0F},       {MenuCommand::kWorldMap, 132.0F, 169.0F},
    {MenuCommand::kNavigation, 4.0F, 203.0F},    {MenuCommand::kOption, 36.0F, 203.0F},
    {MenuCommand::kBank, 68.0F, 203.0F},         {MenuCommand::kMail, 100.0F, 203.0F},
    {MenuCommand::kAchievement, 132.0F, 203.0F}, {MenuCommand::kCashShop, 4.0F, 235.0F},
    {MenuCommand::kShortcut, 36.0F, 237.0F},     {MenuCommand::kCardAlbum, 68.0F, 237.0F},
    {MenuCommand::kCraftAtlas, 100.0F, 237.0F},  {MenuCommand::kMvpTracker, 132.0F, 237.0F},
    {MenuCommand::kRankingBoard, 4.0F, 271.0F},  {MenuCommand::kPetWindow, 36.0F, 271.0F},
    {MenuCommand::kMarket, 68.0F, 271.0F},       {MenuCommand::kDatabase, 100.0F, 271.0F},
};

// LES MINI-JEUX : une place à la suite, mais masqués.
const std::vector<IconPlace> kHiddenIcons = {
    {MenuCommand::kRojeweled, 132.0F, 271.0F},
    {MenuCommand::kRoggle, 4.0F, 305.0F},
    {MenuCommand::kSurvivors, 36.0F, 305.0F},
};

}  // namespace

TEST_CASE("Usine : les jauges valent le profil du joueur", "[app][factory]") {
  const ro::ui::StatBarsLook look = FactoryStatBars();
  CHECK(look.shown);
  CHECK(look.locked);
  CHECK(look.sticky);
  CHECK(look.keep_maxed_exp);
  CHECK(look.warn_overweight);
  CHECK_FALSE(look.center_mark);
  CHECK(look.zeny_reference == 1'000'000);
  for (const GaugeRow& row : kGauges) {
    const ro::ui::StatBarLook& bar = look.bars[static_cast<std::size_t>(row.bar)];
    INFO("jauge " << static_cast<int>(row.bar));
    CHECK(bar.shown);
    CHECK(SameRect(bar.rect, row.rect));
    CHECK(bar.text == (row.bar == StatBar::kWeight ? ro::ui::StatBarText::kBoth
                                                   : ro::ui::StatBarText::kPercent));
  }
  // Les couleurs restent celles d'origine.
  const auto origin = ro::ui::DefaultStatBars();
  for (std::size_t rank = 0; rank < origin.size(); ++rank) {
    CHECK(look.bars[rank].fill == origin[rank].fill);
  }
}

TEST_CASE("Usine : le portrait vaut le profil du joueur", "[app][factory]") {
  const StatusPortraitSetup setup = FactoryStatusPortrait();
  // Le portrait et les étiquettes montrés, aimantés, sans bordure ; figés par
  // le verrou des jauges, livré fermé.
  for (const ro::ui::PortraitGroupLook& group : setup.look.groups) {
    CHECK(group.shown);
    CHECK(group.sticky);
    CHECK_FALSE(group.border);
  }
  CHECK(FactoryStatBars().locked);
  CHECK(ro::ui::LevelFormatOf(setup.look.level_format) == ro::ui::LevelFormat::kSlashed);
  for (const PortraitRect& row : kPortraitRects) {
    INFO("cadre " << static_cast<int>(row.part));
    CHECK(SameRect(setup.look.parts[static_cast<std::size_t>(row.part)].rect, row.rect));
    CHECK(setup.look.parts[static_cast<std::size_t>(row.part)].shown);
  }
  // Assis, de face, animé, le personnage entier avec sa cape.
  CHECK_FALSE(setup.framing.head_only);
  CHECK(setup.framing.garment);
  CHECK(setup.framing.zoom == 0.8F);
  CHECK(setup.framing.offset_x == 0.0F);
  CHECK(setup.framing.offset_y == 0.21F);
  CHECK(setup.framing.animation == 2);
  CHECK(setup.framing.direction == 0);
  CHECK(setup.framing.animated);
}

TEST_CASE("Usine : chaque icône montrée a sa place, celle du profil", "[app][factory]") {
  const ro::ui::MenuIconsLook look = FactoryMenuIcons();
  CHECK(look.locked);
  CHECK(look.sticky);
  // 🔴 UNE ICÔNE MONTRÉE SANS PLACE D'USINE tomberait dans la grille calculée,
  // par-dessus le portrait et les jauges.
  for (const ro::ui::MenuIconEntry& entry : ro::ui::MenuIconCatalogue()) {
    if (entry.shown) {
      INFO("icône " << entry.stem);
      CHECK(ro::ui::MenuIconPlaced(look.places[static_cast<std::size_t>(entry.command)]));
    }
  }
  for (const IconPlace& row : kIcons) {
    const ro::ui::MenuIconPlace& place = look.places[static_cast<std::size_t>(row.command)];
    INFO("icône " << ro::ui::MenuIconOf(row.command).stem);
    CHECK(place.x == row.x);
    CHECK(place.y == row.y);
    CHECK(place.across == 0);
    CHECK(place.down == 0);
    CHECK_FALSE(place.hidden);
  }
  for (const IconPlace& row : kHiddenIcons) {
    const ro::ui::MenuIconPlace& place = look.places[static_cast<std::size_t>(row.command)];
    INFO("icône " << ro::ui::MenuIconOf(row.command).stem);
    CHECK(place.x == row.x);
    CHECK(place.y == row.y);
    CHECK(place.hidden);
  }
}

TEST_CASE("Usine : jauges, portrait et icônes ne se chevauchent pas", "[app][factory]") {
  std::vector<HudRect> on_screen;
  const ro::ui::StatBarsLook gauges = FactoryStatBars();
  for (const ro::ui::StatBarLook& bar : gauges.bars) {
    on_screen.push_back(ro::ui::HudOnScreen(bar.rect, kScreen));
  }
  for (const ro::ui::PortraitPartLook& part : FactoryStatusPortrait().look.parts) {
    on_screen.push_back(ro::ui::HudOnScreen(part.rect, kScreen));
  }
  // Une icône occupe au plus le carré de repli : c'est la taille du bitmap.
  // ⚠ LES MASQUÉES AUSSI : le joueur qui les allume ne doit rien recouvrir.
  std::vector<IconPlace> every_icon = kIcons;
  every_icon.insert(every_icon.end(), kHiddenIcons.begin(), kHiddenIcons.end());
  const ro::ui::MenuIconsLook icons = FactoryMenuIcons();
  for (const IconPlace& row : every_icon) {
    const ro::ui::MenuIconPlace& place = icons.places[static_cast<std::size_t>(row.command)];
    on_screen.push_back(ro::ui::HudOnScreen(HudRect{.x = place.x,
                                                    .y = place.y,
                                                    .width = ro::ui::kMenuIconFallbackSide,
                                                    .height = ro::ui::kMenuIconFallbackSide},
                                            kScreen));
  }
  for (std::size_t one = 0; one < on_screen.size(); ++one) {
    for (std::size_t other = one + 1; other < on_screen.size(); ++other) {
      INFO("cadres " << one << " et " << other);
      CHECK_FALSE(Overlap(on_screen[one], on_screen[other]));
    }
  }
}

TEST_CASE("Usine : le portrait naît, se relit et se rétablit dans sa disposition",
          "[app][factory]") {
  const ro::test_support::TempDirectory folder;
  const StatusPortraitSetup factory = FactoryStatusPortrait();
  // Un premier lancement : le fichier n'a aucune clé.
  StatusPortraitOptions page(factory);
  moonclient::SettingsFile file(folder.path() / "settings.yml");
  page.Declare(file);
  file.Load();
  CHECK(page.look().group(PortraitGroup::kHead).shown);
  CHECK(page.look().group(PortraitGroup::kLabels).shown);
  CHECK(page.framing().zoom == factory.framing.zoom);
  CHECK(page.framing().animation == factory.framing.animation);

  // Chaque « Réinitialiser la disposition » repose les places livrées de son
  // groupe, et de lui seul.
  const auto factory_rect = [&factory](PortraitPart part) {
    return factory.look.parts[static_cast<std::size_t>(part)].rect;
  };
  auto& head = page.look().parts[static_cast<std::size_t>(PortraitPart::kHead)];
  auto& name = page.look().parts[static_cast<std::size_t>(PortraitPart::kName)];
  head.rect.x = 300.0F;
  name.rect.x = 500.0F;
  ro::ui::OptionsWindowAction reset;
  reset.changed = PortraitRowId(PortraitRow::kLabelsResetLayout);
  REQUIRE(page.Change(reset));
  CHECK(SameRect(name.rect, factory_rect(PortraitPart::kName)));
  CHECK(head.rect.x == 300.0F);
  reset.changed = PortraitRowId(PortraitRow::kResetLayout);
  REQUIRE(page.Change(reset));
  CHECK(SameRect(head.rect, factory_rect(PortraitPart::kHead)));

  // « Rétablir » repose la disposition livrée, pas les défauts d'origine, et
  // les deux groupes.
  ro::ui::OptionsWindowAction zoom;
  zoom.changed = PortraitRowId(PortraitRow::kZoom);
  zoom.value = 1.7F;
  REQUIRE(page.Change(zoom));
  for (ro::ui::PortraitGroupLook& group : page.look().groups) {
    group.shown = false;
    group.sticky = false;
    group.border = true;
  }
  name.rect.y = 400.0F;
  file.RestoreDefaults();
  CHECK(page.framing().zoom == factory.framing.zoom);
  for (const ro::ui::PortraitGroupLook& group : page.look().groups) {
    CHECK(group.shown);
    CHECK(group.sticky);
    CHECK_FALSE(group.border);
  }
  CHECK(name.rect.y == factory_rect(PortraitPart::kName).y);
}

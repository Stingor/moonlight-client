// LES BASCULES « / » QUI COCHENT UNE CASE DES OPTIONS : le mot, la case qu'il
// coche, l'écho qu'il écrit, et l'aller-retour.

#include "chat/setting_toggle_command.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/entity_bars.h"

#include "options/game_option_ids.h"
#include "options/graphics_options.h"

using moonclient::GameOption;
using moonclient::ReadSettingToggle;
using moonclient::SettingToggle;
using moonclient::SettingToggleChanges;
using moonclient::SettingToggleEcho;

namespace {

// La seule case qu'une bascule coche, quand elle n'en coche qu'une.
[[nodiscard]] int OnlyRow(SettingToggle toggle, bool on) {
  const std::vector<ro::ui::OptionsWindowAction> changes = SettingToggleChanges(toggle, on);
  REQUIRE(changes.size() == 1);
  CHECK(changes.front().on == on);
  return changes.front().changed;
}

[[nodiscard]] int EntityBarShown(ro::ui::EntityBarKind kind) {
  return moonclient::EntityBarRow(static_cast<std::size_t>(kind),
                                  moonclient::EntityBarField::kShown);
}

constexpr SettingToggle kAllToggles[] = {
    SettingToggle::kMonsterHp, SettingToggle::kGuildEmblem,    SettingToggle::kWindowSnap,
    SettingToggle::kMissShown, SettingToggle::kSkillFailShown, SettingToggle::kScreenQuake,
};

}  // namespace

TEST_CASE("Bascules : chaque mot et ses doubles, sans la casse, argument ignoré",
          "[app][chat][setting_toggle]") {
  const auto read = [](std::string_view line) { return ReadSettingToggle(line); };
  CHECK(read("/monsterhp") == SettingToggle::kMonsterHp);
  CHECK(read("/MonsterHP") == SettingToggle::kMonsterHp);
  CHECK(read("/emblem") == SettingToggle::kGuildEmblem);
  CHECK(read("/dpaqmffpa") == SettingToggle::kGuildEmblem);
  CHECK(read("/doaqmffpa") == SettingToggle::kGuildEmblem);
  CHECK(read("/window") == SettingToggle::kWindowSnap);
  CHECK(read("/wi") == SettingToggle::kWindowSnap);
  CHECK(read("/miss") == SettingToggle::kMissShown);
  CHECK(read("/skillfail") == SettingToggle::kSkillFailShown);
  CHECK(read("/sf") == SettingToggle::kSkillFailShown);
  CHECK(read("/quake") == SettingToggle::kScreenQuake);
  // Ce qui suit le mot ne change rien : « /quake off » bascule aussi.
  CHECK(read("/quake off") == SettingToggle::kScreenQuake);
  CHECK(read("  /emblem on") == SettingToggle::kGuildEmblem);
  // Un autre mot n'est pas une bascule.
  CHECK_FALSE(read("/quakes").has_value());
  CHECK_FALSE(read("/win").has_value());
  CHECK_FALSE(read("quake").has_value());
  CHECK_FALSE(read("").has_value());
}

TEST_CASE("Bascules : chacune coche la case de son réglage", "[app][chat][setting_toggle]") {
  CHECK(OnlyRow(SettingToggle::kGuildEmblem, true) ==
        static_cast<int>(GameOption::kEntityNamesEmblem));
  CHECK(OnlyRow(SettingToggle::kWindowSnap, false) ==
        static_cast<int>(GameOption::kAlignGridMagnet));
  CHECK(OnlyRow(SettingToggle::kMissShown, true) == static_cast<int>(GameOption::kDamageMiss));
  CHECK(OnlyRow(SettingToggle::kSkillFailShown, false) ==
        static_cast<int>(GameOption::kSkillFailShown));
  CHECK(OnlyRow(SettingToggle::kScreenQuake, false) ==
        moonclient::GraphicsRowId(moonclient::GraphicsRow::kScreenQuake));

  // `/monsterhp` : les monstres ET les boss, à la même valeur.
  const std::vector<ro::ui::OptionsWindowAction> monsters =
      SettingToggleChanges(SettingToggle::kMonsterHp, false);
  REQUIRE(monsters.size() == 2);
  CHECK(monsters[0].changed == EntityBarShown(ro::ui::EntityBarKind::kMonster));
  CHECK(monsters[1].changed == EntityBarShown(ro::ui::EntityBarKind::kBoss));
  CHECK_FALSE(monsters[0].on);
  CHECK_FALSE(monsters[1].on);
}

TEST_CASE("Bascules : l'écho du client du joueur, un par sens", "[app][chat][setting_toggle]") {
  CHECK(std::string_view(SettingToggleEcho(SettingToggle::kMonsterHp, true)) ==
        "La barre de vie d'un monstre attaqué s'affiche.");
  CHECK(std::string_view(SettingToggleEcho(SettingToggle::kWindowSnap, false)) ==
        "Aimantation des fenêtres désactivée. [/wi OFF]");
  CHECK(std::string_view(SettingToggleEcho(SettingToggle::kScreenQuake, true)) ==
        "Tremblements d'écran activés");
  CHECK(std::string_view(SettingToggleEcho(SettingToggle::kScreenQuake, false)) ==
        "Tremblements d'écran désactivés");
  for (const SettingToggle toggle : kAllToggles) {
    const std::string_view on = SettingToggleEcho(toggle, true);
    const std::string_view off = SettingToggleEcho(toggle, false);
    CHECK_FALSE(on.empty());
    CHECK_FALSE(off.empty());
    CHECK(on != off);
  }
}

TEST_CASE("Bascules : /quake passe par la case de Graphics, aller et retour",
          "[app][chat][setting_toggle][quake]") {
  moonclient::GraphicsOptions graphics;
  // Allumés d'usine, comme chez le client du joueur.
  REQUIRE(graphics.screen_quake());
  for (const bool on : {false, true}) {
    const std::vector<ro::ui::OptionsWindowAction> changes =
        SettingToggleChanges(SettingToggle::kScreenQuake, !graphics.screen_quake());
    REQUIRE(changes.size() == 1);
    CHECK(graphics.Change(changes.front()));
    CHECK(graphics.screen_quake() == on);
  }
}

TEST_CASE("Bascules : la case de Graphics porte la commande",
          "[app][chat][setting_toggle][quake]") {
  moonclient::GraphicsOptions graphics;
  graphics.Compose();
  const int row_id = moonclient::GraphicsRowId(moonclient::GraphicsRow::kScreenQuake);
  bool found = false;
  for (const ro::ui::OptionRowView& row : graphics.rows()) {
    if (row.id != row_id) {
      continue;
    }
    found = true;
    CHECK(row.on);
    REQUIRE(row.command != nullptr);
    CHECK(std::string_view(row.command) == "/quake");
  }
  CHECK(found);
}

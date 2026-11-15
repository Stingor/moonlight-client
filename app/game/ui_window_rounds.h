#pragma once

// LES ALLER-RETOURS QUE `UiWindows` TIENT À PART : sa vue descend d'`app`, sa
// réponse remonte à l'image suivante. Voir, dans `game/ui_windows.h`, ce que
// chacun porte et pourquoi.
//
// ⚠ À PART DE `game/ui_windows.h`, et c'est la raison d'être de ce fichier :
// ils portent leurs fenêtres par valeur. Seules les sources qui les lisent
// l'incluent.

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "ro/ui/chat_macros.h"
#include "ro/ui/craft_atlas_window.h"
#include "ro/ui/entity_menu.h"
#include "ro/ui/menu_icons.h"
#include "ro/ui/minimap_settings.h"
#include "ro/ui/minimap_window.h"
#include "ro/ui/monster_sheet_window.h"
#include "ro/ui/view_equip_window.h"

#include "game/database_windows.h"
#include "game/ui_windows.h"

namespace moonclient {

struct UiWindows::EntityMenu {
  ro::ui::EntityMenuState state;
  const ro::ui::EntityMenuView* view = nullptr;
  ro::ui::EntityMenuAnswer answer;
};

struct UiWindows::ViewEquip {
  ro::ui::ViewEquipWindowState state;
  const ro::ui::ViewEquipView* view = nullptr;
  ro::ui::ViewEquipAnswer answer;
};

struct UiWindows::MonsterSheet {
  ro::ui::MonsterSheetWindowState state;
  const ro::ui::MonsterSheetView* view = nullptr;
  ro::ui::MonsterSheetAnswer answer;
  const ro::ui::MonsterSheetPreview* preview = nullptr;  // un lien survolé, ou nul
  ro::ui::MonsterSheetPreviewState preview_state;
  DatabaseWindows database;
};

struct UiWindows::MenuIcons {
  ro::ui::MenuIconsState state;
  std::vector<ro::ui::MenuIconView> icons;
  std::optional<ro::ui::MenuCommand> clicked;
  // La place de chaque icône, tenue par les réglages. Nulle, rien ne se dessine.
  ro::ui::MenuIconsLook* look = nullptr;
};

struct UiWindows::CraftAtlas {
  ro::ui::CraftAtlasWindowState state;
  const ro::ui::CraftAtlasView* view = nullptr;
  ro::ui::CraftAtlasAnswer answer;
  // L'aperçu d'un lien de recette survolé : nul quand rien ne l'est.
  const ro::ui::CraftRecipePreview* preview = nullptr;
};

struct UiWindows::Minimap {
  ro::ui::MinimapSettings* settings = nullptr;
  ro::ui::MinimapWindowState state;
  const ro::ui::MinimapView* view = nullptr;
  ro::ui::MinimapAnswer answer;
};

// Les dix libellés de touche des macros : un tableau, dont le nombre est celui
// des macros.
struct UiWindows::MacroKeyLabels : std::array<std::string, ro::ui::kChatMacroCount> {};

}  // namespace moonclient

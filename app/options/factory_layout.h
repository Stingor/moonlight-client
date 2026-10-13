#pragma once

#include "ro/render/entity_names.h"
#include "ro/ui/align_grid.h"
#include "ro/ui/chat_window.h"
#include "ro/ui/entity_bars.h"
#include "ro/ui/item_obtain_toast.h"
#include "ro/ui/menu_icons.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/stat_bars.h"
#include "ro/ui/status_bar.h"

#include "options/status_portrait_options.h"

namespace moonclient {

// LA DISPOSITION D'USINE DE MOONCLIENT : ce que voit un joueur dont le fichier
// de réglages n'a pas encore la clé, et ce que « Rétablir » repose.
//
// ⚠ LES STRUCTURES DE `ui` ET DE `render` GARDENT LES DÉFAUTS DU CLIENT
// D'ORIGINE, et c'est voulu : ils décrivent ce que le client fait, et leurs
// essais s'y tiennent. La disposition que moonclient livre est un choix du
// joueur, posé par-dessus, ici seulement.
[[nodiscard]] ro::ui::StatBarsLook FactoryStatBars();
// Le portrait de statut : son cadrage et ses quatre cadres.
[[nodiscard]] StatusPortraitSetup FactoryStatusPortrait();
// LES ICÔNES DE MENU : un bloc de cinq colonnes au coin haut-gauche, sous le
// portrait de statut, et le bouton du Vote Shop ancré à droite. Une icône
// absente de la table garde sa case de la grille du jeu.
[[nodiscard]] ro::ui::MenuIconsLook FactoryMenuIcons();
[[nodiscard]] ro::ui::StatusBarLayout FactoryStatusBar();
[[nodiscard]] ro::ui::ShortcutBarsLayout FactoryShortcutBars();
[[nodiscard]] ro::ui::EntityBarsLook FactoryEntityBars();
[[nodiscard]] ro::render::EntityNamesLook FactoryEntityNames();
[[nodiscard]] ro::ui::AlignGrid FactoryAlignGrid();
[[nodiscard]] ro::ui::ItemObtainToastLook FactoryItemObtain();
// La conversation en battle mode : la barre de saisie se replie, et le clavier
// revient au jeu.
[[nodiscard]] ro::ui::ChatAppearance FactoryChatAppearance();

}  // namespace moonclient

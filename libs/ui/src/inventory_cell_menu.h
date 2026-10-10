#pragma once

#include "ro/ui/inventory_window.h"

// En-tête PRIVÉ : seule la fenêtre du sac ouvre le menu de ses cases.
namespace ro::ui {

// LE MENU D'UNE CASE, et ce que le joueur y a choisi.
//
// ⚠ IL NE PROPOSE QUE CE QUE L'OBJET PERMET. Une entrée grisée dirait « plus
// tard » ; une entrée absente dit « pas cet objet », ce qui est la vérité.
//
// ⚠ ET IL LUI FAUT RETROUVER SON STYLE. La grille tourne en marge nulle et
// espacement jointif — c'est ce qui colle ses cases — et un menu ouvert dans
// cette portée en hériterait : ses entrées se toucheraient, sans marge. On lui
// rend les deux le temps qu'il est ouvert.
void DrawInventoryCellMenu(const InventoryCellView& cell, ImVec2 menu_padding, ImVec2 menu_spacing,
                           InventoryWindowAction& action, InventoryWindowState& state,
                           const InventoryWindowView& view);

}  // namespace ro::ui

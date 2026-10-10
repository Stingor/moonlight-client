#pragma once

#include <span>

#include <imgui.h>

#include "ro/ui/inventory_window.h"
#include "ro/ui/skin_art.h"

// LE PIED DE LA FENÊTRE DU SAC : ses deux lignes et ses trois interrupteurs.
//
// 🔴 IL A QUITTÉ LA FENÊTRE PARCE QU'ELLE PASSAIT LE PLAFOND, et c'est LUI qui
// est parti parce qu'il forme un tout : le poids, la bourse, le compte et les
// trois boutons se disposent les uns par rapport aux autres, et rien d'autre du
// dessin ne les regarde. La grille, elle, ne se découpe pas — sa boucle de cases
// est un seul geste.
//
// En-tête PRIVÉ : il vit dans `src/`, parce que personne hors de cette
// bibliothèque n'a à dessiner un demi-pied.
namespace ro::ui {

// Le pied, posé entre ces deux coins. ⚠ `tab` EST L'ONGLET CHOISI À CETTE IMAGE
// et non celui que l'état porte encore : la bande dessine déjà le nouveau comme
// actif, et lire l'état ferait paraître les boutons des favoris une image après
// leur onglet.
void DrawInventoryFooter(const SkinArt& art, ItemTab tab, InventoryTally tally, ImVec2 top_left,
                         ImVec2 bottom_right, const InventoryWindowState& state,
                         InventoryWindowAction& action);

}  // namespace ro::ui

#pragma once

// Ce que les deux fichiers de la fenêtre « Base de données » se partagent : la
// fiche d'un objet, dessinée par le second.

#include "ro/ui/database_window.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/skin_art.h"

namespace ro::ui::database {

// La fiche d'un objet dans le panneau en cours. Un lien vers un monstre visite
// sa fiche dans le fil de `state`.
void DrawItemSheet(DatabaseWindowState& state, const DatabaseItemView& view, const SkinArt& art,
                   ItemLinkMenu& menu, DatabaseAnswer& answer);

}  // namespace ro::ui::database

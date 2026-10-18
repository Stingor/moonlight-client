#pragma once

#include "ro/ui/npc_shop_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// « PANIERS ▾ » de l'onglet d'achat : le bouton, son menu — enregistrer, puis
// la liste avec Charger, Renommer et Supprimer —, et sous lui la ligne grise de
// ce que le dernier chargement a laissé.
void DrawNpcShopSavedCarts(NpcShopWindowState& state, const NpcShopView& view, const SkinArt& art,
                           NpcShopWindowAction& action);

}  // namespace ro::ui

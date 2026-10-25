#pragma once

// LE CORPS COMMUN DES DEUX BOUTIQUES DE LA MÉTA : la fenêtre entière, sauf ce
// qui distingue l'Église de la Kafra — le titre, le libellé d'achat, le texte
// d'un effet et la description d'une ligne.
//
// En-tête PRIVÉ de `libs/ui/src`.

#include "ro/ui/skin_art.h"
#include "ro/ui/survivors_shop.h"

#include "survivors_meta_text.h"

namespace ro::ui {

struct SurvivorsShopLook {
  const char* window_name = "";  // le nom complet, déjà traduit (`WindowName`)
  const char* buy_label = "";    // déjà traduit
  float first_width = 0.0F;
  float first_height = 0.0F;
  SurvivorsEffectText (*effect)(const SurvivorsShopRow& row, int total) = nullptr;
  const char* (*describe)(const SurvivorsShopRow& row) = nullptr;
};

[[nodiscard]] SurvivorsShopAsk DrawSurvivorsShop(SurvivorsShopState& state,
                                                 const SurvivorsShopView& view,
                                                 const SurvivorsShopLook& look, const SkinArt& art);

}  // namespace ro::ui

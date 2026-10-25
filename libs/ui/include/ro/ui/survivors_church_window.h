#pragma once

#include "ro/ui/skin_art.h"
#include "ro/ui/survivors_shop.h"

namespace ro::ui {

// L'ÉGLISE DE PRONTERA (U21, Q2) : les onze Bénédictions, une ligne chacune —
// icône du skill RO, nom, pastilles de rang, effet actuel puis au rang
// suivant, prix, « Bénir » grisé avec sa raison —, et en pied « Tout
// rembourser », qui rend tout ce que l'Église a reçu.
//
// Une ligne porte `key` (la clé de la Bénédiction), `effect_now` et
// `effect_next` (les TOTAUX du catalogue à ces rangs) ; la fenêtre en écrit
// l'effet chiffré.

inline constexpr const char* kSurvivorsChurchId = "survivors_church";

[[nodiscard]] SurvivorsShopAsk DrawSurvivorsChurchWindow(SurvivorsShopState& state,
                                                         const SurvivorsShopView& view,
                                                         const SkinArt& art);

}  // namespace ro::ui

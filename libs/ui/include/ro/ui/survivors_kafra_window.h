#pragma once

#include "ro/ui/skin_art.h"
#include "ro/ui/survivors_shop.h"

namespace ro::ui {

// LA KAFRA (U21, Q3) : trois services à rangs permanents et remboursables —
// Magnifier relance, Fly Wing passe, Butterfly Wing bannit —, le rang r
// donnant r charges à chaque partie libre. Une ligne : icône de l'objet RO,
// nom, pastilles, charges actuelles puis au rang suivant, prix, « Acheter ».
//
// Une ligne porte `key` (la clé du service), qui choisit sa description, et
// `effect_now` et `effect_next` (les charges à ces rangs).

inline constexpr const char* kSurvivorsKafraId = "survivors_kafra";

[[nodiscard]] SurvivorsShopAsk DrawSurvivorsKafraWindow(SurvivorsShopState& state,
                                                        const SurvivorsShopView& view,
                                                        const SkinArt& art);

}  // namespace ro::ui

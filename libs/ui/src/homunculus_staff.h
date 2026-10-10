#pragma once

// LA SECTION « STAFF » DE L'ONGLET HOMONCULE. Interne à `homunculus_tab.cpp`.

#include "ro/ui/homunculus_tab.h"

namespace ro::ui {

// La section repliable, seulement sous le droit du staff. Ce qu'elle demande
// s'écrit dans `action` en verbe `kStaff`.
void DrawHomunculusStaff(HomunculusTabState& state, const HomunculusTabView& view,
                         const SkinArt& art, HomunculusTabAction& action);

}  // namespace ro::ui

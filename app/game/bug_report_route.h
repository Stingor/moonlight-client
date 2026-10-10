#pragma once

#include "ro/net/connection.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/description_window.h"

#include "game/ui_stage.h"
#include "items/description_view.h"

namespace moonclient {

// LES DEMANDES DE RAPPORT DE BUG DE L'IMAGE : chaque fenêtre qui a cliqué son
// bouton reçoit la boîte avec SON contexte, et la boîte qui a validé part au
// serveur. Voir l'ADR 0052.
//
// ⚠ POSE AUSSI LE BOUTON des fenêtres pour l'image suivante, selon le réglage.
void RouteBugReports(const UiStage::Answers& answers, UiStage::Windows& windows,
                     const ro::sim::World& world, const DescriptionView& item,
                     const ro::ui::SkillDescriptionView& skill, bool buttons,
                     ro::net::Connection& link);

}  // namespace moonclient

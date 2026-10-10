#pragma once

// LES DESTINATIONS D'UNE COMPÉTENCE DE DÉPLACEMENT, DANS LA BOUCLE DE LA
// PARTIE : ce que la fenêtre montre de la question du monde, et ce que sa
// réponse devient sur le fil.

#include "ro/net/connection.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/warp_list_window.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/map_display_names.h"

#include "game/ui_stage.h"

namespace moonclient {

// Recopie la question du monde dans l'état et la vue de la fenêtre.
//
// ⚠ UN CHANGEMENT DE CARTE EMPORTE LA QUESTION, sans un mot au serveur : c'est
// ici qu'on le constate, avant de dessiner.
void StageWarpList(ro::sim::World& world, const ro::vfs::ItemWords& words,
                   const ro::vfs::MapDisplayNames* map_names, UiStage::Windows& windows);

// Envoie la destination choisie, ou l'annulation, et ferme la question.
void ApplyWarpList(const ro::ui::WarpListWindowAction& asked, ro::net::Connection& link,
                   ro::sim::World& world);

}  // namespace moonclient

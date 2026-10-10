#pragma once

#include "ro/ui/party_friend_window.h"
#include "ro/ui/party_grid.h"

#include "game/adoption_stage.h"

namespace moonclient {

// LA FENÊTRE GROUPE / AMIS ET LA GRILLE DE GROUPE, telles que `UiStage::Windows`
// les porte : ce que chacune garde d'une image à l'autre, la vue que la boucle
// compose, et ce que le joueur a demandé.
//
// ⚠ LA FENÊTRE SE DESSINE À CHAQUE IMAGE, MÊME FERMÉE : la demande reçue et
// l'annonce des règles du groupe vivent hors d'elle. Ses deux modales aussi :
// la dissolution, qui s'ouvre quand personne ne peut reprendre la direction,
// et l'adoption qu'un couple nous demande.
struct PartyWindows {
  ro::ui::PartyFriendWindowState window;
  const ro::ui::PartyFriendView* view = nullptr;
  const ro::ui::PartyFriendLook* look = nullptr;
  ro::ui::PartyFriendAction answer;
  const ro::ui::PartyGridView* grid_view = nullptr;
  ro::ui::PartyGridLook* grid_look = nullptr;
  ro::ui::PartyGridState grid_state;
  ro::ui::PartyGridAnswer grid_answer;
  bool ask_disband = false;
  bool disband_confirmed = false;
  AdoptionBox adoption;
};

}  // namespace moonclient

#pragma once

#include "ro/net/connection.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/npc_dialog_window.h"

namespace moonclient {

// CE QUE LE JOUEUR A DEMANDÉ AU PNJ part au serveur.
//
// ⚠ C'EST LE SEUL ENDROIT OÙ UNE FENÊTRE AGIT SUR LE MONDE, et elle n'y touche
// pas elle-même : `ui` rend une INTENTION, et cette fonction la route. Le jour
// où une deuxième fenêtre émettra, elle passera par une fonction jumelle — pas
// par un accès direct au monde depuis un panneau.
//
// Un refus se dit au journal et n'interrompt rien : un rang de menu hors
// bornes est refusé AVANT de partir, ce qui vaut mieux que l'éjection que le
// serveur réserve à un mauvais rang.
void SendNpcAnswer(const ro::ui::NpcDialogAnswer& answer, ro::net::Connection& link,
                   ro::sim::World& world);

}  // namespace moonclient

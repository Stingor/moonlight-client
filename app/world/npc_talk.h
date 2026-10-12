#pragma once

#include <cstdint>

#include "ro/net/connection.h"
#include "ro/sim/banned_words.h"
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
//
// 🔴 UN TEXTE QUI PORTE UN MOT DE `banned` NE PART PAS : le refus s'écrit dans la
// conversation, et le PNJ attend toujours sa réponse.
void SendNpcAnswer(const ro::ui::NpcDialogAnswer& answer, ro::net::Connection& link,
                   ro::sim::World& world, const ro::sim::BannedWords* banned, std::uint32_t now_ms);

}  // namespace moonclient

#pragma once

// LA FENÊTRE DES JOUEURS BLOQUÉS, DANS LA PARTIE : sa vue lue dans les
// chuchotements et la voix, et ses gestes rendus aux deux serveurs. Ce qu'elle
// montre se compose dans `chat/block_list_view.h`.

#include "ro/net/connection.h"
#include "ro/ui/block_list_window.h"

namespace moonclient {

class VoiceMode;
class WhisperStage;

[[nodiscard]] ro::ui::BlockListView ComposeBlockList(const WhisperStage& whispers,
                                                     const VoiceMode& voice, bool connected);

// LE GESTE QUE LA FENÊTRE A RENDU. Les chuchotements partent au serveur de
// carte, la voix au serveur voix ; hors ligne, rien ne part. Une liste ne
// change qu'à la réponse : la vue suivante la montre.
//
// ⚠ « DÉBLOQUER » NE DEMANDE AU SERVEUR DE CARTE QUE CE QU'IL BLOQUE : un nom
// qu'il ne tient pas lui ferait répondre un refus, dit dans la conversation.
void FollowBlockListAnswer(const ro::ui::BlockListAnswer& answer, ro::net::Connection& link,
                           WhisperStage& whispers, VoiceMode& voice);

}  // namespace moonclient

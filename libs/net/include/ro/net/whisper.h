#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "ro/net/chat_target.h"
#include "ro/net/world_update.h"
#include "ro/sim/friend_list.h"
#include "ro/sim/guild.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/whisper.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// LES CHUCHOTEMENTS : celui qu'on reçoit (`ZC_WHISPER`), et l'accusé de celui
// qu'on a envoyé (`ZC_ACK_WHISPER`). Voir `ro/sim/whisper.h`.
//
// 🔴 L'ACCUSÉ NE PORTE NI TEXTE NI DESTINATAIRE, seulement un code. L'écho de
// ce qu'on a chuchoté se compose donc de ce que `WhisperOutbox` a gardé à
// l'envoi, appairé dans l'ordre.

// CE QUE LA LIGNE A BESOIN DE SAVOIR DE NOUS : qui sont nos amis, qui est de
// notre guilde, et ce qui attend un accusé. Chaque pointeur peut manquer — un
// rejeu sans partie n'a ni amis ni envois : l'autre passe alors pour un
// inconnu, et un accusé ne retrouve personne.
struct WhisperCircle {
  const sim::FriendList* friends = nullptr;
  const sim::GuildBook* guild = nullptr;
  sim::WhisperOutbox* outbox = nullptr;
};

// CE QUE L'AUTRE EST POUR NOUS, dans l'ordre où le client d'origine pose la
// question : l'ami d'abord, puis le staff, puis la guilde. `from_staff` vient du
// paquet reçu, et vaut faux pour l'écho d'un envoi.
[[nodiscard]] sim::WhisperRelation WhisperRelationOf(std::string_view name, bool from_staff,
                                                     const WhisperCircle& circle);

// Lit les deux paquets du chuchotement, et rend `kUnhandled` pour tout autre.
//
// ⚠ À APPELER AVANT `ApplyFrame` quand on tient le cercle : sans lui, le monde
// les lit quand même, mais l'autre y passe pour un inconnu et l'accusé d'un
// envoi ne compose aucun écho.
[[nodiscard]] FrameOutcome ApplyWhisperFrame(const Frame& frame, sim::World& world,
                                             const WhisperCircle& circle, std::uint32_t now_ms);

// NOTE UN ENVOI QUI ATTEND SON ACCUSÉ, et rend vrai s'il en attend un.
//
// 🔴 CE QUE LE SERVEUR N'ACCUSE PAS NE SE NOTE PAS : le noter décalerait
// l'appariement de tous les accusés qui suivent. Il n'accuse ni un CANAL
// (« #nom », qui passe par le même paquet), ni un message à un PNJ qui existe
// (« NPC:nom »), ni une COMMANDE (« @… », « #… »), ni un texte trop long.
//
// ⚠ UN JOUEUR MUET NE SE NOTE PAS NON PLUS : sous un état qui coupe la parole
// (`player_statuses`), le serveur jette le message sans accusé, et l'envoi
// noté aurait pris l'accusé du suivant — l'écho et le refus passaient alors à
// un autre destinataire.
//
// ⚠ CE QUI NE SE VOIT PAS D'ICI attend son oubli : un PNJ qui n'existe pas (le
// serveur accuse alors « absent »), le mutisme de bonnes manières — il ne
// porte pas d'icône —, une ligne envoyée plus vite que le délai de parole du
// serveur, et un « #canal » inexistant, que le serveur reprend comme un
// chuchotement et accuse « absent » sans qu'on l'ait noté.
bool NoteWhisperSent(sim::WhisperOutbox& outbox, const ChatIntent& intent, std::uint32_t sent_ms,
                     std::span<const sim::ActiveStatus> player_statuses = {});

}  // namespace ro::net

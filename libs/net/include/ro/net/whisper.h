#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "ro/net/chat_target.h"
#include "ro/net/world_update.h"
#include "ro/sim/chat_channel.h"
#include "ro/sim/friend_list.h"
#include "ro/sim/guild.h"
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
// ⚠ UN JOUEUR MUET NE SE NOTE PAS NON PLUS (`muted`, voir
// `sim::CannotChat`) : le serveur jette alors le message sans accusé, et
// l'envoi noté aurait pris l'accusé du suivant — l'écho et le refus passaient
// à un autre destinataire.
//
// 🔴 ET UN « #nom » N'EST UN CANAL QUE SI LE SERVEUR LE PREND POUR TEL : un
// canal qu'il n'a pas annoncé, ou où le joueur n'a pas le droit d'écrire,
// retombe en chuchotement ordinaire, accusé « absent ». Celui-là se note.
// Sans liste annoncée (`channels` vide), tout « #nom » reste un canal.
//
// ⚠ CE QUI NE SE VOIT PAS D'ICI attend son oubli : un PNJ qui n'existe pas (le
// serveur accuse alors « absent »), une ligne envoyée plus vite que le délai
// de parole du serveur, et le canal d'alliance hors guilde.
struct WhisperNoting {
  bool muted = false;
  // Les canaux que le serveur a annoncés, droits appliqués.
  std::span<const sim::ChatChannel> channels;
};
bool NoteWhisperSent(sim::WhisperOutbox& outbox, const ChatIntent& intent, std::uint32_t sent_ms,
                     const WhisperNoting& noting = {});

}  // namespace ro::net

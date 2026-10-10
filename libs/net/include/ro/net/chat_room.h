#pragma once

#include <cstdint>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/chat_room.h"
#include "ro/sim/world.h"

namespace ro::net {

// LES SALONS DE DISCUSSION : sept paquets reçus, six émis. Voir
// `ro/sim/chat_room.h`.
//
// L'enseigne d'un salon — son apparition, sa disparition — est l'affaire du
// monde. Ce traducteur lit aussi, SANS LA CONSOMMER, la disparition d'un salon
// (le nôtre se ferme avec elle) et les deux changements de carte (le serveur
// nous a sortis du salon avant de déplacer le joueur).
//
// 🔴 L'ENTRÉE LIT L'ENSEIGNE DU MONDE, puis l'efface : le paquet ne porte ni
// titre ni limite, et le client d'origine retire l'enseigne du salon où l'on
// vient d'entrer. D'où le monde en écriture.
[[nodiscard]] FrameOutcome ApplyChatRoomFrame(const Frame& frame, sim::ChatRoom& room,
                                              sim::World& world);

// CE QUI SE DIT AUTOUR, QUAND ON EST DANS UN SALON.
//
// 🔴 C'EST LE SALON QUI LE DIT : un occupant ne reçoit plus la parole de la
// carte, le serveur la lui retient. Tout ce qui arrive par ces deux paquets
// vient donc du salon — parole des membres, écho de la sienne, et ce que le
// serveur écrit au joueur seul —, et le client d'origine le range dans la
// fenêtre du salon, pas dans la conversation.
//
// Rend `kApplied` quand la trame est prise, `kUnhandled` sinon : le monde
// l'applique alors comme d'habitude. ⚠ À APPELER AVANT LUI.
[[nodiscard]] FrameOutcome ClaimChatRoomLine(const Frame& frame, sim::ChatRoom& room,
                                             const sim::World& world, std::uint32_t now_ms);

// Les couleurs des lignes du salon, en `0xRRGGBB` : celles du client d'origine,
// sombres parce que le corps de sa fenêtre est clair.
inline constexpr std::uint32_t kChatRoomSpeechColor = 0x222222;
inline constexpr std::uint32_t kChatRoomOwnSpeechColor = 0x007800;
inline constexpr std::uint32_t kChatRoomArrivalColor = 0x008080;
inline constexpr std::uint32_t kChatRoomDepartureColor = 0x800000;

// ── LES VERBES ──────────────────────────────────────────────────────────────
//
// Chacun REFUSE, sans rien envoyer, ce que le serveur refuserait en silence ou
// couperait : les contrôles sont ceux de `CheckChatRoomSettings`, que la
// fenêtre appelle d'abord pour dire pourquoi.

// CE QUI EMPÊCHE DES RÉGLAGES DE PARTIR, dans l'ordre où le client d'origine
// les vérifie.
enum class ChatRoomSettingsFault : std::uint8_t {
  kNone,
  kNoTitle,
  // Le serveur de Moonlight n'accepte qu'un titre en ASCII : le client
  // d'origine le refuse avant de l'envoyer.
  kTitleNotAscii,
  kTitleTooLong,
  kNoPassword,
  kPasswordTooShort,
  kPasswordTooLong,
  // Un caractère du mot de passe que l'encodage du serveur ne porte pas.
  kPasswordNotEncodable,
};
[[nodiscard]] ChatRoomSettingsFault CheckChatRoomSettings(const sim::ChatRoomSettings& settings);

// Le titre a-t-il un caractère hors de l'ASCII ?
[[nodiscard]] bool ChatRoomTitleIsAscii(std::string_view title) noexcept;

// « Crée ce salon. » Refusé depuis un salon, ou tant qu'une création attend.
[[nodiscard]] core::Result<void> RequestCreateChatRoom(Connection& link, sim::ChatRoom& room,
                                                       sim::ChatRoomSettings settings,
                                                       std::uint32_t now_ms);
// « Change les réglages de mon salon. » Refusé hors salon, ou sans les droits.
[[nodiscard]] core::Result<void> RequestChangeChatRoom(Connection& link, sim::ChatRoom& room,
                                                       const sim::ChatRoomSettings& settings);
// « Laisse-moi entrer. » Le mot de passe est vide pour un salon public.
[[nodiscard]] core::Result<void> RequestEnterChatRoom(Connection& link, sim::ChatRoom& room,
                                                      std::uint32_t room_id,
                                                      std::string_view password_utf8,
                                                      std::uint32_t now_ms);
// « Je cède les droits à ce membre », « j'expulse ce membre ». Le nom en UTF-8.
[[nodiscard]] core::Result<void> RequestChatRoomOwner(Connection& link, const sim::ChatRoom& room,
                                                      std::string_view member_utf8);
[[nodiscard]] core::Result<void> RequestChatRoomExpel(Connection& link, const sim::ChatRoom& room,
                                                      std::string_view member_utf8);
// « Je sors. » Le salon se ferme AUSSITÔT : sortir ne peut pas échouer, et le
// client d'origine ferme sa fenêtre sur-le-champ.
[[nodiscard]] core::Result<void> RequestLeaveChatRoom(Connection& link, sim::ChatRoom& room);
// « Je sors », dû après une entrée aveugle : voir `sim::ChatRoom::TellBlindEntry`.
// Ne part qu'une fois, et seulement si elle est due.
[[nodiscard]] core::Result<void> RequestOwedChatRoomExit(Connection& link, sim::ChatRoom& room);

}  // namespace ro::net

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "ro/voice/voice_wire.h"

namespace ro::voice {

// ── LES MESSAGES DE CONTRÔLE, en JSON dans les trames texte ─────────────────

// CE QUE LE CLIENT DIT. Chacun rend le texte JSON à mettre dans une trame.
//
// 🔴 L'AUTHENTIFICATION NE PORTE AUCUN SECRET : le serveur de jeu garantit au
// serveur voix que ce compte joue ce personnage sous ce `login_id1`.
struct VoiceLogin {
  std::uint32_t account_id = 0;
  std::uint32_t character_id = 0;
  std::uint32_t login_id1 = 0;
  // Tiré au hasard à chaque ouverture, jamais nul : il lie les datagrammes et
  // les ordres à CETTE liaison, et une liaison qui le change remplace l'autre.
  std::uint64_t session_id = 0;
};

[[nodiscard]] std::string AuthMessage(const VoiceLogin& login);
[[nodiscard]] std::string PingMessage(std::uint32_t stamp_ms);
// Le canal ÉCOUTÉ, qui doit être aussi celui où l'on parle : le serveur ne
// relaie la proximité, le groupe et la guilde qu'entre deux joueurs accordés.
[[nodiscard]] std::string SetChannelMessage(VoiceChannel channel);
// 🔴 LE SERVEUR JETTE L'AUDIO de qui n'a pas dit qu'il parle.
[[nodiscard]] std::string TalkMessage(bool talking, std::uint64_t session_id);
// Ne plus être entendu, ou ne plus rien entendre (et ne plus être entendu).
[[nodiscard]] std::string MuteMessage(bool muted, std::uint64_t session_id);
[[nodiscard]] std::string DeafenMessage(bool deafened, std::uint64_t session_id);
// Ne plus entendre un joueur, ou l'entendre de nouveau. 🔴 LE SERVEUR GARDE LE
// BLOCAGE POUR SON COMPTE, d'une session à l'autre, mais ne le pose que sur un
// joueur en ligne : c'est par lui qu'il retrouve le compte.
[[nodiscard]] std::string BlockMessage(std::uint32_t character_id, bool blocked);

// CE QUE LE SERVEUR DIT.
struct AuthAccepted {
  // Le port UDP, ou 0 quand le serveur n'en ouvre pas : l'audio passe alors
  // par le TCP.
  std::uint16_t udp_port = 0;
  std::uint64_t udp_token = 0;
};
struct OwnPlace {
  std::string map;
  std::int32_t x = 0;
  std::int32_t y = 0;
};
struct NearbyPlayer {
  std::uint32_t character_id = 0;
  std::string name;
};
// QUI EST À PORTÉE DE VOIX, dit entier à chaque changement.
struct NearbyPlayers {
  std::vector<NearbyPlayer> players;
};
struct ChannelAccepted {
  VoiceChannel channel = VoiceChannel::kNearby;
};
// LA GUERRE DE GUILDE sur la carte : proximité et salon s'y taisent, et le
// serveur recommande le canal où se replier, s'il y en a un.
struct WarState {
  bool active = false;
  std::int32_t recommended_channel = 0;
};
struct RoomJoined {};
struct RoomLeft {};
struct Pong {};
enum class RefusalKind : std::uint8_t {
  kError,            // un ordre refusé, ou la liaison elle-même
  kBanned,           // un MJ a coupé la voix de ce compte
  kLicenseRequired,  // ce serveur réserve la voix aux comptes qui l'ont
  kFlooded,          // trop de paquets : coupé quelques minutes
};
struct Refusal {
  RefusalKind kind = RefusalKind::kError;
  std::string message;  // le texte du serveur, en anglais, pour le journal
};
// LES JOUEURS BLOQUÉS QUI SONT EN LIGNE, dits après l'authentification.
// ⚠ TUS QUAND IL N'Y EN A AUCUN : la liste se vide à l'authentification.
struct BlockedPlayers {
  std::vector<NearbyPlayer> players;
};
// Un blocage posé ou levé, confirmé par le serveur.
struct BlockChanged {
  std::uint32_t character_id = 0;
  std::string name;  // vide quand le blocage est levé
  bool blocked = false;
};
// Ce que ce client ne lit pas encore : les appels privés.
struct Unread {
  std::string type;
};

using ServerMessage =
    std::variant<AuthAccepted, OwnPlace, NearbyPlayers, ChannelAccepted, WarState, RoomJoined,
                 RoomLeft, Pong, Refusal, BlockedPlayers, BlockChanged, Unread>;

// Ce que dit un texte du serveur, ou une erreur s'il n'est pas un objet JSON
// muni d'un `type`.
[[nodiscard]] core::Result<ServerMessage> ParseServerMessage(std::string_view json);

}  // namespace ro::voice

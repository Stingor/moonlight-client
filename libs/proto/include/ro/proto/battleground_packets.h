#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LE CHAMP DE BATAILLE SUR LE FIL ─────────────────────────────────────────
//
// Les six paquets que le serveur écrit à la main (`clif.cpp`), hors des
// structures engendrées. La file d'inscription, elle, passe par les structures
// engendrées de `map_packets.h` (`PacketBgqueue*`).
//
// ⚠ LES NOMS DE 24 OCTETS SONT EN PAGE DE CODES DU JEU, terminés par un zéro
// quand ils sont plus courts : le décodeur les rend tels quels, octets bruts, et
// c'est `net` qui les convertit.

// La taille d'un nom sur le fil, zéro compris.
inline constexpr std::size_t kBattlefieldNameBytes = 24;

// LA PAROLE DU CHAMP (`ZC_BATTLEFIELD_CHAT`), longueur variable :
//
//     02DC <len>.W <orateur>.L <nom>.24B <texte>
//
// `speaker_id` vaut 0 pour un message du serveur. Le texte est déjà « nom :
// message », terminé par un zéro que le décodeur retire.
struct BattlefieldChatNotice {
  static constexpr std::uint16_t kOpcode = 0x02DC;
  static constexpr std::size_t kHeaderSize = 32;

  std::uint32_t speaker_id = 0;
  std::string speaker_name;
  std::string message;
};

// LE CAMP D'UN ACTEUR (`ZC_BATTLEFIELD_NOTIFY_CAMPINFO`) :
//
//     02DD <acteur>.L <nom>.24B <camp>.W
//
// `camp` est l'identifiant de l'équipe côté serveur : un entier quelconque, pas
// un 1 ou un 2. Le nom n'est pas employé.
struct BattlefieldCampNotice {
  static constexpr std::uint16_t kOpcode = 0x02DD;
  static constexpr std::size_t kWireSize = 32;

  std::uint32_t actor_id = 0;
  std::string name;
  std::uint16_t camp = 0;
};

// LE SCORE (`ZC_BATTLEFIELD_NOTIFY_POINT`) : 02DE <lion>.W <aigle>.W
struct BattlefieldScoreNotice {
  static constexpr std::uint16_t kOpcode = 0x02DE;
  static constexpr std::size_t kWireSize = 6;

  std::uint16_t lion = 0;
  std::uint16_t eagle = 0;
};

// LA POSITION D'UN COÉQUIPIER (`ZC_BATTLEFIELD_NOTIFY_POSITION`) :
//
//     02DF <compte>.L <nom>.24B <classe>.W <x>.W <y>.W
//
// ⚠ LE RETRAIT : nom vide, classe 0 et x = y = 0xFFFF, que le décodeur rend en
// −1 — d'où les coordonnées signées.
struct BattlefieldPositionNotice {
  static constexpr std::uint16_t kOpcode = 0x02DF;
  static constexpr std::size_t kWireSize = 36;

  std::uint32_t account_id = 0;
  std::string name;
  std::uint16_t job = 0;
  std::int16_t x = 0;
  std::int16_t y = 0;
};

// LA RÉPONSE À « J'ENTRE » (`ZC_REPLY_ACK_LOBBY_ADMISSION`) :
//
//     08E1 <résultat>.B <terrain>.24B <salle>.24B
//
// Envoyée au seul joueur qui accepte ; `field_name` porte alors le nom de la
// CARTE.
struct LobbyAdmissionAck {
  static constexpr std::uint16_t kOpcode = 0x08E1;
  static constexpr std::size_t kWireSize = 51;

  std::uint8_t result = 0;
  std::string field_name;
  std::string lobby_name;
};

// « JE PARLE AU CHAMP » (`CZ_BATTLEFIELD_CHAT`), longueur variable :
//
//     02DB <len>.W <« nom : texte »>
//
// Même forme et même exigence que la parole publique : le serveur compare le
// nom en tête à celui du personnage. Les deux arrivent déjà en page de codes.
struct BattlefieldChatRequest {
  static constexpr std::uint16_t kOpcode = 0x02DB;
  static constexpr std::size_t kHeaderSize = 4;

  std::string speaker;
  std::string text;
};

// Faux si la trame n'a ni le bon opcode ni une taille acceptable — pour la
// parole, une longueur déclarée qui ne correspond pas à la trame.
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, BattlefieldChatNotice& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, BattlefieldCampNotice& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, BattlefieldScoreNotice& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, BattlefieldPositionNotice& packet);
[[nodiscard]] bool Decode(std::span<const std::uint8_t> frame, LobbyAdmissionAck& packet);

// L'ENVERS DES DÉCODEURS, pour les essais : le client n'émet pas ces paquets.
[[nodiscard]] std::vector<std::uint8_t> Encode(const BattlefieldChatNotice& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const BattlefieldCampNotice& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const BattlefieldScoreNotice& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const BattlefieldPositionNotice& packet);
[[nodiscard]] std::vector<std::uint8_t> Encode(const LobbyAdmissionAck& packet);

// Refuse un nom vide et une ligne que le serveur couperait, comme la parole
// publique.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const BattlefieldChatRequest& request);

}  // namespace ro::proto

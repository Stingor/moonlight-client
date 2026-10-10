#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LES SALONS DE DISCUSSION : les demandes qui n'ont pas de structure ──────
//
// Créer, entrer et changer les réglages sont décrits par le serveur et
// engendrés dans `map_packets.h`. Ces trois-là n'y figurent pas : le serveur
// les lit champ par champ, ou ne lit que l'opcode. Ils sont donc écrits ici, et
// recoupés avec ce que sa table déclare lire.

// La place d'un nom de personnage sur le fil, zéro final compris.
inline constexpr std::size_t kChatRoomNameBytes = 24;

// « Je cède les droits de chef à ce membre. »
//
// ⚠ LE RÔLE EST CELUI QUE LE MEMBRE PRENDRA : zéro fait de lui le chef. Le
// serveur n'en lit pas d'autre.
struct ChatRoomRoleRequest {
  static constexpr std::uint16_t kOpcode = 0x00E0;
  static constexpr std::size_t kWireSize = 30;
  static constexpr std::uint32_t kOwnerRole = 0;

  std::uint32_t role = kOwnerRole;
  // Le nom, déjà dans l'encodage du serveur : il le compare octet par octet.
  std::string name;
};

// « J'expulse ce membre de mon salon. »
struct ChatRoomExpelRequest {
  static constexpr std::uint16_t kOpcode = 0x00E2;
  static constexpr std::size_t kWireSize = 26;

  std::string name;
};

// « Je quitte le salon. »
struct ChatRoomExitRequest {
  static constexpr std::uint16_t kOpcode = 0x00E3;
  static constexpr std::size_t kWireSize = 2;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const ChatRoomRoleRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const ChatRoomExpelRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const ChatRoomExitRequest& request);

}  // namespace ro::proto

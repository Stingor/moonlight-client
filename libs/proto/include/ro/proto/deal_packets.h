#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── L'ÉCHANGE ENTRE JOUEURS : les demandes qui n'ont pas de structure ────────
//
// Les sept paquets reçus et l'ajout d'un item (`CzAddExchangeItem`) sont
// décrits par le serveur et engendrés dans `map_packets.h`. Ces cinq-là n'y
// figurent pas : le serveur les lit champ par champ, ou ne lit que l'opcode.
// Ils sont donc écrits ici, et recoupés avec ce que sa table déclare lire.

// « Je propose un échange à ce joueur », par son identifiant de COMPTE — celui
// de son acteur.
struct DealRequest {
  static constexpr std::uint16_t kOpcode = 0x00E4;
  static constexpr std::size_t kWireSize = 6;

  std::uint32_t partner_aid = 0;
};

// « J'accepte » ou « je refuse » la proposition reçue.
//
// ⚠ LE SERVEUR NE LIT QUE CES DEUX VALEURS : toute autre le fait sortir sans
// rien répondre, et la proposition reste pendante chez lui.
struct DealAnswer {
  static constexpr std::uint16_t kOpcode = 0x00E6;
  static constexpr std::size_t kWireSize = 3;
  static constexpr std::uint8_t kAccept = 3;
  static constexpr std::uint8_t kRefuse = 4;

  bool accept = false;
};

// « Mon offre est prête » : le verrou. Plus rien ne s'y ajoute ensuite.
struct DealLockRequest {
  static constexpr std::uint16_t kOpcode = 0x00EB;
  static constexpr std::size_t kWireSize = 2;
};

// « J'annule l'échange » — ou la proposition encore pendante.
struct DealCancelRequest {
  static constexpr std::uint16_t kOpcode = 0x00ED;
  static constexpr std::size_t kWireSize = 2;
};

// « J'échange. » Le serveur n'exécute qu'une fois les DEUX joueurs passés par
// là, chacun après les deux verrous.
struct DealExecuteRequest {
  static constexpr std::uint16_t kOpcode = 0x00EF;
  static constexpr std::size_t kWireSize = 2;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const DealRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const DealAnswer& answer);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const DealLockRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const DealCancelRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const DealExecuteRequest& request);

}  // namespace ro::proto

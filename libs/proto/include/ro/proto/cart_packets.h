#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LA LISTE DES CHARRETTES : les trois paquets qui changent son apparence ──
//
// 🔴 AUCUN N'A DE STRUCTURE CÔTÉ SERVEUR. Il les écrit et les lit champ par
// champ (`WFIFOB`, `RFIFOW`) ; ils sont donc écrits ici, et recoupés avec ce que
// le serveur déclare lire.

// « Change l'apparence de ma charrette pour ce type. » C'est la réponse de la
// liste qu'ouvre `MC_CHANGECART`.
//
// ⚠ LE SERVEUR REVALIDE : la compétence apprise, et un palier de niveau de base
// par type. Un type refusé ne vaut aucune réponse du tout.
struct ChangeCartRequest {
  static constexpr std::uint16_t kOpcode = 0x01AF;
  static constexpr std::size_t kWireSize = 4;

  std::uint16_t cart_type = 0;
};

// « Je prends celle-ci », en réponse à la liste que le SERVEUR a proposée —
// celle de `MC_CARTDECORATE`.
//
// ⚠ L'IDENTITÉ EST CELLE QUE LA LISTE PORTAIT, recopiée telle quelle : le
// serveur la compare au compte et ignore sans un mot une réponse qui ne la
// rappelle pas.
struct SelectCartRequest {
  static constexpr std::uint16_t kOpcode = 0x0980;
  static constexpr std::size_t kWireSize = 7;

  std::uint32_t identity = 0;
  std::uint8_t cart_type = 0;
};

// LA LISTE QUE LE SERVEUR PROPOSE : une identité, puis un type par octet.
//
// ⚠ LE NOMBRE DE TYPES EST CE QUI RESTE DE LA LONGUEUR après l'en-tête de huit
// octets. Une liste vide n'ouvre rien.
struct SelectCartOffer {
  static constexpr std::uint16_t kOpcode = 0x097F;
  static constexpr std::size_t kHeaderSize = 8;

  std::uint32_t identity = 0;
  std::vector<std::uint8_t> cart_types;
};

[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const ChangeCartRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const SelectCartRequest& request);

// ⚠ REFUSE une trame dont la longueur annoncée n'est pas la sienne.
[[nodiscard]] core::Result<SelectCartOffer> DecodeSelectCartOffer(
    std::span<const std::uint8_t> frame);

// L'ENVERS DU DÉCODEUR, pour les essais : le client n'émet pas ce paquet.
[[nodiscard]] std::vector<std::uint8_t> EncodeSelectCartOffer(const SelectCartOffer& offer);

}  // namespace ro::proto

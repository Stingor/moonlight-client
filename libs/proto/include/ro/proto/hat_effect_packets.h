#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// LA CARTE DES ITEMS QUI PORTENT UN EFFET DE CHAPEAU, que ce serveur pousse à
// un client vérifié.
//
// Un costume peut se montrer par un effet plutôt que par un sprite : son script
// déclare l'effet, et rien dans les tables du client ne relie l'item à l'effet.
// Le serveur fait donc le relevé une fois, et envoie la liste.
//
// 🔴 LA QUEUE N'A PAS DE STRUCTURE CÔTÉ SERVEUR : il l'écrit champ par champ,
// après un en-tête de six octets — l'item sur quatre, l'effet sur deux.
struct HatEffectMapEntry {
  std::uint32_t item_id = 0;
  // L'ordinal de l'effet de chapeau, celui que `hatEffectTable` indexe.
  std::int16_t ordinal = 0;
};

struct HatEffectMap {
  static constexpr std::uint16_t kOpcode = 0x0F17;
  static constexpr std::size_t kHeaderSize = 6;
  static constexpr std::size_t kEntrySize = 6;

  std::vector<HatEffectMapEntry> entries;
};

// ⚠ REFUSE une trame dont la longueur annoncée n'est pas la sienne, ou dont la
// queue ne se découpe pas en entrées entières.
[[nodiscard]] core::Result<HatEffectMap> DecodeHatEffectMap(std::span<const std::uint8_t> frame);

// L'ENVERS DU DÉCODEUR, pour les essais : le client n'émet pas ce paquet.
[[nodiscard]] std::vector<std::uint8_t> EncodeHatEffectMap(const HatEffectMap& map);

}  // namespace ro::proto

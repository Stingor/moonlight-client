#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// LES SORTS QU'AUTOSPELL PROPOSE, écrits à la main par le serveur
// (clif_autospell) : un en-tête de quatre octets, puis un identifiant de
// compétence sur quatre octets par sort.
struct AutoSpellList {
  static constexpr std::uint16_t kOpcode = 0x0AFB;
  static constexpr std::size_t kHeaderSize = 4;
  static constexpr std::size_t kEntrySize = 4;

  std::vector<std::int32_t> skill_ids;
};

// ⚠ REFUSE une trame dont la longueur annoncée n'est pas la sienne, ou dont la
// queue ne se découpe pas en identifiants entiers.
[[nodiscard]] core::Result<AutoSpellList> DecodeAutoSpellList(std::span<const std::uint8_t> frame);

// L'ENVERS DU DÉCODEUR, pour les essais : le client n'émet pas ce paquet.
[[nodiscard]] std::vector<std::uint8_t> EncodeAutoSpellList(const AutoSpellList& list);

}  // namespace ro::proto

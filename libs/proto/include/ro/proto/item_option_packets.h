#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// `ZC_CHANGE_ITEM_OPTION` : ce qu'une pièce est devenue.
//
// 🔴 LE GÉNÉRATEUR L'ÉCARTE (`tools/packetgen/refus.csv`) : selon la version du
// serveur, le raffinement change de place et de signe. Il se lit donc ici, sous sa
// seule forme à ce `PACKETVER` : cartes et options AVANT le raffinement.
struct ItemOptionChange {
  static constexpr std::uint16_t kOpcode = 0x0B43;
  static constexpr std::size_t kWireSize = 48;
  static constexpr std::size_t kCards = 4;
  static constexpr std::size_t kOptions = 5;

  struct Option {
    std::int16_t index = 0;
    std::int16_t value = 0;
    std::uint8_t param = 0;
  };

  std::int16_t client_slot = 0;
  bool damaged = false;
  std::array<std::uint32_t, kCards> cards = {};
  std::array<Option, kOptions> options = {};
  std::uint8_t refine_level = 0;
  std::uint8_t grade = 0;
};

// ⚠ REFUSE une trame d'une autre taille : une disposition qui a changé se dit.
[[nodiscard]] core::Result<ItemOptionChange> DecodeItemOptionChange(
    std::span<const std::uint8_t> frame);

// L'ENVERS DU DÉCODEUR, pour les essais : le client n'émet pas ce paquet.
[[nodiscard]] std::vector<std::uint8_t> EncodeItemOptionChange(const ItemOptionChange& change);

}  // namespace ro::proto

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// UNE PIÈCE RÉPARABLE, telle que la liste et la demande la portent : vingt-quatre
// octets, place, objet, quatre cartes, raffinement et grade.
struct RepairableEntry {
  std::int16_t index = 0;
  std::uint32_t item_id = 0;
  std::array<std::uint32_t, 4> cards = {};
  std::uint8_t refine = 0;
  std::uint8_t grade = 0;
};

// LES PIÈCES QUE REPAIR WEAPON PROPOSE, écrites à la main par le serveur
// (clif_item_repair_list) : un en-tête de quatre octets, puis une entrée de
// vingt-quatre octets par pièce abîmée.
struct RepairItemList {
  static constexpr std::uint16_t kOpcode = 0x0B65;
  static constexpr std::size_t kHeaderSize = 4;
  static constexpr std::size_t kEntrySize = 24;

  std::vector<RepairableEntry> items;
};

[[nodiscard]] core::Result<RepairItemList> DecodeRepairItemList(
    std::span<const std::uint8_t> frame);

[[nodiscard]] std::vector<std::uint8_t> EncodeRepairItemList(const RepairItemList& list);

// LA DEMANDE : `CZ_REQ_ITEMREPAIR2`, l'entrée choisie recopiée telle quelle.
struct RepairItemRequest {
  static constexpr std::uint16_t kOpcode = 0x0B66;
  static constexpr std::size_t kWireSize = 26;

  RepairableEntry item;
};

[[nodiscard]] std::vector<std::uint8_t> EncodeRepairItemRequest(const RepairItemRequest& request);

}  // namespace ro::proto

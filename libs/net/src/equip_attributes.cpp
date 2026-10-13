// Ce qu'apporte une pièce ou un ensemble. Voir l'en-tête.

#include "ro/net/equip_attributes.h"

#include "ro/proto/map_packets.h"

namespace ro::net {

std::optional<sim::EquipAttributes> ReadEquipAttributes(const Frame& frame) {
  if (frame.opcode != proto::ZcMoonlightEquipAttributes::kOpcode) {
    return std::nullopt;
  }
  proto::ZcMoonlightEquipAttributes packet;
  if (!proto::Decode(frame.bytes, packet)) {
    return std::nullopt;
  }
  sim::EquipAttributes told;
  switch (static_cast<sim::EquipAttributesKind>(packet.kind)) {
    case sim::EquipAttributesKind::kPiece:
    case sim::EquipAttributesKind::kCombo:
    case sim::EquipAttributesKind::kPet:
      told.kind = static_cast<sim::EquipAttributesKind>(packet.kind);
      break;
    default:
      return std::nullopt;
  }
  told.equipped = packet.equipped != 0;
  told.id = packet.id;
  told.bonuses.reserve(packet.bonuses.size());
  for (const proto::MoonlightEquipBonus& entry : packet.bonuses) {
    told.bonuses.push_back(
        sim::EquipBonus{.level = entry.level, .type = entry.type, .values = entry.values});
  }
  return told;
}

}  // namespace ro::net

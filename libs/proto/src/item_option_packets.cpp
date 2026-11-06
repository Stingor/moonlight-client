// Ce qu'une pièce est devenue. Voir l'en-tête pour ce que le générateur écarte.

#include "ro/proto/item_option_packets.h"

#include <cstdint>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/proto/packet_lengths.h"

namespace ro::proto {

core::Result<ItemOptionChange> DecodeItemOptionChange(std::span<const std::uint8_t> frame) {
  const auto declared = PacketLength(ItemOptionChange::kOpcode);
  if (!declared.has_value() ||
      *declared != static_cast<std::int16_t>(ItemOptionChange::kWireSize)) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion,
                      "le changement d'une pièce n'a plus la longueur attendue");
  }
  if (frame.size() != ItemOptionChange::kWireSize) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "changement d'une pièce de mauvaise taille");
  }
  core::ByteReader reader(frame);
  if (reader.U16() != ItemOptionChange::kOpcode) {
    return core::Fail(core::ErrorCode::kBadSignature, "trame d'un autre paquet qu'un changement");
  }
  ItemOptionChange change;
  change.client_slot = reader.I16();
  change.damaged = reader.I8() != 0;
  for (std::uint32_t& card : change.cards) {
    card = reader.U32();
  }
  for (ItemOptionChange::Option& option : change.options) {
    option.index = reader.I16();
    option.value = reader.I16();
    option.param = reader.U8();
  }
  change.refine_level = reader.U8();
  change.grade = reader.U8();
  if (!reader.ok()) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "changement d'une pièce tronqué");
  }
  return change;
}

std::vector<std::uint8_t> EncodeItemOptionChange(const ItemOptionChange& change) {
  std::vector<std::uint8_t> frame;
  core::ByteWriter writer(frame);
  writer.U16(ItemOptionChange::kOpcode);
  writer.I16(change.client_slot);
  writer.I8(change.damaged ? 1 : 0);
  for (const std::uint32_t card : change.cards) {
    writer.U32(card);
  }
  for (const ItemOptionChange::Option& option : change.options) {
    writer.I16(option.index);
    writer.I16(option.value);
    writer.U8(option.param);
  }
  writer.U8(change.refine_level);
  writer.U8(change.grade);
  return frame;
}

}  // namespace ro::proto

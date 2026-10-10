#include "ro/proto/repair_packets.h"

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"

namespace ro::proto {
namespace {

void WriteEntry(core::ByteWriter& writer, const RepairableEntry& entry) {
  writer.I16(entry.index);
  writer.U32(entry.item_id);
  for (const std::uint32_t card : entry.cards) {
    writer.U32(card);
  }
  writer.U8(entry.refine);
  writer.U8(entry.grade);
}

}  // namespace

core::Result<RepairItemList> DecodeRepairItemList(std::span<const std::uint8_t> frame) {
  core::ByteReader reader(frame);
  if (reader.U16() != RepairItemList::kOpcode) {
    return core::Fail(core::ErrorCode::kBadSignature,
                      "trame d'un autre paquet que la liste de réparation");
  }
  const std::uint16_t length = reader.U16();
  if (!reader.ok() || length != frame.size() || length < RepairItemList::kHeaderSize ||
      (frame.size() - RepairItemList::kHeaderSize) % RepairItemList::kEntrySize != 0) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "liste de réparation tronquée");
  }
  RepairItemList list;
  list.items.resize((frame.size() - RepairItemList::kHeaderSize) / RepairItemList::kEntrySize);
  for (RepairableEntry& entry : list.items) {
    entry.index = reader.I16();
    entry.item_id = reader.U32();
    for (std::uint32_t& card : entry.cards) {
      card = reader.U32();
    }
    entry.refine = reader.U8();
    entry.grade = reader.U8();
  }
  if (!reader.ok()) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "liste de réparation tronquée");
  }
  return list;
}

std::vector<std::uint8_t> EncodeRepairItemList(const RepairItemList& list) {
  std::vector<std::uint8_t> frame;
  core::ByteWriter writer(frame);
  writer.U16(RepairItemList::kOpcode);
  writer.U16(static_cast<std::uint16_t>(RepairItemList::kHeaderSize +
                                        list.items.size() * RepairItemList::kEntrySize));
  for (const RepairableEntry& entry : list.items) {
    WriteEntry(writer, entry);
  }
  return frame;
}

std::vector<std::uint8_t> EncodeRepairItemRequest(const RepairItemRequest& request) {
  std::vector<std::uint8_t> frame;
  core::ByteWriter writer(frame);
  writer.U16(RepairItemRequest::kOpcode);
  WriteEntry(writer, request.item);
  return frame;
}

}  // namespace ro::proto

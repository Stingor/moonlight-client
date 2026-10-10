#include "ro/proto/autospell_packets.h"

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"

namespace ro::proto {

core::Result<AutoSpellList> DecodeAutoSpellList(std::span<const std::uint8_t> frame) {
  core::ByteReader reader(frame);
  if (reader.U16() != AutoSpellList::kOpcode) {
    return core::Fail(core::ErrorCode::kBadSignature,
                      "trame d'un autre paquet que la liste d'Autospell");
  }
  const std::uint16_t length = reader.U16();
  if (!reader.ok() || length != frame.size() || length < AutoSpellList::kHeaderSize ||
      (frame.size() - AutoSpellList::kHeaderSize) % AutoSpellList::kEntrySize != 0) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "liste d'Autospell tronquée");
  }
  AutoSpellList list;
  list.skill_ids.resize((frame.size() - AutoSpellList::kHeaderSize) / AutoSpellList::kEntrySize);
  for (std::int32_t& skill_id : list.skill_ids) {
    skill_id = reader.I32();
  }
  if (!reader.ok()) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "liste d'Autospell tronquée");
  }
  return list;
}

std::vector<std::uint8_t> EncodeAutoSpellList(const AutoSpellList& list) {
  std::vector<std::uint8_t> frame;
  core::ByteWriter writer(frame);
  writer.U16(AutoSpellList::kOpcode);
  writer.U16(static_cast<std::uint16_t>(AutoSpellList::kHeaderSize +
                                        list.skill_ids.size() * AutoSpellList::kEntrySize));
  for (const std::int32_t skill_id : list.skill_ids) {
    writer.I32(skill_id);
  }
  return frame;
}

}  // namespace ro::proto

#include "ro/proto/quest_marker_packets.h"

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"

namespace ro::proto {

bool Decode(std::span<const std::uint8_t> frame, QuestMarkerNotice& packet) {
  if (frame.size() != QuestMarkerNotice::kWireSize) {
    return false;
  }
  core::ByteReader reader(frame);
  if (reader.U16() != QuestMarkerNotice::kOpcode) {
    return false;
  }
  QuestMarkerNotice read;
  read.npc_id = reader.U32();
  read.x = reader.I16();
  read.y = reader.I16();
  read.effect = reader.U16();
  read.color = reader.U16();
  if (!reader.ok()) {
    return false;
  }
  packet = read;
  return true;
}

std::vector<std::uint8_t> Encode(const QuestMarkerNotice& packet) {
  std::vector<std::uint8_t> frame;
  core::ByteWriter writer(frame);
  writer.U16(QuestMarkerNotice::kOpcode);
  writer.U32(packet.npc_id);
  writer.I16(packet.x);
  writer.I16(packet.y);
  writer.U16(packet.effect);
  writer.U16(packet.color);
  return frame;
}

}  // namespace ro::proto

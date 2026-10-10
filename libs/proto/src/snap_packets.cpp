#include "ro/proto/snap_packets.h"

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"

namespace ro::proto {

bool Decode(std::span<const std::uint8_t> frame, SnapNotice& packet) {
  if (frame.size() != SnapNotice::kWireSize) {
    return false;
  }
  core::ByteReader reader(frame);
  if (reader.U16() != SnapNotice::kOpcode) {
    return false;
  }
  SnapNotice read;
  read.actor_id = reader.U32();
  read.x = reader.I16();
  read.y = reader.I16();
  if (!reader.ok()) {
    return false;
  }
  packet = read;
  return true;
}

std::vector<std::uint8_t> Encode(const SnapNotice& packet) {
  std::vector<std::uint8_t> frame;
  core::ByteWriter writer(frame);
  writer.U16(SnapNotice::kOpcode);
  writer.U32(packet.actor_id);
  writer.I16(packet.x);
  writer.I16(packet.y);
  return frame;
}

}  // namespace ro::proto

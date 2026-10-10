#include "ro/proto/navigation_packets.h"

#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"

namespace ro::proto {

bool Decode(std::span<const std::uint8_t> frame, NavigationActive& packet) {
  if (frame.size() != NavigationActive::kWireSize) {
    return false;
  }
  core::ByteReader reader(frame);
  if (reader.U16() != NavigationActive::kOpcode) {
    return false;
  }
  NavigationActive read;
  read.type = reader.U8();
  read.flag = reader.U8();
  read.hide_window = reader.U8();
  read.map = std::string(reader.FixedString(NavigationActive::kMapNameWidth));
  read.x = reader.U16();
  read.y = reader.U16();
  read.mob_id = reader.U16();
  if (!reader.ok()) {
    return false;
  }
  packet = std::move(read);
  return true;
}

std::vector<std::uint8_t> Encode(const NavigationActive& packet) {
  std::vector<std::uint8_t> out;
  out.reserve(NavigationActive::kWireSize);
  core::ByteWriter writer(out);
  writer.U16(NavigationActive::kOpcode);
  writer.U8(packet.type);
  writer.U8(packet.flag);
  writer.U8(packet.hide_window);
  writer.FixedString(packet.map, NavigationActive::kMapNameWidth);
  writer.U16(packet.x);
  writer.U16(packet.y);
  writer.U16(packet.mob_id);
  return out;
}

}  // namespace ro::proto

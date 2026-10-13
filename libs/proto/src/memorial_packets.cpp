// Les paquets d'instance. Voir l'en-tête.

#include "ro/proto/memorial_packets.h"

#include <string>
#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"

namespace ro::proto {
namespace {

// La taille de la commande de destruction : l'opcode et la commande.
constexpr std::size_t kInstanceCommandWireSize = 6;

// Lit l'opcode attendu d'une trame de taille fixe.
[[nodiscard]] bool Opens(std::span<const std::uint8_t> frame, std::uint16_t opcode,
                         std::size_t wire_size, core::ByteReader& reader) {
  return frame.size() == wire_size && reader.U16() == opcode;
}

}  // namespace

bool Decode(std::span<const std::uint8_t> frame, InstanceCreatedNotice& packet) {
  core::ByteReader reader(frame);
  if (!Opens(frame, InstanceCreatedNotice::kOpcode, InstanceCreatedNotice::kWireSize, reader)) {
    return false;
  }
  InstanceCreatedNotice read;
  read.name = std::string(reader.FixedString(kInstanceNameBytes));
  read.wait_position = reader.U16();
  if (!reader.ok()) {
    return false;
  }
  packet = std::move(read);
  return true;
}

bool Decode(std::span<const std::uint8_t> frame, InstanceWaitNotice& packet) {
  core::ByteReader reader(frame);
  if (!Opens(frame, InstanceWaitNotice::kOpcode, InstanceWaitNotice::kWireSize, reader)) {
    return false;
  }
  InstanceWaitNotice read;
  read.wait_position = reader.U16();
  if (!reader.ok()) {
    return false;
  }
  packet = read;
  return true;
}

bool Decode(std::span<const std::uint8_t> frame, InstanceStatusNotice& packet) {
  core::ByteReader reader(frame);
  if (!Opens(frame, InstanceStatusNotice::kOpcode, InstanceStatusNotice::kWireSize, reader)) {
    return false;
  }
  InstanceStatusNotice read;
  read.name = std::string(reader.FixedString(kInstanceNameBytes));
  read.keep_limit_unix_s = reader.U32();
  read.idle_limit_unix_s = reader.U32();
  if (!reader.ok()) {
    return false;
  }
  packet = std::move(read);
  return true;
}

bool Decode(std::span<const std::uint8_t> frame, InstanceChangeNotice& packet) {
  core::ByteReader reader(frame);
  if (!Opens(frame, InstanceChangeNotice::kOpcode, InstanceChangeNotice::kWireSize, reader)) {
    return false;
  }
  InstanceChangeNotice read;
  read.type = reader.U32();
  read.limit_unix_s = reader.U32();
  if (!reader.ok()) {
    return false;
  }
  packet = read;
  return true;
}

std::vector<std::uint8_t> Encode(const InstanceCreatedNotice& packet) {
  std::vector<std::uint8_t> frame;
  frame.reserve(InstanceCreatedNotice::kWireSize);
  core::ByteWriter writer(frame);
  writer.U16(InstanceCreatedNotice::kOpcode);
  writer.FixedString(packet.name, kInstanceNameBytes);
  writer.U16(packet.wait_position);
  return frame;
}

std::vector<std::uint8_t> Encode(const InstanceWaitNotice& packet) {
  std::vector<std::uint8_t> frame;
  frame.reserve(InstanceWaitNotice::kWireSize);
  core::ByteWriter writer(frame);
  writer.U16(InstanceWaitNotice::kOpcode);
  writer.U16(packet.wait_position);
  return frame;
}

std::vector<std::uint8_t> Encode(const InstanceStatusNotice& packet) {
  std::vector<std::uint8_t> frame;
  frame.reserve(InstanceStatusNotice::kWireSize);
  core::ByteWriter writer(frame);
  writer.U16(InstanceStatusNotice::kOpcode);
  writer.FixedString(packet.name, kInstanceNameBytes);
  writer.U32(packet.keep_limit_unix_s);
  writer.U32(packet.idle_limit_unix_s);
  return frame;
}

std::vector<std::uint8_t> Encode(const InstanceChangeNotice& packet) {
  std::vector<std::uint8_t> frame;
  frame.reserve(InstanceChangeNotice::kWireSize);
  core::ByteWriter writer(frame);
  writer.U16(InstanceChangeNotice::kOpcode);
  writer.U32(packet.type);
  writer.U32(packet.limit_unix_s);
  return frame;
}

std::vector<std::uint8_t> EncodeInstanceDestroyCommand() {
  std::vector<std::uint8_t> frame;
  frame.reserve(kInstanceCommandWireSize);
  core::ByteWriter writer(frame);
  writer.U16(kInstanceCommandOpcode);
  writer.U32(kInstanceCommandDestroy);
  return frame;
}

}  // namespace ro::proto

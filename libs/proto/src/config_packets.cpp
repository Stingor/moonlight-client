#include "ro/proto/config_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"

#include "layout_holds.h"

namespace ro::proto {
namespace {

using core::ByteReader;
using core::ByteWriter;
using core::ErrorCode;

constexpr std::string_view kConfigHandler = "clif_parse_configuration";
// Le réglage, puis sa valeur.
constexpr std::array<std::uint16_t, 2> kConfigOffsets = {2, 6};

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const ConfigRequest& request) {
  if (!FixedLayoutHolds<ConfigRequest>(kConfigHandler, kConfigOffsets)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la disposition écrite pour un réglage du personnage ne correspond plus à "
                      "ce que le serveur déclare lire");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(ConfigRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(ConfigRequest::kOpcode);
  writer.U32(static_cast<std::uint32_t>(request.type));
  writer.U32(request.value);
  return frame;
}

core::Result<ConfigAck> DecodeConfigAck(std::span<const std::uint8_t> frame) {
  if (frame.size() != ConfigAck::kWireSize) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "écho de réglage de longueur inattendue");
  }
  ByteReader reader(frame);
  if (reader.U16() != ConfigAck::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature, "trame d'un autre paquet que l'écho de réglage");
  }
  ConfigAck ack;
  ack.type = reader.U32();
  ack.value = reader.U32();
  return ack;
}

std::vector<std::uint8_t> EncodeConfigAck(const ConfigAck& ack) {
  std::vector<std::uint8_t> frame;
  frame.reserve(ConfigAck::kWireSize);
  ByteWriter writer(frame);
  writer.U16(ConfigAck::kOpcode);
  writer.U32(ack.type);
  writer.U32(ack.value);
  return frame;
}

}  // namespace ro::proto

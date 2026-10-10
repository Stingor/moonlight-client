#include "ro/proto/progress_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

namespace ro::proto {
namespace {

using core::ByteReader;
using core::ByteWriter;
using core::ErrorCode;

// ⚠ AUCUN CHAMP : le serveur enregistre ce paquet sous son gestionnaire et sa
// longueur seuls. Le recoupement porte donc sur les deux.
constexpr std::string_view kProgressDoneHandler = "clif_parse_progressbar";
constexpr std::array<std::uint16_t, 0> kProgressDoneOffsets = {};

std::vector<std::uint8_t> OpcodeOnly(std::uint16_t opcode) {
  std::vector<std::uint8_t> frame;
  ByteWriter writer(frame);
  writer.U16(opcode);
  return frame;
}

}  // namespace

core::Result<ProgressStart> DecodeProgressStart(std::span<const std::uint8_t> frame) {
  if (frame.size() != ProgressStart::kWireSize) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "barre de progression : longueur inattendue");
  }
  ByteReader reader(frame);
  if (reader.U16() != ProgressStart::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature,
                      "trame d'un autre paquet que la barre de progression");
  }
  ProgressStart start;
  start.color = reader.U32();
  start.seconds = reader.U32();
  if (!reader.ok()) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "barre de progression tronquée");
  }
  return start;
}

std::vector<std::uint8_t> EncodeProgressStart(const ProgressStart& start) {
  std::vector<std::uint8_t> frame;
  frame.reserve(ProgressStart::kWireSize);
  ByteWriter writer(frame);
  writer.U16(ProgressStart::kOpcode);
  writer.U32(start.color);
  writer.U32(start.seconds);
  return frame;
}

core::Result<std::vector<std::uint8_t>> EncodeProgressDone() {
  const auto length = PacketLength(ProgressDone::kOpcode);
  if (!MatchesServerLayout(ProgressDone::kOpcode, kProgressDoneHandler, kProgressDoneOffsets) ||
      !length.has_value() || static_cast<std::size_t>(*length) != ProgressDone::kWireSize) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la disposition écrite pour la fin d'une barre de progression ne "
                      "correspond plus à ce que le serveur déclare lire");
  }
  return OpcodeOnly(ProgressDone::kOpcode);
}

std::vector<std::uint8_t> EncodeProgressCancel() { return OpcodeOnly(ProgressCancel::kOpcode); }

std::vector<std::uint8_t> EncodeProgressActor(const ZcProgressActor& packet) {
  std::vector<std::uint8_t> frame;
  frame.reserve(ZcProgressActor::kWireSize);
  ByteWriter writer(frame);
  writer.U16(kProgressActorOpcode);
  writer.I32(packet.GID);
  writer.I32(packet.color);
  writer.U32(packet.time);
  return frame;
}

}  // namespace ro::proto

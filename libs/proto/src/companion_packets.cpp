// Voir l'en-tête.

#include "ro/proto/companion_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_writer.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

namespace ro::proto {
namespace {

using core::ByteWriter;
using core::ErrorCode;

constexpr std::string_view kAttackHandler = "clif_parse_HomAttack";
// Le compagnon, la cible, l'action.
constexpr std::array<std::uint16_t, 3> kAttackOffsets = {2, 6, 10};
constexpr std::string_view kReturnHandler = "clif_parse_HomMoveToMaster";
constexpr std::array<std::uint16_t, 1> kReturnOffsets = {2};

[[nodiscard]] bool LengthIs(std::uint16_t opcode, std::size_t wire_size) {
  const auto length = PacketLength(opcode);
  return length.has_value() && static_cast<std::size_t>(*length) == wire_size;
}

[[nodiscard]] core::Error Drift() {
  return core::Error{ErrorCode::kUnsupportedVersion,
                     "la disposition écrite pour un ordre de compagnon ne correspond plus à ce "
                     "que le serveur déclare lire"};
}

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const CompanionAttackRequest& request) {
  if (!MatchesServerLayout(CompanionAttackRequest::kOpcode, kAttackHandler, kAttackOffsets) ||
      !LengthIs(CompanionAttackRequest::kOpcode, CompanionAttackRequest::kWireSize)) {
    return std::unexpected(Drift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(CompanionAttackRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(CompanionAttackRequest::kOpcode);
  writer.U32(request.companion_id);
  writer.U32(request.target_id);
  writer.U8(request.action);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const CompanionReturnRequest& request) {
  if (!MatchesServerLayout(CompanionReturnRequest::kOpcode, kReturnHandler, kReturnOffsets) ||
      !LengthIs(CompanionReturnRequest::kOpcode, CompanionReturnRequest::kWireSize)) {
    return std::unexpected(Drift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(CompanionReturnRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(CompanionReturnRequest::kOpcode);
  writer.U32(request.companion_id);
  return frame;
}

}  // namespace ro::proto

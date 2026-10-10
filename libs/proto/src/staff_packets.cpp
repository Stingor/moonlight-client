#include "ro/proto/staff_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_writer.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

namespace ro::proto {
namespace {

using core::ByteWriter;
using core::ErrorCode;

constexpr std::string_view kKickHandler = "clif_parse_GMKick";
constexpr std::array<std::uint16_t, 1> kKickOffsets = {2};

core::Error LayoutDrift() {
  return core::Error{ErrorCode::kUnsupportedVersion,
                     "la disposition écrite pour cet ordre du staff ne correspond plus à ce "
                     "que le serveur déclare lire"};
}

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const KickRequest& request) {
  if (!MatchesServerLayout(KickRequest::kOpcode, kKickHandler, kKickOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  const auto length = PacketLength(KickRequest::kOpcode);
  if (!length.has_value() || static_cast<std::size_t>(*length) != KickRequest::kWireSize) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(KickRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(KickRequest::kOpcode);
  writer.U32(request.account_id);
  return frame;
}

}  // namespace ro::proto

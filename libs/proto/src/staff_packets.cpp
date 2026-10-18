#include "ro/proto/staff_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_writer.h"

#include "layout_holds.h"

namespace ro::proto {
namespace {

using core::ByteWriter;
using core::ErrorCode;

constexpr std::string_view kKickHandler = "clif_parse_GMKick";
constexpr std::array<std::uint16_t, 1> kKickOffsets = {2};
constexpr std::string_view kMannerPointHandler = "clif_parse_GMReqNoChat";
constexpr std::array<std::uint16_t, 3> kMannerPointOffsets = {2, 6, 7};
constexpr std::string_view kFullStripHandler = "clif_parse_GMFullStrip";
constexpr std::array<std::uint16_t, 1> kFullStripOffsets = {2};

core::Error LayoutDrift() {
  return core::Error{ErrorCode::kUnsupportedVersion,
                     "la disposition écrite pour cet ordre du staff ne correspond plus à ce "
                     "que le serveur déclare lire"};
}

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const KickRequest& request) {
  if (!FixedLayoutHolds<KickRequest>(kKickHandler, kKickOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(KickRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(KickRequest::kOpcode);
  writer.U32(request.account_id);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const MannerPointRequest& request) {
  if (!FixedLayoutHolds<MannerPointRequest>(kMannerPointHandler, kMannerPointOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(MannerPointRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(MannerPointRequest::kOpcode);
  writer.U32(request.account_id);
  writer.U8(request.kind);
  writer.U16(request.minutes);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const FullStripRequest& request) {
  if (!FixedLayoutHolds<FullStripRequest>(kFullStripHandler, kFullStripOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(FullStripRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(FullStripRequest::kOpcode);
  writer.U32(request.account_id);
  return frame;
}

}  // namespace ro::proto

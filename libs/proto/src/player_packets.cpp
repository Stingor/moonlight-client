#include "ro/proto/player_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_writer.h"

#include "layout_holds.h"

namespace ro::proto {
namespace {

using core::ByteWriter;
using core::ErrorCode;

constexpr std::string_view kViewEquipHandler = "clif_parse_ViewPlayerEquip";
constexpr std::array<std::uint16_t, 1> kViewEquipOffsets = {2};

core::Error LayoutDrift() {
  return core::Error{ErrorCode::kUnsupportedVersion,
                     "la disposition écrite pour ce geste sur un joueur ne correspond plus à ce "
                     "que le serveur déclare lire"};
}

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const ViewEquipRequest& request) {
  if (!FixedLayoutHolds<ViewEquipRequest>(kViewEquipHandler, kViewEquipOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(ViewEquipRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(ViewEquipRequest::kOpcode);
  writer.U32(request.account_id);
  return frame;
}

}  // namespace ro::proto

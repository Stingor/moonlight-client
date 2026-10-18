#include "ro/proto/craft_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_writer.h"

#include "layout_holds.h"

namespace ro::proto {
namespace {

using core::ByteWriter;
using core::ErrorCode;

constexpr std::string_view kWeaponRefineHandler = "clif_parse_WeaponRefine";
constexpr std::array<std::uint16_t, 1> kWeaponRefineOffsets = {2};

core::Error LayoutDrift() {
  return core::Error{ErrorCode::kUnsupportedVersion,
                     "la disposition écrite pour ce geste d'atelier ne correspond plus à ce que "
                     "le serveur déclare lire"};
}

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const WeaponRefineRequest& request) {
  if (!FixedLayoutHolds<WeaponRefineRequest>(kWeaponRefineHandler, kWeaponRefineOffsets)) {
    return std::unexpected(LayoutDrift());
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(WeaponRefineRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(WeaponRefineRequest::kOpcode);
  writer.I32(request.index);
  return frame;
}

}  // namespace ro::proto

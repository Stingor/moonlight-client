#include "ro/proto/vending_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_writer.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

#include "layout_holds.h"
#include "opcode_only.h"

namespace ro::proto {
namespace {

using core::ByteWriter;
using core::ErrorCode;

constexpr std::string_view kOpenHandler = "clif_parse_OpenVending";
// La longueur, le titre, l'octet d'ouverture, puis la première ligne.
constexpr std::array<std::uint16_t, 4> kOpenOffsets = {2, 4, 84, 85};
constexpr std::string_view kCloseHandler = "clif_parse_CloseVending";
constexpr std::string_view kListHandler = "clif_parse_VendingListReq";
// Le vendeur, juste après l'opcode.
constexpr std::array<std::uint16_t, 1> kListOffsets = {2};

bool OpenLayoutHolds() {
  const auto length = PacketLength(VendingOpenRequest::kOpcode);
  return MatchesServerLayout(VendingOpenRequest::kOpcode, kOpenHandler, kOpenOffsets) &&
         length.has_value() && *length == kVariableLength &&
         kOpenOffsets.back() == VendingOpenRequest::kHeaderSize;
}

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const VendingOpenRequest& request) {
  if (!OpenLayoutHolds()) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la disposition écrite pour l'ouverture d'échoppe ne correspond plus à ce "
                      "que le serveur déclare lire");
  }
  if (request.title_wire.size() >= VendingOpenRequest::kTitleBytes) {
    return core::Fail(ErrorCode::kInvalidArgument,
                      "titre d'échoppe trop long pour ses quatre-vingts octets");
  }
  const std::size_t total =
      VendingOpenRequest::kHeaderSize + request.lines.size() * VendingOpenRequest::kLineSize;
  if (total > UINT16_MAX) {
    return core::Fail(ErrorCode::kInvalidArgument, "échoppe trop longue pour une trame");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(total);
  ByteWriter writer(frame);
  writer.U16(VendingOpenRequest::kOpcode);
  writer.U16(static_cast<std::uint16_t>(total));
  for (std::size_t at = 0; at < VendingOpenRequest::kTitleBytes; ++at) {
    writer.U8(at < request.title_wire.size() ? static_cast<std::uint8_t>(request.title_wire[at])
                                             : std::uint8_t{0});
  }
  writer.U8(request.open ? VendingOpenRequest::kOpen : VendingOpenRequest::kCancel);
  for (const VendingOpenLine& line : request.lines) {
    writer.U16(line.cart_slot);
    writer.U16(line.amount);
    writer.U32(line.price_zeny);
  }
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const VendingCloseRequest& /*request*/) {
  return EncodeOpcodeOnly<VendingCloseRequest>(kCloseHandler, "la fermeture d'échoppe");
}

core::Result<std::vector<std::uint8_t>> Encode(const VendingListRequest& request) {
  if (!FixedLayoutHolds<VendingListRequest>(kListHandler, kListOffsets)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la demande de la liste d'une échoppe ne correspond plus à ce que le "
                      "serveur déclare lire");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(VendingListRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(VendingListRequest::kOpcode);
  writer.U32(request.vendor_aid);
  return frame;
}

}  // namespace ro::proto

#include "ro/proto/deal_packets.h"

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

constexpr std::string_view kRequestHandler = "clif_parse_TradeRequest";
// Le partenaire, juste après l'opcode.
constexpr std::array<std::uint16_t, 1> kRequestOffsets = {2};
constexpr std::string_view kAnswerHandler = "clif_parse_TradeAck";
// La réponse, juste après l'opcode.
constexpr std::array<std::uint16_t, 1> kAnswerOffsets = {2};
constexpr std::string_view kLockHandler = "clif_parse_TradeOk";
constexpr std::string_view kCancelHandler = "clif_parse_TradeCancel";
constexpr std::string_view kExecuteHandler = "clif_parse_TradeCommit";

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const DealRequest& request) {
  if (!FixedLayoutHolds<DealRequest>(kRequestHandler, kRequestOffsets)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la proposition d'échange ne correspond plus à ce que le serveur déclare "
                      "lire");
  }
  if (request.partner_aid == 0) {
    return core::Fail(ErrorCode::kInvalidArgument, "proposition d'échange sans partenaire");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(DealRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(DealRequest::kOpcode);
  writer.U32(request.partner_aid);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const DealAnswer& answer) {
  if (!FixedLayoutHolds<DealAnswer>(kAnswerHandler, kAnswerOffsets)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la réponse à une proposition d'échange ne correspond plus à ce que le "
                      "serveur déclare lire");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(DealAnswer::kWireSize);
  ByteWriter writer(frame);
  writer.U16(DealAnswer::kOpcode);
  writer.U8(answer.accept ? DealAnswer::kAccept : DealAnswer::kRefuse);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const DealLockRequest& /*request*/) {
  return EncodeOpcodeOnly<DealLockRequest>(kLockHandler, "le verrou d'un échange");
}

core::Result<std::vector<std::uint8_t>> Encode(const DealCancelRequest& /*request*/) {
  return EncodeOpcodeOnly<DealCancelRequest>(kCancelHandler, "l'annulation d'un échange");
}

core::Result<std::vector<std::uint8_t>> Encode(const DealExecuteRequest& /*request*/) {
  return EncodeOpcodeOnly<DealExecuteRequest>(kExecuteHandler, "la conclusion d'un échange");
}

}  // namespace ro::proto

#include "ro/proto/deal_packets.h"

#include <array>
#include <string>
#include <string_view>

#include "ro/core/byte_writer.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

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

template <typename Request, typename Offsets>
bool FixedLayoutHolds(std::string_view handler, const Offsets& offsets) {
  const auto length = PacketLength(Request::kOpcode);
  return MatchesServerLayout(Request::kOpcode, handler, offsets) && length.has_value() &&
         static_cast<std::size_t>(*length) == Request::kWireSize;
}

template <typename Request>
core::Result<std::vector<std::uint8_t>> EncodeOpcodeOnly(std::string_view handler,
                                                         std::string_view what) {
  if (!FixedLayoutHolds<Request>(handler, std::array<std::uint16_t, 0>{})) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      std::string(what) + " ne correspond plus à ce que le serveur déclare lire");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(Request::kWireSize);
  ByteWriter writer(frame);
  writer.U16(Request::kOpcode);
  return frame;
}

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

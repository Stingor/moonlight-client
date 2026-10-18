#include "ro/proto/npc_shop_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/proto/packet_lengths.h"

#include "layout_holds.h"
#include "opcode_only.h"

namespace ro::proto {
namespace {

using core::ByteReader;
using core::ByteWriter;
using core::ErrorCode;

constexpr std::string_view kBuyHandler = "clif_parse_NpcBuyListSend";
// La longueur, puis la première ligne.
constexpr std::array<std::uint16_t, 2> kBuyOffsets = {2, 4};
constexpr std::string_view kPointBuyHandler = "clif_parse_npccashshop_buy";
// La longueur, les points, le compte, puis la première ligne.
constexpr std::array<std::uint16_t, 4> kPointBuyOffsets = {2, 4, 8, 10};
constexpr std::string_view kTradeQuitHandler = "clif_parse_NPCShopClosed";
constexpr std::string_view kMarketCloseHandler = "clif_parse_NPCMarketClosed";

template <typename Reply>
bool ReplyLengthHolds() {
  const auto length = PacketLength(Reply::kOpcode);
  return length.has_value() && static_cast<std::size_t>(*length) == Reply::kWireSize;
}

core::Result<void> LinesFit(std::size_t line_count, std::size_t header_size,
                            std::size_t line_size) {
  if (line_count == 0) {
    return core::Fail(ErrorCode::kInvalidArgument, "liste d'achat vide : rien à envoyer");
  }
  if (header_size + line_count * line_size > kTradeFrameMostBytes) {
    return core::Fail(ErrorCode::kInvalidArgument,
                      "liste d'achat trop longue pour une trame de commerce");
  }
  return {};
}

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const NpcBuyRequest& request) {
  if (!VariableLayoutHolds<NpcBuyRequest>(kBuyHandler, kBuyOffsets)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la liste d'achat à un PNJ ne correspond plus à ce que le serveur déclare "
                      "lire");
  }
  if (auto fits =
          LinesFit(request.lines.size(), NpcBuyRequest::kHeaderSize, NpcBuyRequest::kLineSize);
      !fits) {
    return std::unexpected(fits.error());
  }
  const std::size_t length =
      NpcBuyRequest::kHeaderSize + request.lines.size() * NpcBuyRequest::kLineSize;
  std::vector<std::uint8_t> frame;
  frame.reserve(length);
  ByteWriter writer(frame);
  writer.U16(NpcBuyRequest::kOpcode);
  writer.U16(static_cast<std::uint16_t>(length));
  for (const NpcBuyLine& line : request.lines) {
    writer.U16(line.amount);
    writer.U32(line.item_id);
  }
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const PointShopBuyRequest& request) {
  if (!VariableLayoutHolds<PointShopBuyRequest>(kPointBuyHandler, kPointBuyOffsets)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "l'achat à une boutique à points ne correspond plus à ce que le serveur "
                      "déclare lire");
  }
  if (auto fits = LinesFit(request.lines.size(), PointShopBuyRequest::kHeaderSize,
                           PointShopBuyRequest::kLineSize);
      !fits) {
    return std::unexpected(fits.error());
  }
  const std::size_t length =
      PointShopBuyRequest::kHeaderSize + request.lines.size() * PointShopBuyRequest::kLineSize;
  std::vector<std::uint8_t> frame;
  frame.reserve(length);
  ByteWriter writer(frame);
  writer.U16(PointShopBuyRequest::kOpcode);
  writer.U16(static_cast<std::uint16_t>(length));
  writer.U32(request.secondary_points);
  writer.U16(static_cast<std::uint16_t>(request.lines.size()));
  for (const NpcBuyLine& line : request.lines) {
    writer.U16(line.amount);
    writer.U32(line.item_id);
  }
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const NpcTradeQuit& /*quit*/) {
  return EncodeOpcodeOnly<NpcTradeQuit>(kTradeQuitHandler, "la fermeture de la boutique");
}

core::Result<std::vector<std::uint8_t>> Encode(const NpcMarketClose& /*close*/) {
  return EncodeOpcodeOnly<NpcMarketClose>(kMarketCloseHandler, "la fermeture du market");
}

core::Result<NpcSellResult> DecodeNpcSellResult(std::span<const std::uint8_t> frame) {
  if (!ReplyLengthHolds<NpcSellResult>()) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "le résultat de vente n'a plus la longueur que le serveur déclare");
  }
  ByteReader reader(frame);
  if (reader.U16() != NpcSellResult::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature, "trame d'un autre paquet que le résultat de vente");
  }
  NpcSellResult result;
  result.result = reader.U8();
  if (!reader.ok() || frame.size() != NpcSellResult::kWireSize) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "résultat de vente tronqué");
  }
  return result;
}

core::Result<PointShopUpdate> DecodePointShopUpdate(std::span<const std::uint8_t> frame) {
  if (!ReplyLengthHolds<PointShopUpdate>()) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "le résultat d'achat à points n'a plus la longueur que le serveur déclare");
  }
  ByteReader reader(frame);
  if (reader.U16() != PointShopUpdate::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature,
                      "trame d'un autre paquet que le résultat d'achat à points");
  }
  PointShopUpdate update;
  update.points = reader.U32();
  update.secondary_points = reader.U32();
  update.result = reader.U16();
  if (!reader.ok() || frame.size() != PointShopUpdate::kWireSize) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "résultat d'achat à points tronqué");
  }
  return update;
}

std::vector<std::uint8_t> EncodeNpcSellResult(const NpcSellResult& result) {
  std::vector<std::uint8_t> frame;
  ByteWriter writer(frame);
  writer.U16(NpcSellResult::kOpcode);
  writer.U8(result.result);
  return frame;
}

std::vector<std::uint8_t> EncodePointShopUpdate(const PointShopUpdate& update) {
  std::vector<std::uint8_t> frame;
  ByteWriter writer(frame);
  writer.U16(PointShopUpdate::kOpcode);
  writer.U32(update.points);
  writer.U32(update.secondary_points);
  writer.U16(update.result);
  return frame;
}

}  // namespace ro::proto

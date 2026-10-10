#include "ro/proto/buying_store_packets.h"

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

constexpr std::string_view kOpenHandler = "clif_parse_ReqOpenBuyingStore";
// La longueur, la limite de zeny, l'ouverture, le titre, puis les lignes.
constexpr std::array<std::uint16_t, 5> kOpenOffsets = {2, 4, 8, 9, 89};
constexpr std::string_view kCloseHandler = "clif_parse_ReqCloseBuyingStore";
constexpr std::string_view kVisitHandler = "clif_parse_ReqClickBuyingStore";
// Le compte de l'acheteur, juste après l'opcode.
constexpr std::array<std::uint16_t, 1> kVisitOffsets = {2};
constexpr std::string_view kTradeHandler = "clif_parse_ReqTradeBuyingStore";
// La longueur, le compte de l'acheteur, le numéro de l'échoppe, puis les lignes.
constexpr std::array<std::uint16_t, 4> kTradeOffsets = {2, 4, 8, 12};

// La plus longue trame qu'une longueur sur deux octets puisse annoncer.
constexpr std::size_t kMostVariableFrameBytes = 0xFFFF;

// Une demande de taille FIXE : le gestionnaire, les décalages et la longueur
// que la table du serveur déclare doivent tous concorder.
template <typename Request, typename Offsets>
bool FixedLayoutHolds(std::string_view handler, const Offsets& offsets) {
  const auto length = PacketLength(Request::kOpcode);
  return MatchesServerLayout(Request::kOpcode, handler, offsets) && length.has_value() &&
         static_cast<std::size_t>(*length) == Request::kWireSize;
}

// Une demande de taille VARIABLE : la table doit la déclarer comme telle.
template <typename Request, typename Offsets>
bool VariableLayoutHolds(std::string_view handler, const Offsets& offsets) {
  const auto length = PacketLength(Request::kOpcode);
  return MatchesServerLayout(Request::kOpcode, handler, offsets) && length.has_value() &&
         *length == kVariableLength;
}

// Ouvre la lecture d'un paquet reçu de taille fixe : opcode et taille exacts.
template <typename Packet>
core::Result<ByteReader> OpenFixed(std::span<const std::uint8_t> frame, std::string_view what) {
  ByteReader reader(frame);
  if (reader.U16() != Packet::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature,
                      std::string("trame d'un autre paquet que ") + std::string(what));
  }
  if (frame.size() != Packet::kWireSize) {
    return core::Fail(ErrorCode::kUnexpectedEnd,
                      std::string(what) + " : la trame n'a pas la taille du paquet");
  }
  return reader;
}

}  // namespace

core::Result<BuyingStorePreparation> DecodeBuyingStorePreparation(
    std::span<const std::uint8_t> frame) {
  auto reader = OpenFixed<BuyingStorePreparation>(frame, "la préparation d'échoppe d'achat");
  if (!reader) {
    return std::unexpected(reader.error());
  }
  BuyingStorePreparation packet;
  packet.slots = reader->U8();
  return packet;
}

core::Result<BuyingStoreOpenFailure> DecodeBuyingStoreOpenFailure(
    std::span<const std::uint8_t> frame) {
  auto reader = OpenFixed<BuyingStoreOpenFailure>(frame, "le refus d'échoppe d'achat");
  if (!reader) {
    return std::unexpected(reader.error());
  }
  BuyingStoreOpenFailure packet;
  packet.result = reader->U16();
  packet.weight = reader->U32();
  return packet;
}

core::Result<BuyingStoreEnd> DecodeBuyingStoreEnd(std::span<const std::uint8_t> frame) {
  auto reader = OpenFixed<BuyingStoreEnd>(frame, "la fin d'échoppe d'achat");
  if (!reader) {
    return std::unexpected(reader.error());
  }
  BuyingStoreEnd packet;
  packet.result = reader->U16();
  return packet;
}

core::Result<BuyingStoreItemSold> DecodeBuyingStoreItemSold(std::span<const std::uint8_t> frame) {
  auto reader = OpenFixed<BuyingStoreItemSold>(frame, "la vente à une échoppe d'achat");
  if (!reader) {
    return std::unexpected(reader.error());
  }
  BuyingStoreItemSold packet;
  packet.client_slot = reader->U16();
  packet.amount = reader->U16();
  packet.unit_price = reader->U32();
  return packet;
}

core::Result<std::vector<std::uint8_t>> Encode(const BuyingStoreOpenRequest& request) {
  if (!VariableLayoutHolds<BuyingStoreOpenRequest>(kOpenHandler, kOpenOffsets)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la demande d'ouverture d'échoppe d'achat ne correspond plus à ce que le "
                      "serveur déclare lire");
  }
  if (request.title_on_wire.size() >= BuyingStoreOpenRequest::kTitleSize) {
    return core::Fail(ErrorCode::kInvalidArgument,
                      "titre d'échoppe d'achat trop long pour ce que le serveur en garde");
  }
  const std::size_t total =
      BuyingStoreOpenRequest::kHeaderSize + request.lines.size() * BuyingStoreOpenLine::kWireSize;
  if (total > kMostVariableFrameBytes) {
    return core::Fail(ErrorCode::kInvalidArgument,
                      "demande d'ouverture d'échoppe d'achat trop longue pour une trame");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(total);
  ByteWriter writer(frame);
  writer.U16(BuyingStoreOpenRequest::kOpcode);
  writer.U16(static_cast<std::uint16_t>(total));
  writer.U32(request.zeny_limit);
  writer.U8(request.open ? 1 : 0);
  writer.FixedString(request.title_on_wire, BuyingStoreOpenRequest::kTitleSize);
  for (const BuyingStoreOpenLine& line : request.lines) {
    writer.U32(line.item_id);
    writer.U16(line.amount);
    writer.U32(line.price);
  }
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const BuyingStoreCloseRequest& /*request*/) {
  if (!FixedLayoutHolds<BuyingStoreCloseRequest>(kCloseHandler, std::array<std::uint16_t, 0>{})) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la fermeture d'échoppe d'achat ne correspond plus à ce que le serveur "
                      "déclare lire");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(BuyingStoreCloseRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(BuyingStoreCloseRequest::kOpcode);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const BuyingStoreVisitRequest& request) {
  if (!FixedLayoutHolds<BuyingStoreVisitRequest>(kVisitHandler, kVisitOffsets)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la demande de liste d'une échoppe d'achat ne correspond plus à ce que le "
                      "serveur déclare lire");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(BuyingStoreVisitRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(BuyingStoreVisitRequest::kOpcode);
  writer.U32(request.buyer_account_id);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const BuyingStoreTradeRequest& request) {
  if (!VariableLayoutHolds<BuyingStoreTradeRequest>(kTradeHandler, kTradeOffsets)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la vente à une échoppe d'achat ne correspond plus à ce que le serveur "
                      "déclare lire");
  }
  const std::size_t total =
      BuyingStoreTradeRequest::kHeaderSize + request.lines.size() * BuyingStoreTradeLine::kWireSize;
  if (total > BuyingStoreTradeRequest::kMostFrameBytes) {
    return core::Fail(ErrorCode::kInvalidArgument,
                      "vente à une échoppe d'achat trop longue pour une trame");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(total);
  ByteWriter writer(frame);
  writer.U16(BuyingStoreTradeRequest::kOpcode);
  writer.U16(static_cast<std::uint16_t>(total));
  writer.U32(request.buyer_account_id);
  writer.U32(request.store_id);
  for (const BuyingStoreTradeLine& line : request.lines) {
    writer.U16(line.client_slot);
    writer.U32(line.item_id);
    writer.U16(line.amount);
  }
  return frame;
}

}  // namespace ro::proto

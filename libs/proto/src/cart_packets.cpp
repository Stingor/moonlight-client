#include "ro/proto/cart_packets.h"

#include <array>
#include <string_view>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

#include "layout_holds.h"

namespace ro::proto {
namespace {

using core::ByteReader;
using core::ByteWriter;
using core::ErrorCode;

constexpr std::string_view kChangeCartHandler = "clif_parse_ChangeCart";
// Le type, sur deux octets, juste après l'opcode.
constexpr std::array<std::uint16_t, 1> kChangeCartOffsets = {2};
constexpr std::string_view kSelectCartHandler = "clif_parse_SelectCart";
// L'identité, puis le type sur un octet.
constexpr std::array<std::uint16_t, 2> kSelectCartOffsets = {2, 6};

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const ChangeCartRequest& request) {
  if (!FixedLayoutHolds<ChangeCartRequest>(kChangeCartHandler, kChangeCartOffsets)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la disposition écrite pour le changement de charrette ne correspond plus "
                      "à ce que le serveur déclare lire");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(ChangeCartRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(ChangeCartRequest::kOpcode);
  writer.U16(request.cart_type);
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const SelectCartRequest& request) {
  if (!FixedLayoutHolds<SelectCartRequest>(kSelectCartHandler, kSelectCartOffsets)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la disposition écrite pour le choix de charrette ne correspond plus à ce "
                      "que le serveur déclare lire");
  }
  std::vector<std::uint8_t> frame;
  frame.reserve(SelectCartRequest::kWireSize);
  ByteWriter writer(frame);
  writer.U16(SelectCartRequest::kOpcode);
  writer.U32(request.identity);
  writer.U8(request.cart_type);
  return frame;
}

core::Result<SelectCartOffer> DecodeSelectCartOffer(std::span<const std::uint8_t> frame) {
  ByteReader reader(frame);
  if (reader.U16() != SelectCartOffer::kOpcode) {
    return core::Fail(ErrorCode::kBadSignature,
                      "trame d'un autre paquet que la liste des charrettes");
  }
  const std::uint16_t length = reader.U16();
  SelectCartOffer offer;
  offer.identity = reader.U32();
  if (!reader.ok() || length != frame.size() || length < SelectCartOffer::kHeaderSize) {
    return core::Fail(ErrorCode::kUnexpectedEnd, "liste des charrettes tronquée");
  }
  const std::span<const std::uint8_t> types = frame.subspan(SelectCartOffer::kHeaderSize);
  offer.cart_types.assign(types.begin(), types.end());
  return offer;
}

std::vector<std::uint8_t> EncodeSelectCartOffer(const SelectCartOffer& offer) {
  const std::size_t total = SelectCartOffer::kHeaderSize + offer.cart_types.size();
  std::vector<std::uint8_t> frame;
  frame.reserve(total);
  ByteWriter writer(frame);
  writer.U16(SelectCartOffer::kOpcode);
  writer.U16(static_cast<std::uint16_t>(total));
  writer.U32(offer.identity);
  for (const std::uint8_t type : offer.cart_types) {
    writer.U8(type);
  }
  return frame;
}

}  // namespace ro::proto

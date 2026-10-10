#include "ro/proto/cash_shop_packets.h"

#include <string_view>

#include "ro/core/byte_writer.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

namespace ro::proto {
namespace {

using core::ByteWriter;
using core::ErrorCode;

constexpr std::string_view kCatalogueHandler = "clif_parse_cashshop_list_request";
constexpr std::string_view kCloseHandler = "clif_parse_cashshop_close";

// Aucun champ, donc aucun décalage à recouper : seuls le gestionnaire et la
// longueur peuvent mentir.
template <typename Request>
bool LayoutHolds(std::string_view handler) {
  const auto length = PacketLength(Request::kOpcode);
  return MatchesServerLayout(Request::kOpcode, handler, {}) && length.has_value() &&
         static_cast<std::size_t>(*length) == Request::kWireSize;
}

template <typename Request>
std::vector<std::uint8_t> OpcodeOnly() {
  std::vector<std::uint8_t> frame;
  frame.reserve(Request::kWireSize);
  ByteWriter writer(frame);
  writer.U16(Request::kOpcode);
  return frame;
}

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const CashShopCatalogueRequest& /*request*/) {
  if (!LayoutHolds<CashShopCatalogueRequest>(kCatalogueHandler)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la demande du catalogue de la boutique ne correspond plus à ce que le "
                      "serveur déclare lire");
  }
  return OpcodeOnly<CashShopCatalogueRequest>();
}

core::Result<std::vector<std::uint8_t>> Encode(const CashShopCloseNotice& /*notice*/) {
  if (!LayoutHolds<CashShopCloseNotice>(kCloseHandler)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la fermeture de la boutique ne correspond plus à ce que le serveur "
                      "déclare lire");
  }
  return OpcodeOnly<CashShopCloseNotice>();
}

}  // namespace ro::proto

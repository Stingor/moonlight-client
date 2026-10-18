#include "ro/proto/cash_shop_packets.h"

#include <string_view>

#include "opcode_only.h"

namespace ro::proto {
namespace {

constexpr std::string_view kCatalogueHandler = "clif_parse_cashshop_list_request";
constexpr std::string_view kCloseHandler = "clif_parse_cashshop_close";

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const CashShopCatalogueRequest& /*request*/) {
  return EncodeOpcodeOnly<CashShopCatalogueRequest>(kCatalogueHandler,
                                                    "la demande du catalogue de la boutique");
}

core::Result<std::vector<std::uint8_t>> Encode(const CashShopCloseNotice& /*notice*/) {
  return EncodeOpcodeOnly<CashShopCloseNotice>(kCloseHandler, "la fermeture de la boutique");
}

}  // namespace ro::proto

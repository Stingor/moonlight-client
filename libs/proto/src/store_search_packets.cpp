#include "ro/proto/store_search_packets.h"

#include <array>
#include <limits>
#include <string_view>

#include "ro/core/byte_writer.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

namespace ro::proto {
namespace {

using core::ByteWriter;
using core::ErrorCode;

constexpr std::string_view kSearchHandler = "clif_parse_SearchStoreInfo";
// La longueur, la sorte, le prix maximal, le prix minimal, les deux comptes,
// puis le premier identifiant.
constexpr std::array<std::uint16_t, 7> kSearchOffsets = {2, 4, 5, 9, 13, 14, 15};
constexpr std::string_view kNextPageHandler = "clif_parse_SearchStoreInfoNextPage";
constexpr std::string_view kCloseHandler = "clif_parse_CloseSearchStoreInfo";

// Le plus grand compte qu'un octet porte.
constexpr std::size_t kMostIdsPerList = std::numeric_limits<std::uint8_t>::max();

template <typename Request>
bool BareLayoutHolds(std::string_view handler) {
  const auto length = PacketLength(Request::kOpcode);
  return MatchesServerLayout(Request::kOpcode, handler, std::array<std::uint16_t, 0>{}) &&
         length.has_value() && static_cast<std::size_t>(*length) == Request::kWireSize;
}

bool SearchLayoutHolds() {
  const auto length = PacketLength(StoreSearchRequest::kOpcode);
  return MatchesServerLayout(StoreSearchRequest::kOpcode, kSearchHandler, kSearchOffsets) &&
         length.has_value() && *length == kVariableLength &&
         kSearchOffsets.back() == StoreSearchRequest::kHeaderSize;
}

template <typename Request>
core::Result<std::vector<std::uint8_t>> EncodeBare() {
  std::vector<std::uint8_t> frame;
  frame.reserve(Request::kWireSize);
  ByteWriter writer(frame);
  writer.U16(Request::kOpcode);
  return frame;
}

}  // namespace

core::Result<std::vector<std::uint8_t>> Encode(const StoreSearchRequest& request) {
  if (!SearchLayoutHolds()) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la disposition écrite pour la recherche d'échoppes ne correspond plus à "
                      "ce que le serveur déclare lire");
  }
  if (request.item_ids.size() > kMostIdsPerList || request.card_ids.size() > kMostIdsPerList) {
    return core::Fail(ErrorCode::kInvalidArgument,
                      "trop d'objets ou de cartes pour le compte d'un octet");
  }
  const std::size_t total =
      StoreSearchRequest::kHeaderSize +
      (request.item_ids.size() + request.card_ids.size()) * StoreSearchRequest::kIdSize;
  std::vector<std::uint8_t> frame;
  frame.reserve(total);
  ByteWriter writer(frame);
  writer.U16(StoreSearchRequest::kOpcode);
  writer.U16(static_cast<std::uint16_t>(total));
  writer.U8(request.search_type);
  writer.U32(request.max_price_zeny);
  writer.U32(request.min_price_zeny);
  writer.U8(static_cast<std::uint8_t>(request.item_ids.size()));
  writer.U8(static_cast<std::uint8_t>(request.card_ids.size()));
  for (const std::uint32_t item_id : request.item_ids) {
    writer.U32(item_id);
  }
  for (const std::uint32_t card_id : request.card_ids) {
    writer.U32(card_id);
  }
  return frame;
}

core::Result<std::vector<std::uint8_t>> Encode(const StoreSearchNextPageRequest& /*request*/) {
  if (!BareLayoutHolds<StoreSearchNextPageRequest>(kNextPageHandler)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la page suivante de la recherche d'échoppes ne correspond plus à ce que "
                      "le serveur déclare lire");
  }
  return EncodeBare<StoreSearchNextPageRequest>();
}

core::Result<std::vector<std::uint8_t>> Encode(const StoreSearchCloseRequest& /*request*/) {
  if (!BareLayoutHolds<StoreSearchCloseRequest>(kCloseHandler)) {
    return core::Fail(ErrorCode::kUnsupportedVersion,
                      "la fermeture de la recherche d'échoppes ne correspond plus à ce que le "
                      "serveur déclare lire");
  }
  return EncodeBare<StoreSearchCloseRequest>();
}

}  // namespace ro::proto

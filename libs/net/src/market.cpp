// Le Marché : la demande et la réponse. Voir l'en-tête.

#include "ro/net/market.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

static_assert(kMarketReplyOpcode == proto::ZcMoonlightMarket::kOpcode);

// Le statut, ou rien quand le fil porte un code inconnu ou réservé.
std::optional<sim::MarketStatus> StatusOf(std::uint8_t wire) {
  switch (wire) {
    case static_cast<std::uint8_t>(sim::MarketStatus::kOk):
      return sim::MarketStatus::kOk;
    case static_cast<std::uint8_t>(sim::MarketStatus::kOpen):
      return sim::MarketStatus::kOpen;
    case static_cast<std::uint8_t>(sim::MarketStatus::kTooSoon):
      return sim::MarketStatus::kTooSoon;
    case static_cast<std::uint8_t>(sim::MarketStatus::kStoreGone):
      return sim::MarketStatus::kStoreGone;
    case static_cast<std::uint8_t>(sim::MarketStatus::kBadRequest):
      return sim::MarketStatus::kBadRequest;
    default:
      break;
  }
  return std::nullopt;
}

std::optional<sim::MarketKind> KindOf(std::uint8_t wire) {
  switch (wire) {
    case static_cast<std::uint8_t>(sim::MarketKind::kVending):
      return sim::MarketKind::kVending;
    case static_cast<std::uint8_t>(sim::MarketKind::kBuying):
      return sim::MarketKind::kBuying;
    case static_cast<std::uint8_t>(sim::MarketKind::kNpc):
      return sim::MarketKind::kNpc;
    default:
      break;
  }
  return std::nullopt;
}

// La dernière monnaie que le fil connaît : une plus grande ne promet rien.
constexpr auto kLastCurrency = sim::MarketCurrency::kBarter;

// L'offre, ou rien quand sa monnaie est inconnue : son prix ne voudrait rien dire.
std::optional<sim::MarketOffer> OfferFrom(const proto::MoonlightMarketEntry& entry) {
  if (entry.currency > static_cast<std::uint8_t>(kLastCurrency)) {
    return std::nullopt;
  }
  sim::MarketOffer offer;
  offer.currency = static_cast<sim::MarketCurrency>(entry.currency);
  offer.currency_item = entry.currency_item;
  offer.currency_name = core::Utf8FromWire(entry.currency_name);
  offer.store_id = entry.store_id;
  offer.owner_aid = entry.owner_aid;
  offer.price_zeny = entry.price;
  offer.autotrade = entry.autotrade != 0;
  offer.item.item_id = entry.itemId;
  offer.item.amount = static_cast<std::int16_t>(
      std::min<int>(entry.amount, std::numeric_limits<std::int16_t>::max()));
  offer.item.refine_level = entry.refine;
  offer.item.grade = entry.grade;
  offer.item.identified = entry.identified != 0;
  offer.item.damaged = entry.damaged != 0;
  offer.item.cards = entry.cards;
  for (std::size_t rank = 0; rank < sim::kItemOptionSlots; ++rank) {
    offer.item.options[rank].id = static_cast<std::uint16_t>(entry.options[rank].index);
    offer.item.options[rank].value = entry.options[rank].value;
    offer.item.options[rank].param = entry.options[rank].param;
  }
  offer.map_name = core::Utf8FromWire(entry.map);
  offer.cell_x = entry.x;
  offer.cell_y = entry.y;
  // Le serveur écrit le titre dans la page de codes du jeu.
  offer.shop_name = core::Utf8FromWire(entry.title);
  return offer;
}

}  // namespace

core::Result<void> RequestMarket(Connection& link, sim::Market& market, sim::MarketQuery query) {
  if (query.item_ids.size() > sim::kMarketMostIds) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "trop d'objets pour une demande au Marché");
  }
  query.query_id = market.NextQueryId();
  proto::CzMoonlightMarketReq request;
  request.query_id = query.query_id;
  request.kind = static_cast<std::uint8_t>(query.kind);
  request.category = static_cast<std::uint8_t>(query.category);
  request.refine_min = query.refine_min;
  request.refine_max = query.refine_max;
  request.flags = query.flags;
  request.price_min = query.price_min_zeny;
  request.price_max = query.price_max_zeny;
  request.store_id = query.store_id;
  request.owner_aid = query.owner_aid;
  request.items.reserve(query.item_ids.size());
  for (const std::uint32_t item_id : query.item_ids) {
    request.items.push_back(proto::MoonlightMarketItemId{.itemId = item_id});
  }
  const std::vector<std::uint8_t> frame = proto::Encode(request);
  if (frame.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "demande au Marché trop longue");
  }
  link.Send(frame);
  market.NoteSent(query);
  return {};
}

std::optional<sim::MarketReplyPart> DecodeMarketReply(std::span<const std::uint8_t> frame) {
  proto::ZcMoonlightMarket packet;
  if (!proto::Decode(frame, packet)) {
    return std::nullopt;
  }
  const std::optional<sim::MarketStatus> status = StatusOf(packet.status);
  const std::optional<sim::MarketKind> kind = KindOf(packet.kind);
  if (!status.has_value() || !kind.has_value()) {
    return std::nullopt;
  }
  sim::MarketReplyPart part{.query_id = packet.query_id,
                            .status = *status,
                            .kind = *kind,
                            .part = packet.part,
                            .part_count = packet.part_count,
                            .total = packet.total,
                            .truncated = packet.truncated != 0,
                            .store_id = packet.store_id,
                            .owner_aid = packet.owner_aid,
                            .query = core::Utf8FromWire(packet.query),
                            .offers = {}};
  part.offers.reserve(packet.entries.size());
  for (const proto::MoonlightMarketEntry& entry : packet.entries) {
    if (std::optional<sim::MarketOffer> offer = OfferFrom(entry); offer.has_value()) {
      part.offers.push_back(std::move(*offer));
    }
  }
  return part;
}

FrameOutcome ApplyMarketFrame(const Frame& frame, sim::Market& market) {
  if (frame.opcode != proto::ZcMoonlightMarket::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  std::optional<sim::MarketReplyPart> part = DecodeMarketReply(frame.bytes);
  if (!part.has_value()) {
    return FrameOutcome::kUndecodable;
  }
  market.Tell(std::move(*part));
  return FrameOutcome::kApplied;
}

}  // namespace ro::net

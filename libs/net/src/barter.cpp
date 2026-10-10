// Le troc. Voir l'en-tête pour le résultat partagé avec la boutique à zeny, et
// pour les changements de carte, lus sans être consommés.

#include "ro/net/barter.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "ro/proto/barter_packets.h"
#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

// LE PLAFOND DU MODÈLE EST CELUI DU FIL : ses tailles de ligne sont recoupées
// avec les structures engendrées depuis le serveur.
static_assert(proto::CzNpcBarterMarketPurchase::kWireSize == sim::kBarterPurchaseHeaderBytes);
static_assert(proto::CzNpcBarterMarketPurchaseSub::kWireSize == sim::kBarterSimpleLineBytes);
static_assert(proto::CzNpcExpandedBarterMarketPurchase::kWireSize ==
              sim::kBarterPurchaseHeaderBytes);
static_assert(proto::CzNpcExpandedBarterMarketPurchaseSub::kWireSize ==
              sim::kBarterExpandedLineBytes);

// Ce que le troc simple envoie comme index d'inventaire : le serveur ne le lit
// pas.
constexpr std::uint16_t kUnreadInventoryIndex = 0;

[[nodiscard]] std::vector<sim::BarterItem> SimpleItems(
    const std::vector<proto::ZcNpcBarterMarketIteminfoSub>& listed) {
  std::vector<sim::BarterItem> items;
  items.reserve(listed.size());
  for (const proto::ZcNpcBarterMarketIteminfoSub& entry : listed) {
    sim::BarterItem item{.item_id = entry.nameid,
                         .type = entry.type,
                         .stock = entry.amount,
                         .weight = entry.weight,
                         .shop_index = entry.index,
                         .view_sprite = entry.viewSprite,
                         .equip_location = entry.location};
    item.currencies.push_back(
        sim::BarterCurrency{.item_id = entry.currencyNameid, .amount = entry.currencyAmount});
    items.push_back(std::move(item));
  }
  return items;
}

[[nodiscard]] std::vector<sim::BarterItem> ExpandedItems(
    const std::vector<proto::ExpandedBarterItem>& listed) {
  std::vector<sim::BarterItem> items;
  items.reserve(listed.size());
  for (const proto::ExpandedBarterItem& entry : listed) {
    sim::BarterItem item{.item_id = entry.item_id,
                         .type = entry.type,
                         .stock = entry.stock,
                         .weight = entry.weight,
                         .shop_index = entry.shop_index,
                         .view_sprite = entry.view_sprite,
                         .equip_location = entry.location,
                         .zeny = entry.zeny,
                         .refine_level = entry.refine_level};
    item.currencies.reserve(entry.currencies.size());
    for (const proto::ExpandedBarterCurrency& currency : entry.currencies) {
      item.currencies.push_back(sim::BarterCurrency{.item_id = currency.item_id,
                                                    .refine_level = currency.refine_level,
                                                    .amount = currency.amount,
                                                    .type = currency.type});
    }
    items.push_back(std::move(item));
  }
  return items;
}

[[nodiscard]] std::vector<std::uint8_t> EncodePurchase(const sim::Barter& barter) {
  if (barter.generation() == sim::BarterGeneration::kSimple) {
    proto::CzNpcBarterMarketPurchase request;
    for (const sim::BarterCartLine& line : barter.cart()) {
      request.list.push_back(
          proto::CzNpcBarterMarketPurchaseSub{.itemId = barter.FindItem(line.shop_index)->item_id,
                                              .amount = line.amount,
                                              .invIndex = kUnreadInventoryIndex,
                                              .shopIndex = line.shop_index});
    }
    return proto::Encode(request);
  }
  proto::CzNpcExpandedBarterMarketPurchase request;
  for (const sim::BarterCartLine& line : barter.cart()) {
    request.list.push_back(proto::CzNpcExpandedBarterMarketPurchaseSub{
        .itemId = barter.FindItem(line.shop_index)->item_id,
        .shopIndex = line.shop_index,
        .amount = line.amount});
  }
  return proto::Encode(request);
}

}  // namespace

FrameOutcome ApplyBarterFrame(const Frame& frame, sim::Barter& barter) {
  switch (frame.opcode) {
    case proto::ZcNpcBarterMarketIteminfo::kOpcode: {
      proto::ZcNpcBarterMarketIteminfo packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      barter.TellOpening(sim::BarterGeneration::kSimple, SimpleItems(packet.list));
      return FrameOutcome::kApplied;
    }

    case proto::ExpandedBarterItemList::kOpcode: {
      const core::Result<proto::ExpandedBarterItemList> list =
          proto::DecodeExpandedBarterItemList(frame.bytes);
      if (!list) {
        return FrameOutcome::kUndecodable;
      }
      barter.TellOpening(sim::BarterGeneration::kExpanded, ExpandedItems(list->items));
      return FrameOutcome::kApplied;
    }

    case proto::ZcPcPurchaseResult::kOpcode: {
      if (!barter.awaiting_result()) {
        return FrameOutcome::kUnhandled;
      }
      proto::ZcPcPurchaseResult packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      barter.TellResult(packet.result);
      return FrameOutcome::kApplied;
    }

    case proto::ZcNpcackMapmove::kOpcode:
      barter.CloseForMapChange();
      return FrameOutcome::kUnhandled;

    case proto::ZcNpcackServermove::kOpcode:
      barter.CloseSilently();
      return FrameOutcome::kUnhandled;

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestBarterPurchase(Connection& link, sim::Barter& barter) {
  const sim::BarterCartCheck shape = sim::CheckBarterCartShape(barter);
  if (!shape.passed()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "panier de troc que le serveur ignorerait sans répondre");
  }
  const std::vector<std::uint8_t> frame = EncodePurchase(barter);
  if (frame.empty() || frame.size() > sim::kBarterMostPurchaseBytes) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "achat de troc trop long pour une trame");
  }
  link.Send(frame);
  barter.MarkPurchaseSent();
  return {};
}

core::Result<void> SayBarterClosed(Connection& link, sim::BarterGeneration generation) {
  if (generation == sim::BarterGeneration::kSimple) {
    link.Send(proto::Encode(proto::CzNpcBarterMarketClose{}));
  } else {
    link.Send(proto::Encode(proto::CzNpcExpandedBarterMarketClose{}));
  }
  return {};
}

core::Result<void> SayBarterFarewells(Connection& link, sim::Barter& barter) {
  for (std::optional<sim::BarterGeneration> due = barter.TakeFarewell(); due.has_value();
       due = barter.TakeFarewell()) {
    if (auto said = SayBarterClosed(link, *due); !said) {
      return said;
    }
  }
  return {};
}

}  // namespace ro::net

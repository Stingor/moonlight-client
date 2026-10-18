// La boutique de vote. Voir l'en-tête pour la bascule et pour les changements de
// carte, lus sans être consommés.

#include "ro/net/cash_shop.h"

#include <vector>

#include "ro/proto/cash_shop_packets.h"
#include "ro/proto/hat_effect_packets.h"
#include "ro/proto/map_packets.h"

#include "send_encoded.h"

namespace ro::net {

FrameOutcome ApplyCashShopFrame(const Frame& frame, sim::CashShop& shop) {
  switch (frame.opcode) {
    case proto::ZcSeCashshopOpen::kOpcode: {
      proto::ZcSeCashshopOpen packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      shop.TellOpening(packet.cashPoints, packet.kafraPoints, packet.tab);
      return FrameOutcome::kApplied;
    }

    case proto::ZcAckSchedulerCashitem::kOpcode: {
      proto::ZcAckSchedulerCashitem packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // ⚠ LE NOMBRE D'ITEMS SE LIT DANS LA LONGUEUR, pas dans le champ `count` :
      // c'est la longueur qui dit ce que la trame porte vraiment.
      std::vector<sim::CashShopItem> items;
      items.reserve(packet.items.size());
      for (const proto::ZcAckSchedulerCashitemSub& listed : packet.items) {
        items.push_back(sim::CashShopItem{.item_id = listed.itemId,
                                          .price = static_cast<std::int32_t>(listed.price),
                                          .view_sprite = listed.viewSprite,
                                          .equip_location = listed.location});
      }
      shop.TellCatalogue(static_cast<std::uint16_t>(packet.tabNum), items);
      return FrameOutcome::kApplied;
    }

    case proto::ZcSePcBuyCashitemResult::kOpcode: {
      proto::ZcSePcBuyCashitemResult packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      shop.TellPurchase(packet.result, packet.cashPoints, packet.kafraPoints);
      return FrameOutcome::kApplied;
    }

    case proto::ZcNpcackMapmove::kOpcode:
      shop.CloseForMapChange();
      return FrameOutcome::kUnhandled;

    case proto::ZcNpcackServermove::kOpcode:
      shop.CloseSilently();
      return FrameOutcome::kUnhandled;

    default:
      return FrameOutcome::kUnhandled;
  }
}

FrameOutcome ApplyHatEffectItemsFrame(const Frame& frame, sim::HatEffectItems& items) {
  if (frame.opcode != proto::HatEffectMap::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  const core::Result<proto::HatEffectMap> map = proto::DecodeHatEffectMap(frame.bytes);
  if (!map) {
    return FrameOutcome::kUndecodable;
  }
  items.Clear();
  for (const proto::HatEffectMapEntry& entry : map->entries) {
    items.Tell(entry.item_id, entry.ordinal);
  }
  return FrameOutcome::kApplied;
}

core::Result<void> RequestCashShopOpen(Connection& link, std::uint32_t tab) {
  proto::CzSeCashshopOpen2 request;
  request.tab = tab;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestCashShopCatalogue(Connection& link) {
  return SendEncoded(link, proto::Encode(proto::CashShopCatalogueRequest{}));
}

core::Result<void> SayCashShopClosed(Connection& link) {
  return SendEncoded(link, proto::Encode(proto::CashShopCloseNotice{}));
}

core::Result<void> RequestCashShopPurchase(Connection& link, std::uint32_t event_points,
                                           std::span<const CashShopPurchaseLine> lines) {
  proto::CzSePcBuyCashitemList request;
  request.count = static_cast<std::uint16_t>(lines.size());
  request.kafraPoints = event_points;
  request.items.reserve(lines.size());
  for (const CashShopPurchaseLine& line : lines) {
    request.items.push_back(proto::CzSePcBuyCashitemListSub{
        .itemId = line.item_id, .amount = line.amount, .tab = line.tab});
  }
  const std::vector<std::uint8_t> frame = proto::Encode(request);
  if (frame.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "achat de la boutique trop long pour une trame");
  }
  link.Send(frame);
  return {};
}

}  // namespace ro::net

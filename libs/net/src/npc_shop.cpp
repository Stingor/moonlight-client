// La boutique d'un PNJ. Voir l'en-tête pour le réarmement, et pour le résultat
// d'achat partagé avec le troc.

#include "ro/net/npc_shop.h"

#include <cstdint>
#include <vector>

#include "ro/proto/map_packets.h"
#include "ro/proto/npc_shop_packets.h"

namespace ro::net {
namespace {

using core::ErrorCode;

// Le stock que le market écrit pour une ligne sans borne : −1 sur trente-deux
// bits non signés.
constexpr std::uint32_t kMarketUnlimitedOnWire = 0xFFFFFFFF;

// Le type d'échange qu'attend le choix : zéro achat, un vente
// (`clif_parse_NpcBuySellSelected`).
constexpr std::uint8_t kDealTypeBuy = 0;
constexpr std::uint8_t kDealTypeSell = 1;

template <typename Packet>
core::Result<void> SendHandWritten(Connection& link, const Packet& packet) {
  core::Result<std::vector<std::uint8_t>> frame = proto::Encode(packet);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

void SendDealChoice(Connection& link, std::uint32_t npc_id, sim::NpcShopSide side) {
  proto::CzAckSelectDealtype choice;
  choice.GID = npc_id;
  choice.type = side == sim::NpcShopSide::kBuy ? kDealTypeBuy : kDealTypeSell;
  link.Send(proto::Encode(choice));
}

// La boutique peut-elle recevoir une transaction maintenant ? Et faut-il la
// réarmer d'abord ?
core::Result<bool> RearmNeeded(const sim::NpcShop& shop) {
  if (!shop.open()) {
    return core::Fail(ErrorCode::kInvalidArgument, "aucune boutique de PNJ n'est ouverte");
  }
  if (shop.awaiting_result()) {
    return core::Fail(ErrorCode::kInvalidArgument,
                      "une transaction attend encore son résultat : deux se croiseraient sur "
                      "une boutique que le serveur a déjà oubliée");
  }
  if (shop.armed()) {
    return false;
  }
  if (shop.npc_id() == 0) {
    return core::Fail(ErrorCode::kInvalidArgument,
                      "boutique ouverte par un script et déjà servie : le serveur l'a oubliée, "
                      "et rien ne permet de la lui redemander");
  }
  return true;
}

sim::NpcShopOffer OfferOf(const proto::ZcPcPurchaseItemlistSub& listed) {
  return sim::NpcShopOffer{.item_id = listed.itemId,
                           .base_price = listed.price,
                           .price = listed.discountPrice,
                           .item_type = listed.itemType,
                           .view_sprite = listed.viewSprite,
                           .equip_location = listed.location};
}

sim::NpcShopOffer OfferOf(const proto::ZcNpcMarketOpenSub& listed) {
  return sim::NpcShopOffer{.item_id = listed.nameid,
                           .base_price = listed.price,
                           .price = listed.price,
                           .item_type = listed.type,
                           .equip_location = listed.location,
                           .stock = listed.qty == kMarketUnlimitedOnWire
                                        ? sim::kNpcShopUnlimitedStock
                                        : static_cast<std::int64_t>(listed.qty),
                           .unit_weight = listed.weight};
}

sim::NpcShopOffer OfferOf(const proto::ZcPcCashPointItemlistSub& listed) {
  return sim::NpcShopOffer{.item_id = listed.itemId,
                           .base_price = listed.price,
                           .price = listed.discountPrice,
                           .item_type = listed.itemType};
}

}  // namespace

FrameOutcome ApplyNpcShopFrame(const Frame& frame, sim::NpcShop& shop) {
  switch (frame.opcode) {
    case proto::ZcSelectDealtype::kOpcode: {
      proto::ZcSelectDealtype packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      shop.TellDealChoice(packet.npcId);
      return FrameOutcome::kApplied;
    }

    case proto::ZcPcPurchaseItemlist::kOpcode: {
      proto::ZcPcPurchaseItemlist packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // ⚠ LE NOMBRE DE LIGNES SE LIT DANS LA LONGUEUR : le décodeur l'y a lu.
      std::vector<sim::NpcShopOffer> offers;
      offers.reserve(packet.items.size());
      for (const proto::ZcPcPurchaseItemlistSub& listed : packet.items) {
        offers.push_back(OfferOf(listed));
      }
      shop.TellBuyList(offers);
      return FrameOutcome::kApplied;
    }

    case proto::ZcPcSellItemlist::kOpcode: {
      proto::ZcPcSellItemlist packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      std::vector<sim::NpcShopSellOffer> offers;
      offers.reserve(packet.items.size());
      for (const proto::ZcPcSellItemlistSub& listed : packet.items) {
        offers.push_back(sim::NpcShopSellOffer{
            .client_slot = listed.index, .base_price = listed.price, .price = listed.overcharge});
      }
      shop.TellSellList(offers);
      return FrameOutcome::kApplied;
    }

    case proto::ZcPcPurchaseResult::kOpcode: {
      // ⚠ PARTAGÉ AVEC LE TROC : sans boutique de PNJ qui l'attende, il n'est
      // pas à nous.
      if (!shop.open() && !shop.awaiting_result()) {
        return FrameOutcome::kUnhandled;
      }
      proto::ZcPcPurchaseResult packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      shop.TellBuyResult(packet.result);
      return FrameOutcome::kApplied;
    }

    case proto::NpcSellResult::kOpcode: {
      const core::Result<proto::NpcSellResult> packet = proto::DecodeNpcSellResult(frame.bytes);
      if (!packet) {
        return FrameOutcome::kUndecodable;
      }
      shop.TellSellResult(packet->result);
      return FrameOutcome::kApplied;
    }

    case proto::ZcNpcMarketOpen::kOpcode: {
      proto::ZcNpcMarketOpen packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      std::vector<sim::NpcShopOffer> offers;
      offers.reserve(packet.list.size());
      for (const proto::ZcNpcMarketOpenSub& listed : packet.list) {
        offers.push_back(OfferOf(listed));
      }
      shop.TellMarket(offers);
      return FrameOutcome::kApplied;
    }

    case proto::ZcNpcMarketPurchaseResult::kOpcode: {
      proto::ZcNpcMarketPurchaseResult packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      std::vector<sim::NpcShop::MarketBought> bought;
      bought.reserve(packet.list.size());
      for (const proto::ZcNpcMarketPurchaseResultSub& line : packet.list) {
        bought.push_back(sim::NpcShop::MarketBought{.item_id = line.ITID, .amount = line.qty});
      }
      shop.TellMarketResult(packet.result, bought);
      return FrameOutcome::kApplied;
    }

    case proto::ZcPcCashPointItemlist::kOpcode: {
      proto::ZcPcCashPointItemlist packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      std::vector<sim::NpcShopOffer> offers;
      offers.reserve(packet.items.size());
      for (const proto::ZcPcCashPointItemlistSub& listed : packet.items) {
        offers.push_back(OfferOf(listed));
      }
      shop.TellPointShop(offers, packet.cashPoints, packet.kafraPoints);
      return FrameOutcome::kApplied;
    }

    case proto::PointShopUpdate::kOpcode: {
      const core::Result<proto::PointShopUpdate> packet = proto::DecodePointShopUpdate(frame.bytes);
      if (!packet) {
        return FrameOutcome::kUndecodable;
      }
      shop.TellPointShopResult(packet->result, packet->points, packet->secondary_points);
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

core::Result<void> ChooseNpcShopSide(Connection& link, const sim::NpcShop& shop,
                                     sim::NpcShopSide side) {
  if (!shop.open() || shop.kind() != sim::NpcShopKind::kZeny || shop.npc_id() == 0) {
    return core::Fail(ErrorCode::kInvalidArgument,
                      "seule une boutique à zeny nommée par le serveur se choisit");
  }
  SendDealChoice(link, shop.npc_id(), side);
  return {};
}

core::Result<void> BuyFromNpcShop(Connection& link, sim::NpcShop& shop,
                                  std::span<const sim::NpcShopBuyLine> cart) {
  const core::Result<bool> rearm = RearmNeeded(shop);
  if (!rearm) {
    return std::unexpected(rearm.error());
  }
  std::vector<proto::NpcBuyLine> lines;
  lines.reserve(cart.size());
  for (const sim::NpcShopBuyLine& line : cart) {
    if (line.amount <= 0 || line.amount > sim::kNpcShopMostPerStack) {
      return core::Fail(ErrorCode::kInvalidArgument, "quantité d'achat hors des bornes du serveur");
    }
    lines.push_back(proto::NpcBuyLine{.amount = static_cast<std::uint16_t>(line.amount),
                                      .item_id = line.item_id});
  }

  switch (shop.kind()) {
    case sim::NpcShopKind::kZeny: {
      const core::Result<std::vector<std::uint8_t>> frame =
          proto::Encode(proto::NpcBuyRequest{.lines = lines});
      if (!frame) {
        return std::unexpected(frame.error());
      }
      if (*rearm) {
        SendDealChoice(link, shop.npc_id(), sim::NpcShopSide::kBuy);
      }
      link.Send(*frame);
      break;
    }
    case sim::NpcShopKind::kMarket: {
      proto::CzNpcMarketPurchase request;
      request.list.reserve(lines.size());
      for (const proto::NpcBuyLine& line : lines) {
        request.list.push_back(
            proto::CzNpcMarketPurchaseSub{.ITID = line.item_id, .qty = line.amount});
      }
      const std::vector<std::uint8_t> frame = proto::Encode(request);
      if (frame.empty() || frame.size() > proto::kTradeFrameMostBytes) {
        return core::Fail(ErrorCode::kInvalidArgument,
                          "achat au market trop long pour une trame de commerce");
      }
      link.Send(frame);
      break;
    }
    case sim::NpcShopKind::kPoints: {
      // Une boutique à points ou à objet-devise n'a pas de seconde monnaie :
      // rien à dépenser de ce côté.
      const core::Result<std::vector<std::uint8_t>> frame =
          proto::Encode(proto::PointShopBuyRequest{.secondary_points = 0, .lines = lines});
      if (!frame) {
        return std::unexpected(frame.error());
      }
      link.Send(*frame);
      break;
    }
    case sim::NpcShopKind::kNone:
      return core::Fail(ErrorCode::kInvalidArgument, "aucune boutique de PNJ n'est ouverte");
  }
  shop.NoteDealSent();
  return {};
}

core::Result<void> SellToNpcShop(Connection& link, sim::NpcShop& shop,
                                 std::span<const sim::NpcShopSellLine> cart) {
  if (shop.kind() != sim::NpcShopKind::kZeny || shop.deals() == sim::NpcShopDeals::kBuyOnly) {
    return core::Fail(ErrorCode::kInvalidArgument, "cette boutique n'achète rien");
  }
  const core::Result<bool> rearm = RearmNeeded(shop);
  if (!rearm) {
    return std::unexpected(rearm.error());
  }
  if (cart.empty()) {
    return core::Fail(ErrorCode::kInvalidArgument, "liste de vente vide : rien à envoyer");
  }
  proto::CzPcSellItemlist request;
  request.sellList.reserve(cart.size());
  for (const sim::NpcShopSellLine& line : cart) {
    if (line.amount <= 0 || line.amount > sim::kNpcShopMostPerStack) {
      return core::Fail(ErrorCode::kInvalidArgument, "quantité de vente hors des bornes");
    }
    request.sellList.push_back(proto::CzPcSellItemlistSub{
        .index = line.client_slot, .amount = static_cast<std::uint16_t>(line.amount)});
  }
  const std::vector<std::uint8_t> frame = proto::Encode(request);
  if (frame.empty() || frame.size() > proto::kTradeFrameMostBytes) {
    return core::Fail(ErrorCode::kInvalidArgument, "vente trop longue pour une trame de commerce");
  }
  if (*rearm) {
    SendDealChoice(link, shop.npc_id(), sim::NpcShopSide::kSell);
  }
  link.Send(frame);
  shop.NoteDealSent();
  return {};
}

core::Result<void> SayNpcShopFarewell(Connection& link, sim::NpcShopFarewell farewell) {
  switch (farewell) {
    case sim::NpcShopFarewell::kNone:
      return {};
    case sim::NpcShopFarewell::kTradeQuit:
      return SendHandWritten(link, proto::NpcTradeQuit{});
    case sim::NpcShopFarewell::kMarketClose:
      return SendHandWritten(link, proto::NpcMarketClose{});
  }
  return {};
}

}  // namespace ro::net

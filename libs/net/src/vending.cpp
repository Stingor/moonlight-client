// L'échoppe de vente. Voir l'en-tête pour les trames lues sans être consommées.

#include "ro/net/vending.h"

#include <string>
#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/vending_packets.h"

namespace ro::net {
namespace {

// Les champs communs aux deux listes : ma propre échoppe et celle qu'on visite
// décrivent l'objet de la même façon, seul l'ordre du prix et de la quantité
// change sur le fil.
template <typename Entry>
sim::VendingLine LineFrom(const Entry& entry, std::int16_t amount) {
  sim::VendingLine line;
  line.price_zeny = entry.price;
  line.item.client_slot = static_cast<std::uint16_t>(entry.index);
  line.item.item_id = entry.itemId;
  line.item.type = entry.itemType;
  line.item.amount = amount;
  line.item.identified = entry.identified != 0;
  line.item.damaged = entry.damaged != 0;
  line.item.cards = entry.slot.card;
  for (std::size_t rank = 0; rank < sim::kItemOptionSlots; ++rank) {
    line.item.options[rank].id = static_cast<std::uint16_t>(entry.option_data[rank].index);
    line.item.options[rank].value = entry.option_data[rank].value;
    line.item.options[rank].param = entry.option_data[rank].param;
  }
  line.item.refine_level = entry.refine;
  line.item.grade = entry.grade;
  return line;
}

core::Result<void> SendEncoded(Connection& link,
                               const core::Result<std::vector<std::uint8_t>>& frame) {
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

}  // namespace

FrameOutcome ApplyVendingFrame(const Frame& frame, sim::Vending& vending) {
  switch (frame.opcode) {
    case proto::ZcOpenstore::kOpcode: {
      proto::ZcOpenstore packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      vending.TellPreparation(packet.num);
      return FrameOutcome::kApplied;
    }

    case proto::ZcAckOpenstore2::kOpcode: {
      proto::ZcAckOpenstore2 packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      vending.TellOpeningResult(packet.result);
      return FrameOutcome::kApplied;
    }

    case proto::ZcPcPurchaseMyitemlist::kOpcode: {
      proto::ZcPcPurchaseMyitemlist packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // ⚠ LE NOMBRE DE LIGNES SE LIT DANS LA LONGUEUR : le paquet n'a pas de
      // champ qui le compte.
      std::vector<sim::VendingLine> lines;
      lines.reserve(packet.items.size());
      for (const proto::ZcPcPurchaseMyitemlistSub& entry : packet.items) {
        lines.push_back(LineFrom(entry, entry.amount));
      }
      vending.TellShopOpened(packet.AID, std::move(lines));
      return FrameOutcome::kApplied;
    }

    case proto::ZcDeleteitemFromMcstore::kOpcode: {
      proto::ZcDeleteitemFromMcstore packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      vending.TellSale(packet.index, packet.amount, packet.buyerCID, packet.date, packet.zeny);
      return FrameOutcome::kApplied;
    }

    case proto::ZcPcPurchaseItemlistFrommc::kOpcode: {
      proto::ZcPcPurchaseItemlistFrommc packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      std::vector<sim::VendingLine> offers;
      offers.reserve(packet.items.size());
      for (const proto::ZcPcPurchaseItemlistFrommcSub& entry : packet.items) {
        sim::VendingLine offer = LineFrom(entry, static_cast<std::int16_t>(entry.amount));
        offer.item.wearable_at = entry.location;
        offer.item.view_id = entry.viewSprite;
        offers.push_back(std::move(offer));
      }
      vending.TellVisit(packet.AID, packet.venderId, std::move(offers));
      return FrameOutcome::kApplied;
    }

    case proto::ZcPcPurchaseResultFrommc::kOpcode: {
      proto::ZcPcPurchaseResultFrommc packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      vending.TellPurchaseRefused(packet.index, packet.amount, packet.result);
      return FrameOutcome::kApplied;
    }

    case proto::ZcDisappearEntry::kOpcode: {
      proto::ZcDisappearEntry packet;
      if (proto::Decode(frame.bytes, packet)) {
        vending.TellVendorGone(static_cast<std::uint32_t>(packet.GID));
      }
      return FrameOutcome::kUnhandled;
    }

    case proto::ZcNpcackMapmove::kOpcode:
      vending.CloseForMapChange();
      return FrameOutcome::kUnhandled;

    case proto::ZcNpcackServermove::kOpcode:
      vending.CloseSilently();
      return FrameOutcome::kUnhandled;

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestVendingOpen(Connection& link, sim::Vending& vending,
                                      std::string_view title_utf8,
                                      std::span<const sim::VendingSetupLine> lines,
                                      std::span<const sim::InventoryItem> cart, std::int64_t zeny) {
  if (!vending.CanSendOpening()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "aucune préparation d'échoppe n'attend de demande d'ouverture");
  }
  std::string title = sim::FitVendingTitle(title_utf8);
  if (!sim::CheckVendingSetup(title, lines, vending.permitted_lines(), cart, zeny).empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "échoppe que le serveur refuserait ou fausserait");
  }
  proto::VendingOpenRequest request;
  request.title_wire = core::WireFromUtf8Text(title);
  request.open = true;
  request.lines.reserve(lines.size());
  for (const sim::VendingSetupLine& line : lines) {
    request.lines.push_back(proto::VendingOpenLine{
        .cart_slot = line.cart_slot, .amount = line.amount, .price_zeny = line.price_zeny});
  }
  auto frame = proto::Encode(request);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  if (frame->size() > kVendingMostFrameBytes) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "échoppe trop longue pour une trame");
  }
  link.Send(*frame);
  vending.NoteOpeningSent(std::move(title));
  return {};
}

core::Result<void> SayVendingPreparationCancelled(Connection& link) {
  proto::VendingOpenRequest request;
  request.open = false;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestVendingShopClose(Connection& link, sim::Vending& vending) {
  if (!vending.shop_open()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "aucune échoppe ouverte à fermer");
  }
  if (auto sent = SendEncoded(link, proto::Encode(proto::VendingCloseRequest{})); !sent) {
    return sent;
  }
  vending.CloseShop();
  return {};
}

core::Result<void> RequestVendingList(Connection& link, std::uint32_t vendor_aid) {
  return SendEncoded(link, proto::Encode(proto::VendingListRequest{.vendor_aid = vendor_aid}));
}

core::Result<void> RequestVendingPurchase(Connection& link, sim::Vending& vending,
                                          std::span<const sim::VendingBasketLine> basket,
                                          std::int64_t zeny) {
  if (!vending.visiting() || vending.purchase_pending()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "aucune échoppe visitée, ou un achat attend encore sa liste");
  }
  if (!sim::CheckVendingBasket(basket, vending.offers(), zeny).empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "panier que le serveur refuserait ou ignorerait");
  }
  proto::CzPcPurchaseItemlistFrommc2 request;
  request.AID = vending.vendor_aid();
  request.UniqueID = vending.vender_id();
  for (const sim::VendingBasketLine& line : sim::MergeVendingBasket(basket)) {
    request.list.push_back(
        proto::CzPurchaseItemFrommc{.count = static_cast<std::int16_t>(line.amount),
                                    .index = static_cast<std::int16_t>(line.shop_slot)});
  }
  const std::vector<std::uint8_t> purchase = proto::Encode(request);
  if (purchase.empty() || purchase.size() > kVendingMostFrameBytes) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "achat trop long pour une trame");
  }
  auto refresh = proto::Encode(proto::VendingListRequest{.vendor_aid = vending.vendor_aid()});
  if (!refresh) {
    return std::unexpected(refresh.error());
  }
  link.Send(purchase);
  link.Send(*refresh);
  vending.NotePurchaseSent();
  return {};
}

}  // namespace ro::net

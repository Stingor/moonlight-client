// L'échoppe d'achat. Voir l'en-tête pour les changements de carte, lus sans être
// consommés, et pour la vente qui retire l'objet du sac.

#include "ro/net/buying_store.h"

#include <limits>
#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/proto/buying_store_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// Ce que le serveur garde du titre : le champ, moins son terminateur.
constexpr std::size_t kTitleBytesKept = proto::BuyingStoreOpenRequest::kTitleSize - 1;

// Les lignes de mon échoppe et celles d'une échoppe visitée ont la même forme
// sur le fil, sous deux noms de structure.
template <typename Listed>
std::vector<sim::BuyingStoreLine> LinesOf(const std::vector<Listed>& listed) {
  std::vector<sim::BuyingStoreLine> lines;
  lines.reserve(listed.size());
  for (const Listed& entry : listed) {
    lines.push_back(sim::BuyingStoreLine{.item_id = entry.itemId,
                                         .item_type = entry.itemType,
                                         .wanted = entry.amount,
                                         .price = entry.price});
  }
  return lines;
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

FrameOutcome ApplyBuyingStoreFrame(const Frame& frame, sim::BuyingStore& store) {
  switch (frame.opcode) {
    case proto::BuyingStorePreparation::kOpcode: {
      const auto packet = proto::DecodeBuyingStorePreparation(frame.bytes);
      if (!packet) {
        return FrameOutcome::kUndecodable;
      }
      store.TellPreparation(packet->slots);
      return FrameOutcome::kApplied;
    }

    case proto::BuyingStoreOpenFailure::kOpcode: {
      const auto packet = proto::DecodeBuyingStoreOpenFailure(frame.bytes);
      if (!packet) {
        return FrameOutcome::kUndecodable;
      }
      store.TellOpenRefused(packet->result, packet->weight);
      return FrameOutcome::kApplied;
    }

    case proto::ZcMyitemlistBuyingStore::kOpcode: {
      proto::ZcMyitemlistBuyingStore packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // ⚠ LE NOMBRE DE LIGNES SE LIT DANS LA LONGUEUR : le paquet n'a pas de
      // compteur, et le décodeur a déjà découpé la queue.
      const std::vector<sim::BuyingStoreLine> lines = LinesOf(packet.items);
      store.TellOpened(packet.AID, packet.zenyLimit, lines);
      return FrameOutcome::kApplied;
    }

    case proto::ZcUpdateItemFromBuyingStore::kOpcode: {
      proto::ZcUpdateItemFromBuyingStore packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // `zeny` est le coût de la LIGNE, pas le prix unitaire ; `zenyLimit` ce
      // qu'il reste à dépenser après elle.
      store.TellPurchase(sim::BuyingStorePurchase{.item_id = packet.itemId,
                                                  .amount = packet.amount,
                                                  .zeny = packet.zeny,
                                                  .seller_char_id = packet.charId,
                                                  .unix_s = packet.updateTime},
                         packet.zenyLimit);
      return FrameOutcome::kApplied;
    }

    case proto::BuyingStoreEnd::kOpcode: {
      const auto packet = proto::DecodeBuyingStoreEnd(frame.bytes);
      if (!packet) {
        return FrameOutcome::kUndecodable;
      }
      store.TellEnded(packet->result);
      return FrameOutcome::kApplied;
    }

    case proto::ZcAckItemlistBuyingStore::kOpcode: {
      proto::ZcAckItemlistBuyingStore packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      sim::VisitedBuyingStore visited;
      visited.buyer_account_id = packet.AID;
      visited.store_id = packet.storeId;
      visited.zeny_limit = packet.zenyLimit;
      visited.lines = LinesOf(packet.items);
      store.TellVisit(std::move(visited));
      return FrameOutcome::kApplied;
    }

    case proto::BuyingStoreItemSold::kOpcode: {
      const auto packet = proto::DecodeBuyingStoreItemSold(frame.bytes);
      if (!packet) {
        return FrameOutcome::kUndecodable;
      }
      store.TellSold(packet->client_slot, packet->amount, packet->unit_price);
      return FrameOutcome::kApplied;
    }

    case proto::ZcFailedTradeBuyingStoreToSeller::kOpcode: {
      proto::ZcFailedTradeBuyingStoreToSeller packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      store.TellSaleRefused(packet.result, packet.itemId);
      return FrameOutcome::kApplied;
    }

    case proto::ZcNpcackMapmove::kOpcode:
      store.CloseForMapChange();
      return FrameOutcome::kUnhandled;

    case proto::ZcNpcackServermove::kOpcode:
      store.CloseSilently();
      return FrameOutcome::kUnhandled;

    default:
      return FrameOutcome::kUnhandled;
  }
}

FrameOutcome ApplyBuyingStoreSaleToBag(const Frame& frame, sim::Inventory& bag) {
  if (frame.opcode != proto::BuyingStoreItemSold::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  const auto packet = proto::DecodeBuyingStoreItemSold(frame.bytes);
  if (!packet) {
    return FrameOutcome::kUndecodable;
  }
  bag.Take(packet->client_slot, static_cast<std::int16_t>(packet->amount));
  return FrameOutcome::kApplied;
}

FrameOutcome ApplyBuyingStoreSignboardFrame(const Frame& frame, sim::World& world) {
  switch (frame.opcode) {
    case proto::ZcBuyingStoreEntry::kOpcode: {
      proto::ZcBuyingStoreEntry packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // `makerAID` est le compte de l'acheteur, lequel EST son identifiant
      // d'acteur. ⚠ LE TITRE EST UNE PHRASE DU JOUEUR, et le fil porte les deux
      // encodages : un titre écrit par ce client revient en UTF-8.
      sim::Signboard board;
      board.kind = sim::SignboardKind::kBuyingStore;
      board.title = core::Utf8FromWire(packet.storeName);
      return world.SetSignboard(packet.makerAID, std::move(board)) ? FrameOutcome::kApplied
                                                                   : FrameOutcome::kIgnored;
    }

    case proto::ZcDisappearBuyingStoreEntry::kOpcode: {
      proto::ZcDisappearBuyingStoreEntry packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      return world.ClearSignboard(packet.makerAID) ? FrameOutcome::kApplied
                                                   : FrameOutcome::kIgnored;
    }

    default:
      return FrameOutcome::kUnhandled;
  }
}

std::string BuyingStoreTitleOnWire(std::string_view title_utf8) {
  // ⚠ COUPÉ ICI ET NON À L'ENCODAGE quand il est en UTF-8 : couper un caractère
  // en deux laisserait sur l'enseigne un octet orphelin.
  std::string wire = core::WireFromUtf8Text(title_utf8);
  if (wire.size() > kTitleBytesKept) {
    const bool utf8 = wire != core::Cp1252FromUtf8(title_utf8);
    wire.resize(kTitleBytesKept);
    if (utf8) {
      wire.resize(core::WithoutDanglingUtf8(wire).size());
    }
  }
  return wire;
}

core::Result<void> RequestBuyingStoreOpen(Connection& link, sim::BuyingStore& store,
                                          std::string_view title_utf8, std::int64_t zeny_limit,
                                          std::span<const sim::BuyingStoreWish> lines) {
  if (store.phase() != sim::BuyingStorePhase::kPreparing) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "aucune préparation d'échoppe d'achat en cours");
  }
  // Sans ligne, le serveur prend la demande pour une renonciation et se tait.
  if (lines.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "échoppe d'achat sans ligne");
  }
  // Le fil porte la limite sur quatre octets, que le serveur lit signés : hors
  // de cette plage, elle arriverait déformée — et une valeur déformée pourrait
  // passer ses contrôles.
  if (zeny_limit <= 0 || zeny_limit > sim::kBuyingStoreMostZeny) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "limite de zeny hors de ce que le fil porte");
  }

  proto::BuyingStoreOpenRequest request;
  request.zeny_limit = static_cast<std::uint32_t>(zeny_limit);
  request.open = true;
  request.title_on_wire = BuyingStoreTitleOnWire(title_utf8);
  request.lines.reserve(lines.size());
  for (const sim::BuyingStoreWish& line : lines) {
    if (line.amount > std::numeric_limits<std::uint16_t>::max()) {
      return core::Fail(core::ErrorCode::kInvalidArgument,
                        "quantité voulue hors de ce que le fil porte");
    }
    request.lines.push_back(
        proto::BuyingStoreOpenLine{.item_id = line.item_id,
                                   .amount = static_cast<std::uint16_t>(line.amount),
                                   .price = line.price});
  }
  const auto frame = proto::Encode(request);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  store.TellRequested(core::Utf8FromWire(request.title_on_wire), zeny_limit);
  link.Send(*frame);
  return {};
}

core::Result<void> RequestBuyingStoreCancel(Connection& link, sim::BuyingStore& store) {
  if (store.phase() != sim::BuyingStorePhase::kPreparing) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "aucune préparation d'échoppe d'achat en cours");
  }
  proto::BuyingStoreOpenRequest request;
  request.open = false;
  const auto frame = proto::Encode(request);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  store.CancelPreparation();
  link.Send(*frame);
  return {};
}

core::Result<void> SayBuyingStoreClosed(Connection& link) {
  return SendEncoded(link, proto::Encode(proto::BuyingStoreCloseRequest{}));
}

core::Result<void> RequestBuyingStoreList(Connection& link, std::uint32_t buyer_account_id) {
  return SendEncoded(
      link, proto::Encode(proto::BuyingStoreVisitRequest{.buyer_account_id = buyer_account_id}));
}

core::Result<void> RequestBuyingStoreSale(Connection& link, sim::BuyingStore& store,
                                          std::span<const sim::BuyingStoreSaleLine> lines) {
  if (!store.visiting()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "aucune échoppe d'achat visitée");
  }
  // Sans ligne, le serveur se tait.
  if (lines.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "vente sans ligne");
  }
  if (store.sale_pending()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "une vente attend encore la réponse du serveur");
  }

  proto::BuyingStoreTradeRequest request;
  request.buyer_account_id = store.visited().buyer_account_id;
  request.store_id = store.visited().store_id;
  request.lines.reserve(lines.size());
  for (const sim::BuyingStoreSaleLine& line : lines) {
    request.lines.push_back(proto::BuyingStoreTradeLine{
        .client_slot = line.client_slot, .item_id = line.item_id, .amount = line.amount});
  }
  const auto frame = proto::Encode(request);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  store.TellSaleRequested(lines);
  link.Send(*frame);
  return {};
}

}  // namespace ro::net

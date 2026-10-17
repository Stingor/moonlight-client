// La recherche d'échoppes. Voir l'en-tête pour les trames lues sans être
// consommées.

#include "ro/net/store_search.h"

#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/store_search_packets.h"

#include "send_encoded.h"

namespace ro::net {
namespace {

sim::StoreSearchResult ResultFrom(const proto::ZcSearchStoreInfoAckSub& entry) {
  sim::StoreSearchResult result;
  result.store_id = entry.storeId;
  result.owner_aid = entry.AID;
  result.shop_name = core::Utf8FromWire(entry.shopName);
  result.price_zeny = entry.price;
  result.item.item_id = entry.itemId;
  result.item.type = entry.itemType;
  result.item.amount = static_cast<std::int16_t>(entry.amount);
  result.item.identified = true;
  result.item.cards = entry.slot.card;
  for (std::size_t rank = 0; rank < sim::kItemOptionSlots; ++rank) {
    result.item.options[rank].id = static_cast<std::uint16_t>(entry.option_data[rank].index);
    result.item.options[rank].value = entry.option_data[rank].value;
    result.item.options[rank].param = entry.option_data[rank].param;
  }
  result.item.refine_level = entry.refine;
  result.item.grade = entry.grade;
  return result;
}

core::Result<void> RefuseClosed() {
  return core::Fail(core::ErrorCode::kInvalidArgument, "aucune recherche d'échoppes n'est ouverte");
}

}  // namespace

FrameOutcome ApplyStoreSearchFrame(const Frame& frame, sim::StoreSearch& search) {
  switch (frame.opcode) {
    case proto::ZcOpenSearchStoreInfo::kOpcode: {
      proto::ZcOpenSearchStoreInfo packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // ⚠ UN EFFET INCONNU SE LIT COMME L'OUVERTURE À DISTANCE : c'est la seule
      // valeur non nulle que le client d'origine distingue, et elle lui fait
      // demander confirmation avant le clic.
      search.TellOpened(packet.effect == 0 ? sim::StoreSearchEffect::kShowPosition
                                           : sim::StoreSearchEffect::kOpenRemotely,
                        packet.remainingUses);
      return FrameOutcome::kApplied;
    }

    case proto::ZcSearchStoreInfoAck::kOpcode: {
      proto::ZcSearchStoreInfoAck packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // ⚠ LE NOMBRE DE RÉSULTATS SE LIT DANS LA LONGUEUR.
      std::vector<sim::StoreSearchResult> page;
      page.reserve(packet.items.size());
      for (const proto::ZcSearchStoreInfoAckSub& entry : packet.items) {
        page.push_back(ResultFrom(entry));
      }
      search.TellResults(packet.firstPage != 0, packet.nextPage != 0, packet.usesCount,
                         std::move(page));
      return FrameOutcome::kApplied;
    }

    case proto::ZcSearchStoreInfoFailed::kOpcode: {
      proto::ZcSearchStoreInfoFailed packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      search.TellFailure(packet.reason);
      return FrameOutcome::kApplied;
    }

    case proto::ZcSsilistItemClickAck::kOpcode: {
      proto::ZcSsilistItemClickAck packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      search.TellStorePosition(packet.x, packet.y);
      return FrameOutcome::kApplied;
    }

    case proto::ZcNpcackMapmove::kOpcode:
    case proto::ZcNpcackServermove::kOpcode:
      search.CloseForMapChange();
      return FrameOutcome::kUnhandled;

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestStoreSearch(Connection& link, const sim::StoreSearch& search,
                                      const sim::StoreSearchPlan& plan) {
  if (!search.open()) {
    return RefuseClosed();
  }
  if (plan.fault != sim::StoreSearchFault::kNone) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "recherche d'échoppes fautive");
  }
  proto::StoreSearchRequest request;
  request.search_type = static_cast<std::uint8_t>(plan.kind);
  request.min_price_zeny = plan.min_price_zeny;
  request.max_price_zeny = plan.max_price_zeny;
  request.item_ids = plan.item_ids;
  request.card_ids = plan.card_ids;
  return SendEncoded(link, proto::Encode(request));
}

core::Result<void> RequestStoreSearchNextPage(Connection& link, const sim::StoreSearch& search) {
  if (!search.open()) {
    return RefuseClosed();
  }
  if (!search.more_on_server()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "le serveur n'annonce pas d'autre page de résultats");
  }
  return SendEncoded(link, proto::Encode(proto::StoreSearchNextPageRequest{}));
}

core::Result<void> RequestStoreSearchClick(Connection& link, sim::StoreSearch& search,
                                           const sim::StoreSearchResult& result) {
  if (!search.open()) {
    return RefuseClosed();
  }
  proto::CzSsilistItemClick packet;
  packet.AID = result.owner_aid;
  packet.storeId = result.store_id;
  packet.itemId = result.item.item_id;
  link.Send(proto::Encode(packet));
  search.NoteClickSent(result.owner_aid);
  return {};
}

core::Result<void> RequestStoreSearchClose(Connection& link, sim::StoreSearch& search) {
  if (!search.open()) {
    return RefuseClosed();
  }
  // ⚠ LE MODÈLE SE FERME MÊME SI L'ENVOI ÉCHOUE : sinon la fenêtre redemanderait
  // la fermeture à chaque image.
  core::Result<void> sent = SendEncoded(link, proto::Encode(proto::StoreSearchCloseRequest{}));
  search.Close();
  return sent;
}

}  // namespace ro::net

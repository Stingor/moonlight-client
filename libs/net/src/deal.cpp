// L'échange entre joueurs. Voir l'en-tête.

#include "ro/net/deal.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/proto/deal_packets.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/skill_text_packets.h"

namespace ro::net {
namespace {

using core::ErrorCode;

core::Result<void> SendEncoded(Connection& link,
                               const core::Result<std::vector<std::uint8_t>>& frame) {
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

// L'item que le partenaire dépose, dans la forme du sac.
sim::InventoryItem ItemFrom(const proto::ZcAddExchangeItem& packet) {
  sim::InventoryItem item;
  item.item_id = packet.itemId;
  item.type = packet.itemType;
  item.amount = static_cast<std::int16_t>(
      std::min<std::int32_t>(packet.amount, std::numeric_limits<std::int16_t>::max()));
  item.identified = packet.identified != 0;
  item.damaged = packet.damaged != 0;
  item.cards = packet.slot.card;
  for (std::size_t rank = 0; rank < sim::kItemOptionSlots; ++rank) {
    item.options[rank].id = static_cast<std::uint16_t>(packet.option_data[rank].index);
    item.options[rank].value = packet.option_data[rank].value;
    item.options[rank].param = packet.option_data[rank].param;
  }
  item.wearable_at = packet.location;
  item.view_id = packet.look;
  item.refine_level = packet.refine;
  item.grade = packet.grade;
  return item;
}

// Le gain que ce paquet vient d'inscrire au sac ressemble-t-il à un dépôt ?
//
// ⚠ LE GAIN SE RELIT DANS LE JOURNAL, à la condition d'être LE DERNIER et de
// porter la case, l'item et la quantité du paquet. Un ramassage refusé n'inscrit
// rien et remet ces champs à zéro : il ne peut pas désigner un gain plus ancien.
void SuspectReturn(const proto::ZcItemPickupAck& packet, sim::Deal& deal, sim::Inventory& bag) {
  const std::span<const sim::ObtainedItem> obtained = bag.obtained();
  if (obtained.empty()) {
    return;
  }
  const sim::ObtainedItem& last = obtained.back();
  if (last.seq != bag.last_obtained_seq() || last.picked.client_slot != packet.Index ||
      last.picked.item_id != packet.nameid ||
      last.picked.amount != static_cast<std::int16_t>(packet.count)) {
    return;
  }
  if (deal.ClaimReturn(last.picked)) {
    bag.SuspectTradeReturn(last.seq);
  }
}

const char* AddFaultText(sim::DealAddFault fault) {
  switch (fault) {
    case sim::DealAddFault::kNone:
      return "";
    case sim::DealAddFault::kNotOpen:
      return "aucun échange n'est ouvert";
    case sim::DealAddFault::kLocked:
      return "l'offre est verrouillée";
    case sim::DealAddFault::kBadAmount:
      return "quantité hors de la pile";
    case sim::DealAddFault::kEquipped:
      return "item porté : le serveur le tiendrait pour de la triche";
    case sim::DealAddFault::kRented:
      return "item loué";
    case sim::DealAddFault::kHatchedEgg:
      return "œuf éclos";
    case sim::DealAddFault::kAllOffered:
      return "toute la pile est déjà offerte";
    case sim::DealAddFault::kOfferFull:
      return "dix items déjà offerts";
  }
  return "dépôt refusé";
}

const char* ZenyFaultText(sim::DealZenyFault fault) {
  switch (fault) {
    case sim::DealZenyFault::kNone:
      return "";
    case sim::DealZenyFault::kNotOpen:
      return "aucun échange n'est ouvert";
    case sim::DealZenyFault::kLocked:
      return "l'offre est verrouillée";
    case sim::DealZenyFault::kNegative:
      return "montant négatif";
    case sim::DealZenyFault::kAbovePurse:
      return "montant au-delà de la bourse : le serveur annulerait l'échange";
    case sim::DealZenyFault::kAboveMost:
      return "montant au-delà du plafond de zeny";
  }
  return "montant refusé";
}

const char* ExecuteFaultText(sim::DealExecuteFault fault) {
  switch (fault) {
    case sim::DealExecuteFault::kNone:
      return "";
    case sim::DealExecuteFault::kNotOpen:
      return "aucun échange n'est ouvert";
    case sim::DealExecuteFault::kNotBothLocked:
      return "les deux offres ne sont pas verrouillées";
    case sim::DealExecuteFault::kAlreadySent:
      return "validation déjà partie";
    case sim::DealExecuteFault::kAddInFlight:
      return "un dépôt est encore en route";
    case sim::DealExecuteFault::kPartnerChanged:
      return "l'offre du partenaire a changé depuis le récapitulatif";
    case sim::DealExecuteFault::kZenyMelted:
      return "le zeny offert dépasse la bourse : le serveur le tiendrait pour de la triche";
  }
  return "validation refusée";
}

}  // namespace

FrameOutcome ApplyDealFrame(const Frame& frame, sim::Deal& deal, sim::Inventory& bag) {
  switch (frame.opcode) {
    case proto::ZcReqExchangeItem::kOpcode: {
      proto::ZcReqExchangeItem packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      deal.TellProposal(core::Utf8FromWire(packet.requesterName), packet.targetId, packet.targetLv);
      return FrameOutcome::kApplied;
    }

    case proto::ZcAckExchangeItem::kOpcode: {
      proto::ZcAckExchangeItem packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      deal.TellAnswer(packet.result, packet.targetId, packet.targetLv);
      return FrameOutcome::kApplied;
    }

    case proto::ZcAddExchangeItem::kOpcode: {
      proto::ZcAddExchangeItem packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // UN IDENTIFIANT NUL EST DU ZENY : le montant, absolu, voyage seul.
      if (packet.itemId == 0) {
        deal.TellPartnerZeny(packet.amount);
      } else {
        deal.TellPartnerItem(ItemFrom(packet));
      }
      return FrameOutcome::kApplied;
    }

    case proto::ZcAckAddExchangeItem::kOpcode: {
      proto::ZcAckAddExchangeItem packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      if (const std::optional<sim::DealTaken> taken =
              deal.TellAddResult(packet.index, packet.result);
          taken.has_value()) {
        bag.Take(taken->client_slot, taken->amount);
      }
      return FrameOutcome::kApplied;
    }

    case proto::ZcConcludeExchangeItem::kOpcode: {
      proto::ZcConcludeExchangeItem packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      deal.TellLocked(packet.who != 0);
      return FrameOutcome::kApplied;
    }

    case proto::ZcCancelExchangeItem::kOpcode:
      deal.TellCancelled();
      static_cast<void>(bag.SettleTradeReturns(/*cancelled=*/true));
      return FrameOutcome::kApplied;

    case proto::ZcExecExchangeItem::kOpcode: {
      proto::ZcExecExchangeItem packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      deal.TellExecuted(packet.result);
      static_cast<void>(bag.SettleTradeReturns(/*cancelled=*/false));
      return FrameOutcome::kApplied;
    }

    // UN GAIN QUI EST PEUT-ÊTRE UN DÉPÔT RENDU : le monde l'a déjà rangé et
    // noté, il ne reste qu'à le soupçonner. Lu sans être pris.
    case proto::ZcItemPickupAck::kOpcode: {
      proto::ZcItemPickupAck packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUnhandled;
      }
      SuspectReturn(packet, deal, bag);
      return FrameOutcome::kUnhandled;
    }

    // ⚠ LE SERVEUR NE L'ÉMET JAMAIS, et son propre code en ignore l'usage : lu
    // pour ne pas le compter parmi les inconnus, sans effet.
    case proto::ZcExchangeitemUndo::kOpcode:
      return FrameOutcome::kApplied;

    case proto::ZcNpcackMapmove::kOpcode:
    case proto::ZcNpcackServermove::kOpcode:
      deal.CloseForMapChange();
      return FrameOutcome::kUnhandled;

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> ProposeDeal(Connection& link, sim::Deal& deal, sim::DealPartner partner,
                               std::uint32_t now_ms) {
  if (deal.open()) {
    return core::Fail(ErrorCode::kInvalidArgument,
                      "proposition refusée : un échange est déjà ouvert");
  }
  if (partner.account_id == 0) {
    return core::Fail(ErrorCode::kInvalidArgument, "proposition d'échange sans partenaire");
  }
  if (const core::Result<void> sent =
          SendEncoded(link, proto::Encode(proto::DealRequest{.partner_aid = partner.account_id}));
      !sent) {
    return sent;
  }
  deal.NoteProposalSent(std::move(partner), now_ms);
  return {};
}

core::Result<void> AnswerDealProposal(Connection& link, sim::Deal& deal, bool accept) {
  if (deal.phase() != sim::DealPhase::kProposed || deal.answer_sent()) {
    return core::Fail(ErrorCode::kInvalidArgument, "aucune proposition d'échange n'attend");
  }
  if (const core::Result<void> sent =
          SendEncoded(link, proto::Encode(proto::DealAnswer{.accept = accept}));
      !sent) {
    return sent;
  }
  deal.NoteAnswerSent(accept);
  return {};
}

core::Result<void> OfferDealItem(Connection& link, sim::Deal& deal,
                                 const sim::InventoryItem& bag_item, std::int32_t amount,
                                 std::uint32_t now_ms) {
  if (const sim::DealAddFault fault = deal.CheckAdd(bag_item, amount);
      fault != sim::DealAddFault::kNone) {
    return core::Fail(ErrorCode::kInvalidArgument,
                      std::string("dépôt refusé : ") + AddFaultText(fault));
  }
  proto::CzAddExchangeItem packet;
  packet.index = bag_item.client_slot;
  packet.amount = amount;
  link.Send(proto::Encode(packet));
  deal.NoteAddSent(bag_item, static_cast<std::int16_t>(amount), now_ms);
  return {};
}

core::Result<void> OfferDealZeny(Connection& link, sim::Deal& deal, std::int64_t amount,
                                 std::int64_t purse) {
  if (const sim::DealZenyFault fault = deal.CheckZeny(amount, purse);
      fault != sim::DealZenyFault::kNone) {
    return core::Fail(ErrorCode::kInvalidArgument,
                      std::string("zeny refusé : ") + ZenyFaultText(fault));
  }
  proto::CzAddExchangeItem packet;
  packet.index = sim::kDealZenySlot;
  packet.amount = static_cast<std::int32_t>(amount);
  link.Send(proto::Encode(packet));
  deal.NoteZenySent(amount);
  return {};
}

core::Result<void> LockDeal(Connection& link, sim::Deal& deal) {
  if (!deal.open() || deal.lock_sent()) {
    return core::Fail(ErrorCode::kInvalidArgument, "verrou refusé : rien à verrouiller");
  }
  if (!deal.pending_adds().empty()) {
    return core::Fail(ErrorCode::kInvalidArgument, "verrou refusé : un dépôt est encore en route");
  }
  if (const core::Result<void> sent = SendEncoded(link, proto::Encode(proto::DealLockRequest{}));
      !sent) {
    return sent;
  }
  deal.NoteLockSent();
  return {};
}

core::Result<void> ExecuteDeal(Connection& link, sim::Deal& deal,
                               std::uint32_t confirmed_partner_revision, std::int64_t purse) {
  if (const sim::DealExecuteFault fault = deal.CheckExecute(confirmed_partner_revision, purse);
      fault != sim::DealExecuteFault::kNone) {
    return core::Fail(ErrorCode::kInvalidArgument,
                      std::string("validation refusée : ") + ExecuteFaultText(fault));
  }
  if (const core::Result<void> sent = SendEncoded(link, proto::Encode(proto::DealExecuteRequest{}));
      !sent) {
    return sent;
  }
  deal.NoteExecuteSent();
  return {};
}

DealFrameRefusal DealRefusesFrame(const sim::Deal& deal,
                                  std::span<const std::uint8_t> frame) noexcept {
  constexpr std::size_t kOpcodeBytes = 2;
  if (!deal.open() || frame.size() < kOpcodeBytes) {
    return DealFrameRefusal::kNone;
  }
  const auto opcode = static_cast<std::uint16_t>(frame[0] | (frame[1] << 8));
  switch (opcode) {
    case proto::CzReqWearEquip::kOpcode:
    case proto::UnequipItemRequest::kOpcode:
    case proto::UseItemRequest::kOpcode:
      return DealFrameRefusal::kItemGesture;
    case proto::SkillOnTargetRequest::kOpcode:
    case proto::SkillOnGroundRequest::kOpcode:
    case proto::SkillOnGroundWithTextRequest::kOpcode:
    case proto::CzUseSkillStart::kOpcode:
      return sim::DealForbidsSkills(deal) ? DealFrameRefusal::kSkillWithStake
                                          : DealFrameRefusal::kNone;
    case proto::CzPcPurchaseItemlistFrommc2::kOpcode:
    case proto::CzReqBankingDeposit::kOpcode:
      return sim::DealForbidsZenySpending(deal) ? DealFrameRefusal::kZenyWithStake
                                                : DealFrameRefusal::kNone;
    default:
      return DealFrameRefusal::kNone;
  }
}

core::Result<void> CancelDeal(Connection& link, sim::Deal& deal) {
  if (deal.phase() == sim::DealPhase::kNone) {
    return core::Fail(ErrorCode::kInvalidArgument, "aucun échange à annuler");
  }
  // UNE PROPOSITION REÇUE SE REFUSE : le proposant l'apprend alors comme un
  // refus, et non comme une annulation sans auteur.
  if (deal.phase() == sim::DealPhase::kProposed) {
    return AnswerDealProposal(link, deal, false);
  }
  if (const core::Result<void> sent = SendEncoded(link, proto::Encode(proto::DealCancelRequest{}));
      !sent) {
    return sent;
  }
  deal.NoteCancelSent();
  return {};
}

}  // namespace ro::net

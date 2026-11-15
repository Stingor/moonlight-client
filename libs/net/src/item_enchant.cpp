// L'enchantement, sur le fil. Voir l'en-tête.

#include "ro/net/item_enchant.h"

#include <cstdint>

#include "ro/proto/map_packets.h"

namespace ro::net {
namespace {

// LA RÉUSSITE : la pièce porte ses nouvelles cartes, le reste ne bouge pas.
void Enchant(sim::Inventory& bag, const sim::ItemEnchant::Result& result) {
  const sim::InventoryItem* item = bag.Find(result.client_slot);
  if (item == nullptr) {
    return;
  }
  const sim::InventoryItem kept = *item;
  static_cast<void>(bag.Reforge(result.client_slot, kept.refine_level, kept.grade, kept.damaged,
                                sim::EnchantedCards(kept.cards, result), kept.options));
}

}  // namespace

FrameOutcome ApplyItemEnchantFrame(const Frame& frame, sim::ItemEnchant& enchant,
                                   sim::Inventory& bag) {
  switch (frame.opcode) {
    case proto::ZcUiOpenV3::kOpcode: {
      proto::ZcUiOpenV3 packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      if (packet.type != sim::kItemEnchantUiType) {
        return FrameOutcome::kUnhandled;
      }
      enchant.TellOpening(packet.data);
      return FrameOutcome::kApplied;
    }

    case proto::ZcResponseEnchant::kOpcode: {
      proto::ZcResponseEnchant packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      enchant.TellResult(packet.msgId, packet.ITID);
      if (enchant.result().succeeded()) {
        Enchant(bag, enchant.result());
      }
      return FrameOutcome::kApplied;
    }

    case proto::ZcNpcackMapmove::kOpcode:
    case proto::ZcNpcackServermove::kOpcode:
      enchant.CloseSilently();
      return FrameOutcome::kUnhandled;

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestItemEnchant(Connection& link, sim::ItemEnchant& enchant,
                                      const sim::ItemEnchantGroup& group,
                                      sim::ItemEnchantRequest request, const sim::Inventory& bag,
                                      std::uint8_t card_slots, std::int64_t zeny) {
  const sim::ItemEnchantCheck check =
      sim::CheckItemEnchantRequest(enchant, group, request, bag, card_slots, zeny);
  if (!check.passed()) {
    return core::Fail(core::ErrorCode::kInvalidArgument,
                      "demande d'enchantement que le serveur refuserait");
  }
  const auto group_id = static_cast<std::int64_t>(enchant.group_id());
  const auto index = static_cast<std::int16_t>(*enchant.chosen_slot());
  switch (request.mode) {
    case sim::ItemEnchantMode::kRandom:
      link.Send(
          proto::Encode(proto::CzRequestRandomEnchant{.enchant_group = group_id, .index = index}));
      break;
    case sim::ItemEnchantMode::kPerfect:
      link.Send(proto::Encode(proto::CzRequestPerfectEnchant{
          .enchant_group = group_id, .index = index, .ITID = request.enchant_item_id}));
      break;
    case sim::ItemEnchantMode::kUpgrade:
      link.Send(proto::Encode(
          proto::CzRequestUpgradeEnchant{.enchant_group = group_id,
                                         .index = index,
                                         .slot = static_cast<std::int16_t>(request.slot)}));
      break;
    case sim::ItemEnchantMode::kReset:
      link.Send(
          proto::Encode(proto::CzRequestResetEnchant{.enchant_group = group_id, .index = index}));
      break;
  }
  enchant.MarkRequestSent(request, check.target_slot, card_slots);
  return {};
}

void SayItemEnchantFarewell(Connection& link, sim::ItemEnchant& enchant) {
  if (enchant.TakeFarewell()) {
    link.Send(proto::Encode(proto::CzCloseUiEnchant{}));
  }
}

}  // namespace ro::net

// Les nouvelles d'objets. Voir l'en-tête.

#include "ro/net/item_news.h"

#include <cstddef>

#include "ro/proto/map_packets.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

[[nodiscard]] sim::PartyPickup PickupOf(const proto::ZcItemPickupParty& packet,
                                        std::uint32_t now_ms) {
  sim::PartyPickup pickup{.account_id = packet.AID, .heard_ms = now_ms};
  sim::InventoryItem& item = pickup.item;
  item.item_id = packet.itemId;
  item.type = packet.itemType;
  item.wearable_at = packet.location;
  item.identified = packet.identified != 0;
  item.damaged = packet.damaged != 0;
  item.refine_level = packet.refine;
  item.grade = packet.grade;
  for (std::size_t rank = 0; rank < item.cards.size(); ++rank) {
    item.cards[rank] = packet.slot.card[rank];
  }
  item.amount = 1;
  return pickup;
}

}  // namespace

FrameOutcome ApplyItemNewsFrame(const Frame& frame, sim::ItemNews& news, std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::ZcItemPickupParty::kOpcode:
      return Decoded<proto::ZcItemPickupParty>(frame, [&news, now_ms](const auto& packet) {
        news.NotePartyPickup(PickupOf(packet, now_ms));
        return FrameOutcome::kApplied;
      });

    case proto::ZcEquipitemDamaged::kOpcode:
      return Decoded<proto::ZcEquipitemDamaged>(frame, [&news, now_ms](const auto& packet) {
        news.NoteEquipDamage(sim::EquipDamage{
            .account_id = packet.GID, .location = packet.equipLocation, .heard_ms = now_ms});
        return FrameOutcome::kApplied;
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

}  // namespace ro::net

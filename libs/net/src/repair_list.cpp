// La liste de Repair Weapon. Voir l'en-tête.

#include "ro/net/repair_list.h"

#include <vector>

#include "ro/proto/map_packets.h"
#include "ro/proto/repair_packets.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// La place qui dit « aucune pièce » au serveur.
constexpr std::int16_t kRefusedIndex = -1;

}  // namespace

FrameOutcome ApplyRepairFrame(const Frame& frame, sim::RepairListOffer& offer) {
  if (frame.opcode == proto::ZcAckItemrepair::kOpcode) {
    return Decoded<proto::ZcAckItemrepair>(frame, [&offer](const auto& packet) {
      offer.TellOutcome(packet.result == 0);
      return FrameOutcome::kApplied;
    });
  }
  if (frame.opcode != proto::RepairItemList::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  const auto list = proto::DecodeRepairItemList(frame.bytes);
  if (!list) {
    return FrameOutcome::kUndecodable;
  }
  std::vector<sim::RepairableItem> items;
  items.reserve(list->items.size());
  for (const proto::RepairableEntry& entry : list->items) {
    items.push_back(sim::RepairableItem{.index = entry.index,
                                        .item_id = entry.item_id,
                                        .cards = entry.cards,
                                        .refine = entry.refine,
                                        .grade = entry.grade});
  }
  offer.Offer(std::move(items));
  return offer.open() ? FrameOutcome::kApplied : FrameOutcome::kIgnored;
}

core::Result<void> RequestRepair(Connection& link, const sim::RepairableItem* chosen) {
  proto::RepairItemRequest request;
  if (chosen == nullptr) {
    request.item.index = kRefusedIndex;
  } else {
    request.item = proto::RepairableEntry{.index = chosen->index,
                                          .item_id = chosen->item_id,
                                          .cards = chosen->cards,
                                          .refine = chosen->refine,
                                          .grade = chosen->grade};
  }
  link.Send(proto::EncodeRepairItemRequest(request));
  return {};
}

}  // namespace ro::net

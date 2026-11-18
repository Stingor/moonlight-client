// L'atelier : sept trames, quatre verbes. Voir l'en-tête pour ce que le serveur
// tient de la session.

#include "ro/net/craft_session.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/proto/craft_packets.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/craft_session.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"

#include "frame_apply.h"
#include "send_encoded.h"

namespace ro::net {
namespace {

// Les réponses d'affinage que le client reconnaît. ⚠ TOUTE AUTRE VALEUR EST
// TUE, comme chez lui.
constexpr std::int32_t kMostRefineOutcome =
    static_cast<std::int32_t>(sim::RefineOutcome::kMissingOre);

template <typename Entries>
[[nodiscard]] std::vector<std::uint32_t> ItemIdsOf(const Entries& entries) {
  std::vector<std::uint32_t> item_ids;
  item_ids.reserve(entries.size());
  for (const auto& entry : entries) {
    item_ids.push_back(entry.itemId);
  }
  return item_ids;
}

[[nodiscard]] FrameOutcome ApplyRefineList(const proto::ZcNotifyWeaponitemlist& packet,
                                           sim::World& world) {
  std::vector<sim::RefinableWeapon> weapons;
  weapons.reserve(packet.items.size());
  for (const proto::ZcNotifyWeaponitemlistSub& entry : packet.items) {
    weapons.push_back(sim::RefinableWeapon{.index = entry.index,
                                           .item_id = entry.itemId,
                                           .refine = entry.refine,
                                           .cards = entry.slot.card});
  }
  world.craft().TellRefineList(std::move(weapons));
  return FrameOutcome::kApplied;
}

// CE QUE LE SERVEUR RETIRE DES EMPLACEMENTS DE FORGE, sans paquet de retrait.
//
// Un Star Crumb par emplacement qui en porte un, et UNE SEULE pierre
// élémentaire quel que soit leur nombre : les autres restent dans le sac. Chaque
// exemplaire se prend dans la première pile de l'objet, celle de la plus petite
// case — c'est celle que le serveur vide. Un emplacement sans exemplaire au sac
// ne retire rien, et ne compte pas comme la pierre prise.
constexpr std::uint32_t kStarCrumbId = 1000;
constexpr std::uint32_t kFirstElementStoneId = 994;  // Flame Heart
constexpr std::uint32_t kLastElementStoneId = 997;   // Great Nature

void TakeForgeExtras(const sim::ForgeExtras& extras, sim::Inventory& bag) {
  bool element_taken = false;
  for (const std::uint32_t item_id : extras) {
    const bool star = item_id == kStarCrumbId;
    const bool element =
        item_id >= kFirstElementStoneId && item_id <= kLastElementStoneId && !element_taken;
    if (!star && !element) {
      continue;
    }
    const auto items = bag.items();
    const auto held = std::ranges::find_if(items, [item_id](const sim::InventoryItem& item) {
      return item.item_id == item_id && item.amount > 0;
    });
    if (held == items.end()) {
      continue;
    }
    static_cast<void>(bag.Take(held->client_slot, 1));
    element_taken = element_taken || element;
  }
}

}  // namespace

FrameOutcome ApplyCraftFrame(const Frame& frame, sim::World& world) {
  switch (frame.opcode) {
    case proto::ZcMakingarrowList::kOpcode:
      return Decoded<proto::ZcMakingarrowList>(frame, [&world](const auto& packet) {
        // ⚠ LE GENRE EST POSÉ ICI, faute d'être sur le fil : c'est celui que la
        // réponse des flèches suppose.
        world.craft().TellMakeList(sim::MakeList{.kind = sim::MakeListKind::kArrow,
                                                 .make_type = sim::kArrowMakeType,
                                                 .item_ids = ItemIdsOf(packet.items)});
        return FrameOutcome::kApplied;
      });

    case proto::ZcMakingitemList::kOpcode:
      return Decoded<proto::ZcMakingitemList>(frame, [&world](const auto& packet) {
        // 🔴 LE GENRE DES FLÈCHES PASSE AUSSI PAR ICI, et il répond alors par le
        // paquet des flèches : c'est le genre, pas l'opcode, qui choisit.
        const sim::MakeListKind kind = packet.makeItem == sim::kArrowMakeType
                                           ? sim::MakeListKind::kArrow
                                           : sim::MakeListKind::kMaking;
        world.craft().TellMakeList(sim::MakeList{
            .kind = kind, .make_type = packet.makeItem, .item_ids = ItemIdsOf(packet.items)});
        return FrameOutcome::kApplied;
      });

    case proto::ZcMakableitemlist::kOpcode:
      return Decoded<proto::ZcMakableitemlist>(frame, [&world](const auto& packet) {
        // Les trois matériaux de chaque entrée sont toujours nuls : seul
        // l'objet compte.
        world.craft().TellMakeList(sim::MakeList{.kind = sim::MakeListKind::kProduce,
                                                 .make_type = 0,
                                                 .item_ids = ItemIdsOf(packet.items)});
        return FrameOutcome::kApplied;
      });

    case proto::ZcAckReqmakingitem::kOpcode:
      return Decoded<proto::ZcAckReqmakingitem>(frame, [&world](const auto& packet) {
        // Réussie ou ratée, la forge a consommé ses emplacements.
        TakeForgeExtras(world.craft().TakeForgeExtras(), world.inventory());
        world.craft().TellMakeResult(
            sim::MakeResult{.code = packet.result, .item_id = packet.itemId});
        return FrameOutcome::kApplied;
      });

    case proto::ZcMoonlightCookMastery::kOpcode:
      return Decoded<proto::ZcMoonlightCookMastery>(frame, [&world](const auto& packet) {
        world.craft().TellCookMastery(packet.mastery);
        return FrameOutcome::kApplied;
      });

    case proto::ZcNotifyWeaponitemlist::kOpcode:
      return Decoded<proto::ZcNotifyWeaponitemlist>(
          frame, [&world](const auto& packet) { return ApplyRefineList(packet, world); });

    case proto::ZcAckWeaponrefine::kOpcode:
      return Decoded<proto::ZcAckWeaponrefine>(frame, [&world](const auto& packet) {
        if (packet.result < 0 || packet.result > kMostRefineOutcome) {
          return FrameOutcome::kIgnored;
        }
        world.craft().TellRefineResult(sim::RefineResult{
            .outcome = static_cast<sim::RefineOutcome>(packet.result), .item_id = packet.itemId});
        return FrameOutcome::kApplied;
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

void ObserveCraftTrigger(std::span<const std::uint8_t> sent, sim::World& world,
                         std::uint32_t now_ms) {
  core::ByteReader reader(sent);
  const std::uint16_t opcode = reader.U16();
  if (!reader.ok()) {
    return;
  }
  if (opcode == proto::SkillOnTargetRequest::kOpcode &&
      sent.size() >= proto::SkillOnTargetRequest::kWireSize) {
    const std::uint16_t level = reader.U16();
    const std::uint16_t skill_id = reader.U16();
    world.craft().NoteSkillCast(skill_id, level, now_ms);
    return;
  }
  if (opcode == proto::UseItemRequest::kOpcode && sent.size() >= proto::UseItemRequest::kWireSize) {
    const std::uint16_t client_slot = reader.U16();
    const sim::InventoryItem* item = world.inventory().Find(client_slot);
    world.craft().NoteItemUse(item != nullptr ? item->item_id : 0, now_ms);
  }
}

core::Result<void> RequestMakeArrow(Connection& link, std::uint32_t item_id) {
  proto::CzReqMakingarrow request;
  request.itemId = item_id;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestMakeItem(Connection& link, std::uint16_t make_type,
                                   std::uint32_t item_id) {
  proto::CzReqMakingitem request;
  request.type = static_cast<std::int16_t>(make_type);
  request.itemId = item_id;
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestProduce(Connection& link, sim::CraftSession& craft, std::uint32_t item_id,
                                  const sim::ForgeExtras& extras) {
  proto::CzReqmakingitem request;
  request.itemId = item_id;
  request.material = extras;
  link.Send(proto::Encode(request));
  // Une annulation ne retire rien.
  craft.NoteForgeSent(item_id != 0 ? extras : sim::ForgeExtras{});
  return {};
}

core::Result<void> RequestWeaponRefine(Connection& link, std::int32_t index) {
  return SendEncoded(link, proto::Encode(proto::WeaponRefineRequest{.index = index}));
}

}  // namespace ro::net

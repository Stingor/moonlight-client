// Les nouvelles d'objets. Voir l'en-tête.

#include "ro/net/item_news.h"

#include <cstddef>
#include <optional>
#include <utility>

#include "ro/core/byte_reader.h"
#include "ro/core/text.h"
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

// L'OBJET RARE ANNONCÉ À TOUS. ⚠ LUE CHAMP PAR CHAMP : la même trame a deux
// formes, et sa queue dépend de la source — l'identifiant de la boîte pour une
// boîte, le nom du monstre (vide pour un PNJ) sinon.
//
//   type(1) item(4) longueur(1) nom(longueur) longueur(1) boîte(4) | nom(longueur)
constexpr std::uint16_t kRareObtainOpcode = 0x07FD;
constexpr std::size_t kVariableHeaderSize = 4;

[[nodiscard]] std::optional<sim::RareItemObtain> ReadRareObtain(const Frame& frame,
                                                                std::uint32_t now_ms) {
  core::ByteReader reader(frame.bytes);
  reader.Skip(kVariableHeaderSize);
  sim::RareItemObtain obtained{.heard_ms = now_ms};
  obtained.source = reader.U8();
  obtained.item_id = reader.U32();
  const std::uint8_t name_size = reader.U8();
  obtained.player_name = core::Utf8FromWire(reader.FixedString(name_size));
  const std::uint8_t tail_size = reader.U8();
  if (obtained.source == static_cast<std::uint8_t>(sim::RareObtainSource::kBox)) {
    obtained.box_item_id = reader.U32();
  } else {
    obtained.monster_name = core::Utf8FromWire(reader.FixedString(tail_size));
  }
  if (!reader.ok()) {
    return std::nullopt;
  }
  return obtained;
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

    case kRareObtainOpcode: {
      std::optional<sim::RareItemObtain> obtained = ReadRareObtain(frame, now_ms);
      if (!obtained.has_value()) {
        return FrameOutcome::kIgnored;
      }
      news.NoteRareObtain(std::move(*obtained));
      return FrameOutcome::kApplied;
    }

    case proto::ZcBroadcastItemrefiningResult::kOpcode:
      return Decoded<proto::ZcBroadcastItemrefiningResult>(
          frame, [&news, now_ms](const auto& packet) {
            news.NoteRefine(sim::RefineAnnounce{.player_name = core::Utf8FromWire(packet.name),
                                                .item_id = packet.itemId,
                                                .refine_level = packet.refine_level,
                                                .success = packet.status != 0,
                                                .heard_ms = now_ms});
            return FrameOutcome::kApplied;
          });

    case proto::ZcGradeEnchantBroadcastResult::kOpcode:
      return Decoded<proto::ZcGradeEnchantBroadcastResult>(
          frame, [&news, now_ms](const auto& packet) {
            // ⚠ LA RÉUSSITE SE LIT À UN EXACTEMENT, comme chez le client : toute
            // autre valeur se dit comme un échec.
            news.NoteGrade(sim::GradeAnnounce{.player_name = core::Utf8FromWire(packet.name),
                                              .item_id = packet.itemId,
                                              .grade = packet.grade,
                                              .success = packet.status == 1,
                                              .heard_ms = now_ms});
            return FrameOutcome::kApplied;
          });

    default:
      return FrameOutcome::kUnhandled;
  }
}

}  // namespace ro::net

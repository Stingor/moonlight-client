// Ce que le serveur dit du personnage et que rien ne montre encore. Voir
// l'en-tête.

#include "ro/net/character_ledger.h"

#include <optional>
#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/proto/ledger_packets.h"
#include "ro/proto/map_packets.h"

#include "frame_apply.h"

namespace ro::net {
FrameOutcome ApplyLedgerFrame(const Frame& frame, sim::CharacterLedger& ledger) {
  switch (frame.opcode) {
    case proto::ZcAckBuyCashEmotion::kOpcode:
      return Decoded<proto::ZcAckBuyCashEmotion>(frame, [&ledger](const auto& packet) {
        // ⚠ LE COMPTE NE VAUT QUE SI LE DRAPEAU LE DIT : sans lui, le serveur
        // laisse le champ à zéro et le pack est sans limite.
        ledger.OwnEmotionPack(packet.pack_id, packet.has_count != 0
                                                  ? std::optional<std::int32_t>(packet.count)
                                                  : std::nullopt);
        return FrameOutcome::kApplied;
      });

    case proto::EquipSwitchList::kOpcode: {
      const core::Result<proto::EquipSwitchList> list = proto::DecodeEquipSwitchList(frame.bytes);
      if (!list) {
        return FrameOutcome::kUndecodable;
      }
      std::vector<sim::EquipSwitchPiece> pieces;
      pieces.reserve(list->entries.size());
      for (const proto::EquipSwitchEntry& entry : list->entries) {
        pieces.push_back(
            sim::EquipSwitchPiece{.client_slot = entry.client_slot, .location = entry.location});
      }
      ledger.ReplaceEquipSwitch(std::move(pieces));
      return FrameOutcome::kApplied;
    }

    case proto::ZcReputeInfo::kOpcode:
      return Decoded<proto::ZcReputeInfo>(frame, [&ledger](const auto& packet) {
        if (packet.success == 0) {
          return FrameOutcome::kIgnored;
        }
        for (const proto::ZcReputeInfoSub& entry : packet.list) {
          ledger.SetReputation(entry.type, entry.points);
        }
        return FrameOutcome::kApplied;
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

}  // namespace ro::net

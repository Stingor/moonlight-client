// Ce que le serveur dit du personnage et que rien ne montre encore. Voir
// l'en-tête.

#include "ro/net/character_ledger.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "ro/core/text.h"
#include "ro/proto/ledger_packets.h"
#include "ro/proto/map_packets.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// Les codes des accusés du jeu de rechange. Le rangement répond par
// `ITEM_EQUIP_ACK_*` ; le client d'origine dit « rien à échanger » pour trois et
// « impossible de ranger » pour tout autre refus.
constexpr std::uint16_t kSwitchGranted = 0;
constexpr std::uint16_t kSwitchAddNothingToReplace = 3;
// La sortie et l'échange refusent par un : « rien à échanger ».
constexpr std::uint16_t kSwitchNothingToReplace = 1;

}  // namespace

std::optional<sim::EquipSwitchNews> EquipSwitchNewsOf(const Frame& frame) {
  switch (frame.opcode) {
    case proto::EquipSwitchAddAck::kOpcode: {
      const core::Result<proto::EquipSwitchAddAck> ack =
          proto::DecodeEquipSwitchAddAck(frame.bytes);
      if (!ack) {
        return std::nullopt;
      }
      sim::EquipSwitchNews news{.event = sim::EquipSwitchEvent::kPlaced,
                                .client_slot = ack->client_slot,
                                .location = ack->location};
      if (ack->result != kSwitchGranted) {
        news.event = sim::EquipSwitchEvent::kPlaceRefused;
        news.refusal = ack->result == kSwitchAddNothingToReplace
                           ? sim::EquipSwitchRefusal::kNothingToReplace
                           : sim::EquipSwitchRefusal::kCannotRegister;
      }
      return news;
    }
    case proto::EquipSwitchRemoveAck::kOpcode: {
      const core::Result<proto::EquipSwitchRemoveAck> ack =
          proto::DecodeEquipSwitchRemoveAck(frame.bytes);
      if (!ack) {
        return std::nullopt;
      }
      sim::EquipSwitchNews news{.event = sim::EquipSwitchEvent::kRemoved,
                                .client_slot = ack->client_slot,
                                .location = ack->location};
      if (ack->failure == kSwitchGranted) {
        return news;
      }
      if (ack->failure != kSwitchNothingToReplace) {
        return std::nullopt;
      }
      news.event = sim::EquipSwitchEvent::kRemoveRefused;
      news.refusal = sim::EquipSwitchRefusal::kNothingToReplace;
      return news;
    }
    case proto::EquipSwitchRunAck::kOpcode: {
      const core::Result<proto::EquipSwitchRunAck> ack =
          proto::DecodeEquipSwitchRunAck(frame.bytes);
      if (!ack || ack->failure == kSwitchGranted) {
        return std::nullopt;
      }
      return sim::EquipSwitchNews{.event = sim::EquipSwitchEvent::kSwapRefused,
                                  .refusal = ack->failure == kSwitchNothingToReplace
                                                 ? sim::EquipSwitchRefusal::kNothingToReplace
                                                 : sim::EquipSwitchRefusal::kCannotSwap};
    }
    default:
      return std::nullopt;
  }
}

FrameOutcome ApplyLedgerFrame(const Frame& frame, sim::CharacterLedger& ledger) {
  switch (frame.opcode) {
    case proto::EquipSwitchAddAck::kOpcode:
    case proto::EquipSwitchRemoveAck::kOpcode:
    case proto::EquipSwitchRunAck::kOpcode: {
      const std::optional<sim::EquipSwitchNews> news = EquipSwitchNewsOf(frame);
      if (!news.has_value()) {
        // ⚠ RIEN À DIRE N'EST PAS ILLISIBLE : seule la taille distingue une
        // trame abîmée d'un accusé qui se tait.
        static_assert(proto::EquipSwitchAddAck::kWireSize ==
                      proto::EquipSwitchRemoveAck::kWireSize);
        const std::size_t expected = frame.opcode == proto::EquipSwitchRunAck::kOpcode
                                         ? proto::EquipSwitchRunAck::kWireSize
                                         : proto::EquipSwitchAddAck::kWireSize;
        return frame.bytes.size() == expected ? FrameOutcome::kIgnored : FrameOutcome::kUndecodable;
      }
      switch (news->event) {
        case sim::EquipSwitchEvent::kPlaced:
          ledger.PlaceEquipSwitch(
              sim::EquipSwitchPiece{.client_slot = news->client_slot, .location = news->location});
          return FrameOutcome::kApplied;
        case sim::EquipSwitchEvent::kRemoved:
        case sim::EquipSwitchEvent::kRemoveRefused:
          ledger.RemoveEquipSwitch(news->client_slot);
          return FrameOutcome::kApplied;
        case sim::EquipSwitchEvent::kPlaceRefused:
        case sim::EquipSwitchEvent::kSwapRefused:
          break;
      }
      return FrameOutcome::kIgnored;
    }

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

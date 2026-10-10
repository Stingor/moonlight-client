// Le champ de bataille : la file et le combat. Voir l'en-tête.

#include "ro/net/battleground.h"

#include <string>
#include <utility>

#include "ro/core/text.h"
#include "ro/proto/battleground_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/battlefield.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// LES DEUX RÉPONSES D'UNE ANNULATION QUI FAIT SORTIR DE LA FILE : 1, et 14,
// que le serveur envoie pour qu'un client qui ne referme « en attente » que sur
// ce code la referme aussi. Toute autre valeur est un refus.
constexpr std::uint8_t kWireCancelSucceeded = 1;
constexpr std::uint8_t kWireCancelClosed = 14;
// Les deux réponses à l'invitation.
constexpr std::uint8_t kWireLobbyAccept = 1;
constexpr std::uint8_t kWireLobbyDecline = 2;

// UN NOM DE TERRAIN OU DE SALLE TEL QU'IL PART : en page de codes du jeu, non
// vide, et assez court pour garder son zéro dans ses 24 octets — le serveur
// n'en lit pas davantage, et un nom coupé désignerait un autre terrain, ou
// aucun.
[[nodiscard]] core::Result<std::string> WireName(std::string_view utf8) {
  std::string wire = core::Cp1252FromUtf8(utf8);
  if (wire.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "nom de champ de bataille vide");
  }
  if (wire.size() >= proto::kBattlefieldNameBytes) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "nom de champ de bataille trop long");
  }
  return wire;
}

}  // namespace

FrameOutcome ApplyEntryQueueFrame(const Frame& frame, sim::EntryQueue& queue,
                                  std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::PacketBgqueueAck::kOpcode:
      return Decoded<proto::PacketBgqueueAck>(frame, [&queue, now_ms](const auto& packet) {
        queue.TellApplyResult(packet.type, core::Utf8FromCp1252(packet.bg_name), now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::PacketBgqueueNotifyEntry::kOpcode:
      return Decoded<proto::PacketBgqueueNotifyEntry>(frame, [&queue, now_ms](const auto& packet) {
        // L'effectif TOTAL des deux équipes en file, et non un rang.
        queue.TellCount(core::Utf8FromCp1252(packet.name), packet.position, now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::PacketBgqueueNoticeDelete::kOpcode:
      return Decoded<proto::PacketBgqueueNoticeDelete>(frame, [&queue](const auto& packet) {
        queue.TellCancelResult(
            packet.type == kWireCancelSucceeded || packet.type == kWireCancelClosed,
            core::Utf8FromCp1252(packet.bg_name));
        return FrameOutcome::kApplied;
      });

    case proto::PacketBgqueueBattlebegins::kOpcode:
      return Decoded<proto::PacketBgqueueBattlebegins>(frame, [&queue, now_ms](const auto& packet) {
        queue.TellInvitation(core::Utf8FromCp1252(packet.bg_name),
                             core::Utf8FromCp1252(packet.game_name), now_ms);
        return FrameOutcome::kApplied;
      });

    case proto::LobbyAdmissionAck::kOpcode:
      return Decoded<proto::LobbyAdmissionAck>(frame, [&queue](const auto& packet) {
        // Le premier nom est celui de la CARTE où le serveur nous envoie.
        queue.TellAdmission(packet.result, core::Utf8FromCp1252(packet.field_name));
        return FrameOutcome::kApplied;
      });

    case proto::ZcEntryQueueInit::kOpcode:
      return Decoded<proto::ZcEntryQueueInit>(frame, [&queue](const auto& /*packet*/) {
        queue.Reset();
        return FrameOutcome::kApplied;
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

FrameOutcome ApplyBattlefieldFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  switch (frame.opcode) {
    case proto::BattlefieldChatNotice::kOpcode:
      return Decoded<proto::BattlefieldChatNotice>(frame, [&world, now_ms](const auto& packet) {
        // Le texte porte déjà le nom de qui parle. Un orateur nul est le
        // serveur : la ligne entre dans la conversation, sans bulle.
        world.RecordChat(sim::ChatKind::kBattlefield, sim::ChatType::kBattlefield,
                         packet.speaker_id, core::Utf8FromWire(packet.message), now_ms,
                         sim::kBattlefieldChatRgb);
        return FrameOutcome::kApplied;
      });

    case proto::BattlefieldCampNotice::kOpcode:
      return Decoded<proto::BattlefieldCampNotice>(frame, [&world](const auto& packet) {
        // ⚠ AUCUN ACTEUR N'EST EXIGÉ : le camp se dit à l'apparition, et le
        // nôtre arrive avant que nous soyons un acteur du monde.
        world.battlefield().TellCamp(packet.actor_id, packet.camp, world.player_id());
        return FrameOutcome::kApplied;
      });

    case proto::BattlefieldScoreNotice::kOpcode:
      return Decoded<proto::BattlefieldScoreNotice>(frame, [&world](const auto& packet) {
        world.battlefield().TellScore(packet.lion, packet.eagle);
        return FrameOutcome::kApplied;
      });

    case proto::BattlefieldPositionNotice::kOpcode:
      return Decoded<proto::BattlefieldPositionNotice>(frame, [&world](const auto& packet) {
        // Notre propre position a sa marque à part sur la minimap.
        if (packet.account_id == world.player_id()) {
          return FrameOutcome::kIgnored;
        }
        world.battlefield().TellMate(packet.account_id, core::Utf8FromCp1252(packet.name),
                                     packet.job, packet.x, packet.y);
        return FrameOutcome::kApplied;
      });

    case proto::ZcBattlefieldNotifyHp::kOpcode:
      return Decoded<proto::ZcBattlefieldNotifyHp>(frame, [&world](const auto& packet) {
        // Un coéquipier hors de vue n'a pas d'acteur où poser sa vie.
        return world.SetHealth(packet.AID, packet.hp, packet.maxhp) ? FrameOutcome::kApplied
                                                                    : FrameOutcome::kIgnored;
      });

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestEntryQueueApply(Connection& link, sim::EntryQueueApplyKind kind,
                                          std::string_view field_name) {
  auto wire = WireName(field_name);
  if (!wire) {
    return std::unexpected(wire.error());
  }
  proto::PacketBgqueueRegister request;
  request.type = static_cast<std::int16_t>(kind);
  request.bg_name = std::move(*wire);
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestEntryQueueCancel(Connection& link, std::string_view field_name) {
  auto wire = WireName(field_name);
  if (!wire) {
    return std::unexpected(wire.error());
  }
  proto::PacketBgqueueRevokeReq request;
  request.bg_name = std::move(*wire);
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> AnswerLobbyAdmission(Connection& link, bool accept, std::string_view field_name,
                                        std::string_view lobby_name) {
  auto wire_field = WireName(field_name);
  if (!wire_field) {
    return std::unexpected(wire_field.error());
  }
  auto wire_lobby = WireName(lobby_name);
  if (!wire_lobby) {
    return std::unexpected(wire_lobby.error());
  }
  proto::PacketBgqueueBattlebeginAck request;
  request.result = accept ? kWireLobbyAccept : kWireLobbyDecline;
  request.bg_name = std::move(*wire_field);
  request.game_name = std::move(*wire_lobby);
  link.Send(proto::Encode(request));
  return {};
}

core::Result<void> RequestEntryQueueRanking(Connection& link, std::string_view field_name) {
  auto wire = WireName(field_name);
  if (!wire) {
    return std::unexpected(wire.error());
  }
  proto::PacketBgqueueCheckstate request;
  request.bg_name = std::move(*wire);
  link.Send(proto::Encode(request));
  return {};
}

}  // namespace ro::net

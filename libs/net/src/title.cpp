// Les titres. Voir l'en-tête pour les quatre paquets et pour celui qui ne donne
// rien.

#include "ro/net/title.h"

#include "ro/proto/map_packets.h"
#include "ro/proto/title_packets.h"

namespace ro::net {

FrameOutcome ApplyTitleFrame(const Frame& frame, sim::TitleBook& titles, std::uint32_t player_id) {
  switch (frame.opcode) {
    case proto::AchievementList::kOpcode: {
      const auto list = proto::DecodeAchievementList(frame.bytes);
      if (!list) {
        return FrameOutcome::kUndecodable;
      }
      for (const proto::AchievementEntry& entry : list->entries) {
        if (entry.rewarded) {
          static_cast<void>(titles.NoteRewarded(entry.achievement_id));
        }
      }
      return FrameOutcome::kApplied;
    }

    case proto::ChangeTitleAck::kOpcode: {
      const auto ack = proto::DecodeChangeTitleAck(frame.bytes);
      if (!ack) {
        return FrameOutcome::kUndecodable;
      }
      if (ack->result != proto::ChangeTitleAck::kAccepted) {
        return FrameOutcome::kIgnored;
      }
      titles.TellEquipped(ack->title_id);
      return FrameOutcome::kApplied;
    }

    case proto::ZcAckReqnameall::kOpcode: {
      proto::ZcAckReqnameall packet;
      if (!proto::Decode(frame.bytes, packet)) {
        return FrameOutcome::kUndecodable;
      }
      // ⚠ LA CARTE D'UN AUTRE NE DIT RIEN DE NOUS, et c'est l'immense majorité
      // de ce trafic : les monstres de ce serveur passent tous par ce paquet.
      if (static_cast<std::uint32_t>(packet.gid) != player_id) {
        return FrameOutcome::kIgnored;
      }
      titles.TellEquipped(packet.title_id);
      return FrameOutcome::kApplied;
    }

    default:
      return FrameOutcome::kUnhandled;
  }
}

core::Result<void> RequestChangeTitle(Connection& link, std::int32_t title_id) {
  proto::ChangeTitleRequest request;
  request.title_id = title_id;
  auto frame = proto::Encode(request);
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

}  // namespace ro::net

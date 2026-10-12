#include "game/monster_talk_stage.h"

#include <span>
#include <string>
#include <utility>

#include "ro/proto/map_packets.h"

namespace moonclient {
namespace {

[[nodiscard]] std::uint32_t AsId(std::int32_t id) { return static_cast<std::uint32_t>(id); }

}  // namespace

void MonsterTalkStage::LoadTable(ro::vfs::Vfs& vfs) {
  if (!table_.has_value()) {
    table_ = ro::vfs::ReadMonsterTalkTable(vfs);
  }
}

void MonsterTalkStage::StartSession() {
  memories_.Clear();
  memories_map_entries_ = 0;
}

void MonsterTalkStage::NoteBeforeWorld(const ro::net::Frame& frame, ro::sim::World& world,
                                       const std::function<bool(const ro::sim::Actor&)>& at_rest,
                                       std::uint32_t now_ms) {
  if (world.map_entries() != memories_map_entries_) {
    memories_map_entries_ = world.map_entries();
    memories_.Clear();
  }
  // NOTRE COUP fait parler notre familier, pas nous : voir `PetTalkStage`.
  const std::uint32_t own_id = world.player_id();
  switch (frame.opcode) {
    case ro::proto::ZcNotifyAct::kOpcode: {
      ro::proto::ZcNotifyAct packet;
      if (!ro::proto::Decode(frame.bytes, packet) || AsId(packet.srcID) == own_id ||
          packet.damage == ro::sim::kVoidStrikeDamage) {
        return;
      }
      const std::uint32_t attacker_id = AsId(packet.srcID);
      const ro::sim::Actor* attacker = world.Find(attacker_id);
      // UN ACTEUR HORS DE VUE ne mémorise ni ne dit rien.
      if (attacker == nullptr) {
        return;
      }
      // Le repos ne se juge que pour un acteur qui a quelque chose à dire ; la
      // cible se mémorise dans tous les cas.
      const std::span<const std::string> lines = LinesOf(*attacker);
      const bool rests = !lines.empty() && at_rest(*attacker);
      if (memories_.Strike(attacker_id, rests, AsId(packet.targetID), packet.type)) {
        Say(*attacker, lines, world, now_ms);
      }
      return;
    }
    case ro::proto::ZcNotifySkill::kOpcode: {
      ro::proto::ZcNotifySkill packet;
      if (ro::proto::Decode(frame.bytes, packet) && packet.AID != own_id &&
          !(packet.SKID == ro::sim::kSelfIgnoredStrikeSkill && packet.targetID == packet.AID)) {
        memories_.Remember(packet.AID, packet.targetID);
      }
      return;
    }
    case ro::proto::ZcUseskillAck::kOpcode: {
      // UNE INCANTATION QUI VISE QUELQU'UN ; une compétence instantanée ne
      // mémorise rien.
      ro::proto::ZcUseskillAck packet;
      if (ro::proto::Decode(frame.bytes, packet) && packet.srcId != own_id && packet.dstId != 0 &&
          packet.delayTime != 0) {
        memories_.Remember(packet.srcId, packet.dstId);
      }
      return;
    }
    case ro::proto::ZcNotifyVanish::kOpcode: {
      ro::proto::ZcNotifyVanish packet;
      if (ro::proto::Decode(frame.bytes, packet)) {
        memories_.Forget(packet.gid);
      }
      return;
    }
    default:
      return;
  }
}

std::span<const std::string> MonsterTalkStage::LinesOf(const ro::sim::Actor& actor) const {
  // UN ACTEUR SANS NOM ne dit rien.
  if (!table_.has_value() || catalog_ == nullptr || actor.name.empty()) {
    return {};
  }
  return table_->AttackLinesOf(
      ro::sim::MonsterTalkSpecies(ro::render::NonPlayerResourceName(*catalog_, actor.look.job)));
}

void MonsterTalkStage::Say(const ro::sim::Actor& speaker, std::span<const std::string> lines,
                           ro::sim::World& world, std::uint32_t now_ms) {
  const std::uint32_t actor_id = speaker.id;
  std::string text = ro::sim::MonsterLineShown(speaker.name, lines[draw_() % lines.size()]);
  if (talk_mute_ != nullptr && talk_mute_->hides_talk()) {
    world.ShowSpeech(actor_id, std::move(text), now_ms);
    return;
  }
  world.RecordChat(ro::sim::ChatKind::kPublic, ro::sim::ChatType::kPublicChat, actor_id,
                   std::move(text), now_ms, kMonsterLineColor);
}

}  // namespace moonclient

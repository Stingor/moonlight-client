#include "game/pet_talk_stage.h"

#include <limits>
#include <span>
#include <string>
#include <utility>
#include <variant>

#include "ro/core/log.h"
#include "ro/net/pet_frames.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/movement.h"
#include "ro/sim/player_status.h"

namespace moonclient {
namespace {

// Un tirage du client, en pour cent.
constexpr unsigned kPercent = 100;

[[nodiscard]] std::uint32_t AsId(std::int32_t id) { return static_cast<std::uint32_t>(id); }

}  // namespace

void PetTalkStage::LoadTable(ro::vfs::Vfs& vfs) {
  if (!table_.has_value()) {
    table_ = ro::vfs::ReadPetTalkTable(vfs);
  }
}

void PetTalkStage::StartSession() {
  chatter_.Reset();
  pending_.clear();
  owner_.reset();
  hunt_.Forget();
  hunt_map_entries_ = 0;
}

int PetTalkStage::Roll() { return static_cast<int>(draw_() % kPercent); }

void PetTalkStage::Queue(const std::vector<std::int32_t>& codes) {
  pending_.insert(pending_.end(), codes.begin(), codes.end());
}

// ── CE QUI S'ENTEND ─────────────────────────────────────────────────────────

ro::net::FrameOutcome PetTalkStage::Hear(const ro::net::Frame& frame, const ro::sim::Pet& pet,
                                         ro::sim::World& world, std::uint32_t now_ms) {
  const std::optional<ro::net::PetActNotice> act = ro::net::ReadPetAct(frame);
  if (!act.has_value()) {
    return frame.opcode == ro::proto::ZcPetAct::kOpcode ? ro::net::FrameOutcome::kUndecodable
                                                        : ro::net::FrameOutcome::kUnhandled;
  }
  // UN FAMILIER HORS DE VUE ne dit rien.
  if (world.Find(act->actor_id) == nullptr) {
    return ro::net::FrameOutcome::kIgnored;
  }
  const ro::sim::PetActHeard heard = ro::sim::ReadPetActCode(act->code);
  if (const auto* emotion = std::get_if<ro::sim::PetEmotionHeard>(&heard)) {
    // Un numéro hors des émoticônes ne montre rien.
    if (emotion->emotion < 0 || emotion->emotion > std::numeric_limits<std::uint16_t>::max()) {
      return ro::net::FrameOutcome::kIgnored;
    }
    const ro::sim::Emotion shown{ro::sim::kBaseEmotionPack,
                                 static_cast<std::uint16_t>(emotion->emotion)};
    return world.RecordEmotion(act->actor_id, shown, now_ms) ? ro::net::FrameOutcome::kApplied
                                                             : ro::net::FrameOutcome::kIgnored;
  }
  ShowLine(std::get<ro::sim::PetLineHeard>(heard), act->actor_id, pet, world, now_ms);
  return ro::net::FrameOutcome::kApplied;
}

void PetTalkStage::ShowLine(const ro::sim::PetLineHeard& heard, std::uint32_t actor_id,
                            const ro::sim::Pet& pet, ro::sim::World& world, std::uint32_t now_ms) {
  if (!table_.has_value()) {
    return;
  }
  const std::span<const std::string> lines =
      table_->LinesOf(heard.pet_class, heard.hunger_digit, heard.occasion_digit);
  if (lines.empty()) {
    return;
  }
  const ro::sim::Actor* speaker = world.Find(actor_id);
  // UN ACTEUR SANS NOM ne parle pas — sauf notre familier.
  if (speaker == nullptr || (speaker->name.empty() && actor_id != pet.actor_id())) {
    return;
  }
  std::string text = ro::sim::PetLineShown(speaker->name, lines[draw_() % lines.size()]);
  if (talk_mute_ != nullptr && talk_mute_->hides_talk()) {
    world.ShowSpeech(actor_id, std::move(text), now_ms);
    return;
  }
  world.RecordChat(ro::sim::ChatKind::kPublic, ro::sim::ChatType::kPublicChat, actor_id,
                   std::move(text), now_ms, kPetLineColor);
}

// ── CE QUE NOTRE FAMILIER DOIT DIRE ─────────────────────────────────────────

void PetTalkStage::NoteBeforeWorld(const ro::net::Frame& frame, const ro::sim::Pet& pet,
                                   const ro::sim::World& world,
                                   const std::function<bool()>& owner_at_rest,
                                   std::uint32_t now_ms) {
  const std::uint32_t owner_id = world.player_id();
  if (owner_id == 0) {
    return;
  }
  if (world.map_entries() != hunt_map_entries_) {
    hunt_map_entries_ = world.map_entries();
    hunt_.Forget();
  }
  switch (frame.opcode) {
    case ro::proto::ZcNotifyAct::kOpcode: {
      ro::proto::ZcNotifyAct packet;
      if (!ro::proto::Decode(frame.bytes, packet) || AsId(packet.srcID) != owner_id ||
          packet.damage == ro::sim::kVoidStrikeDamage) {
        return;
      }
      if (hunt_.Strike(owner_at_rest(), AsId(packet.targetID), packet.type) && pet.known() &&
          world.Find(pet.actor_id()) != nullptr) {
        Queue(chatter_.OwnerEngaged(pet, now_ms, Roll()));
      }
      return;
    }
    case ro::proto::ZcNotifySkill::kOpcode: {
      ro::proto::ZcNotifySkill packet;
      if (ro::proto::Decode(frame.bytes, packet) && packet.AID == owner_id &&
          !(packet.SKID == ro::sim::kSelfIgnoredStrikeSkill && packet.targetID == owner_id)) {
        hunt_.Remember(packet.targetID);
      }
      return;
    }
    case ro::proto::ZcUseskillAck::kOpcode: {
      // UNE INCANTATION QUI VISE QUELQU'UN ; une compétence instantanée ne
      // mémorise rien.
      ro::proto::ZcUseskillAck packet;
      if (ro::proto::Decode(frame.bytes, packet) && packet.srcId == owner_id && packet.dstId != 0 &&
          packet.delayTime != 0) {
        hunt_.Remember(packet.dstId);
      }
      return;
    }
    default:
      return;
  }
}

void PetTalkStage::NoteOwnerHealth(const ro::net::Frame& frame, const ro::sim::Pet& pet,
                                   const ro::sim::World& world, std::uint32_t now_ms) {
  if (frame.opcode != ro::proto::ZcParChange::kOpcode) {
    return;
  }
  ro::proto::ZcParChange packet;
  if (!ro::proto::Decode(frame.bytes, packet) ||
      packet.varID != static_cast<std::uint16_t>(ro::sim::ServerParameter::kHp)) {
    return;
  }
  const ro::sim::Actor* owner = world.player();
  if (owner == nullptr || !pet.known() || world.Find(pet.actor_id()) == nullptr) {
    return;
  }
  Queue(chatter_.OwnerHurt(pet, packet.count, owner->max_health, now_ms, Roll()));
}

void PetTalkStage::NoteFed(const ro::sim::Pet& pet, const ro::sim::World& world) {
  if (pet.known() && world.Find(pet.actor_id()) != nullptr) {
    Queue(chatter_.Fed(pet));
  }
}

bool PetTalkStage::NotePerformance(const ro::net::Frame& frame, const ro::sim::Pet& pet,
                                   const ro::sim::World& world) {
  const std::optional<ro::net::PetPerformanceNotice> shown = ro::net::ReadPetPerformance(frame);
  if (!shown.has_value()) {
    return false;
  }
  if (pet.known() && shown->actor_id == pet.actor_id() && world.Find(shown->actor_id) != nullptr) {
    Queue(chatter_.Performed(pet, shown->performance));
  }
  return true;
}

void PetTalkStage::Observe(const ro::sim::Pet& pet, const ro::sim::World& world,
                           std::uint32_t now_ms) {
  const ro::sim::Actor* owner = world.player();
  if (owner == nullptr) {
    return;
  }
  OwnerSeen now{
      .dead = owner->posture == ro::sim::Posture::kDead,
      .base_level = world.player_status().base_level,
  };
  const OwnerSeen before = owner_.value_or(now);
  owner_ = now;
  const ro::sim::Actor* const shown = pet.known() ? world.Find(pet.actor_id()) : nullptr;
  // SANS SON ACTEUR EN VUE, le familier ne dit rien — mais ce qu'a vécu le
  // maître est noté, pour ne pas le lui faire dire à son retour.
  if (shown == nullptr) {
    return;
  }
  if (now.dead && !before.dead) {
    Queue(chatter_.OwnerDied(pet));
  }
  if (before.base_level != ro::sim::kUnknownStat && now.base_level > before.base_level) {
    Queue(chatter_.OwnerLevelledUp(pet));
  }
  Queue(chatter_.ObserveRest(pet, ro::sim::IsWalking(*shown, now_ms), now_ms, Roll()));
}

void PetTalkStage::Flush(ro::net::Connection& link) {
  for (const std::int32_t code : std::exchange(pending_, {})) {
    if (const auto sent = ro::net::RequestPetAct(link, code); !sent) {
      ro::core::LogWarning("parole du familier non envoyée : {}", ro::core::Describe(sent.error()));
    }
  }
}

}  // namespace moonclient

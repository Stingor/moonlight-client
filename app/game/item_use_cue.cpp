#include "game/item_use_cue.h"

#include <string>

#include "ro/core/i18n.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/item_use_effect.h"

#include "world/actor_view.h"

namespace moonclient {
namespace {

// L'accusé d'usage dit la réussite par un : zéro est un refus.
constexpr std::uint8_t kUseRefused = 0;

[[nodiscard]] const char* LineText(ro::sim::ItemUseLine line) {
  switch (line) {
    case ro::sim::ItemUseLine::kExpBonus50:
      return ro::core::Tr(
          "L'expérience de base et de job gagnée sur les monstres augmente de moitié "
          "pendant 30 minutes.");
    case ro::sim::ItemUseLine::kExpBonus75:
      return ro::core::Tr(
          "L'expérience de base et de job gagnée sur les monstres augmente de trois quarts "
          "pendant 30 minutes.");
    case ro::sim::ItemUseLine::kNone:
      break;
  }
  return nullptr;
}

}  // namespace

std::optional<std::string_view> CueItemUse(const ro::net::Frame& frame, ro::sim::World& world,
                                           std::uint32_t now_ms) {
  if (frame.opcode != ro::proto::ZcUseItemAck::kOpcode) {
    return std::nullopt;
  }
  ro::proto::ZcUseItemAck packet;
  if (!ro::proto::Decode(frame.bytes, packet) || packet.result == kUseRefused) {
    return std::nullopt;
  }
  const ro::sim::Actor* user = world.Find(packet.AID);
  if (user == nullptr) {
    return std::nullopt;
  }
  if (packet.AID == world.player_id()) {
    if (const char* text = LineText(ro::sim::ItemUseLineFor(packet.itemId, ShownJobOf(*user)));
        text != nullptr) {
      world.RecordChat(ro::sim::ChatKind::kServer, ro::sim::kItemUseLineType, 0, std::string(text),
                       now_ms, ro::sim::kItemUseLineColor);
    }
  }
  const ro::sim::ItemUseEffect* use = ro::sim::FindItemUseEffect(packet.itemId);
  if (use == nullptr) {
    return std::nullopt;
  }
  for (std::uint8_t rank = 0; rank < use->effect_count; ++rank) {
    static_cast<void>(world.RecordEffect(packet.AID, use->effect_ids[rank], now_ms));
  }
  if (use->sound.empty()) {
    return std::nullopt;
  }
  return use->sound;
}

}  // namespace moonclient

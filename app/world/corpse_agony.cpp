#include "world/corpse_agony.h"

#include <algorithm>
#include <cmath>

#include "ro/render/sprite_frame.h"
#include "ro/render/vanish_fade.h"
#include "ro/sim/death_rules.h"

#include "world/actor_view.h"

namespace moonclient {

std::uint32_t Agony::EndMs() const noexcept {
  return start_ms + static_cast<std::uint32_t>(std::lround(std::max(duration_ms, 0.0F)));
}

std::uint32_t AgonyStartAfter(std::uint32_t death_ms, std::uint32_t receiver_id,
                              std::span<const BattleMessage> messages) noexcept {
  std::uint32_t start_ms = death_ms;
  for (const BattleMessage& message : messages) {
    if (message.receiver_id == receiver_id) {
      start_ms = std::max(start_ms, message.due_ms);
    }
  }
  return start_ms;
}

std::optional<Agony> AgonyOf(const ro::sim::Actor& actor, const ro::render::ActorOutfit& outfit,
                             std::size_t facing, std::uint32_t start_ms) {
  if (actor.posture != ro::sim::Posture::kDead ||
      ro::sim::DeathAftermathOf(actor) != ro::sim::DeathAftermath::kLeavesCorpse) {
    return std::nullopt;
  }
  const std::size_t die = ro::render::AnimationFor(ro::render::Motion::kDie, LayoutOf(actor));
  return Agony{
      .start_ms = start_ms,
      .duration_ms = ro::render::AgonyDurationMs(outfit.BodyActionFrameCount(die, facing),
                                                 outfit.BodyActionDelayUnits(die, facing))};
}

}  // namespace moonclient

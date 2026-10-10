#include "world/ailment_tints.h"

#include <cstddef>

#include "ro/sim/world.h"

#include "world/actor_tint.h"

namespace moonclient {

void AilmentTints::Observe(const ro::sim::World& world, std::uint32_t now_ms) {
  std::erase_if(seen_,
                [&world](const auto& entry) { return !world.actors().contains(entry.first); });
  for (const auto& [id, actor] : world.actors()) {
    const bool yields = StillCarriesEffect(actor, ro::render::kPoisonTintYieldingEffect, now_ms);
    auto found = seen_.find(id);
    if (found == seen_.end()) {
      // L'APPARITION est un changement d'état depuis un corps vide.
      Seen fresh;
      ro::render::LatchStateChange(fresh.latch, 0, actor.body_state, actor.health_state, yields);
      fresh.body_state = actor.body_state;
      fresh.state_change_count = actor.state_change_count;
      found = seen_.emplace(id, fresh).first;
    }
    Seen& seen = found->second;

    // LES HANDICAPS ET BLADE STOP D'ABORD : ils posent ce qui retient le
    // changement d'état de blanchir.
    for (std::size_t slot = 0; slot < ro::render::kTintingHandicaps; ++slot) {
      const bool held = actor.status_effects.contains(ro::render::kTintingHandicapStates[slot]);
      if (held != seen.handicaps[slot]) {
        ro::render::LatchHandicap(seen.latch, slot, held);
        seen.handicaps[slot] = held;
      }
    }
    const bool blade_stop = actor.blade_stop_since_ms.has_value();
    if (blade_stop != seen.blade_stop) {
      ro::render::LatchBladeStop(seen.latch, blade_stop);
      seen.blade_stop = blade_stop;
    }

    if (actor.state_change_count != seen.state_change_count ||
        actor.body_state != seen.body_state) {
      ro::render::LatchStateChange(seen.latch, seen.body_state, actor.body_state,
                                   actor.health_state, yields);
      seen.body_state = actor.body_state;
      seen.state_change_count = actor.state_change_count;
    }
  }
}

std::optional<std::array<std::uint8_t, 4>> AilmentTints::TintOf(std::uint32_t actor_id) const {
  const auto found = seen_.find(actor_id);
  if (found == seen_.end()) {
    return std::nullopt;
  }
  return ro::render::LatchedTint(found->second.latch);
}

}  // namespace moonclient

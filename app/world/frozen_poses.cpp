#include "world/frozen_poses.h"

#include "ro/sim/world.h"

namespace moonclient {

FrozenPoses::Shown FrozenPoses::Hold(std::uint32_t actor_id, std::uint32_t since_ms,
                                     bool keeps_shown_image, const Shown& now) {
  Kept& kept = kept_[actor_id];
  if (kept.frozen_since_ms != since_ms) {
    kept.frozen_since_ms = since_ms;
    kept.frozen = now;
    if (keeps_shown_image && kept.last_shown.has_value()) {
      kept.frozen.animation = kept.last_shown->animation;
      kept.frozen.frame = kept.last_shown->frame;
    }
  }
  return kept.frozen;
}

void FrozenPoses::NoteShown(std::uint32_t actor_id, const Shown& shown) {
  kept_[actor_id].last_shown = shown;
}

void FrozenPoses::Retain(const ro::sim::World& world) {
  std::erase_if(kept_, [&world](const auto& entry) { return world.Find(entry.first) == nullptr; });
}

}  // namespace moonclient

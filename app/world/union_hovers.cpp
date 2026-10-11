#include "world/union_hovers.h"

#include <bit>

#include "ro/render/actor_layers.h"
#include "ro/render/shown_job.h"

namespace moonclient {

float UnionHovers::PixelsOf(const ro::sim::Actor& actor, ro::render::Motion motion, bool red_veiled,
                            const ro::render::ComposedActor& composed, std::uint32_t now_ms) {
  const auto found = kept_.find(actor.id);
  if (found != kept_.end()) {
    found->second.seen = true;
  }
  if ((actor.option & ro::render::kOptionFlying) == 0) {
    return 0.0F;
  }
  Kept& kept = found != kept_.end() ? found->second : kept_[actor.id];
  kept.seen = true;
  return static_cast<float>(kept.clock.PixelsAt(
      now_ms, ro::render::UnionHoverPostureOf(motion, red_veiled), PartsDrawnOf(composed)));
}

void UnionHovers::ForgetUnseen() {
  std::erase_if(kept_, [](const auto& entry) { return !entry.second.seen; });
  for (auto& [id, kept] : kept_) {
    kept.seen = false;
  }
}

std::uint32_t PartsDrawnOf(const ro::render::ComposedActor& composed) {
  if (composed.quads.empty()) {
    return 0;
  }
  ro::render::ActorPartSet parts = 0;
  for (std::size_t index = 0; index < composed.quads.size(); ++index) {
    parts = static_cast<ro::render::ActorPartSet>(
        parts | ro::render::PartBit(ro::render::PartOfQuad(composed, index)));
  }
  return static_cast<std::uint32_t>(std::popcount(parts));
}

}  // namespace moonclient

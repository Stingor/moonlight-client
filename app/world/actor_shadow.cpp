#include "world/actor_shadow.h"

#include "ro/render/shown_job.h"

namespace moonclient {

bool CastsShadow(const ShadowFacts& facts) noexcept {
  if (facts.leaving_sight) {
    return false;
  }
  constexpr std::uint8_t kOpaque = 255;
  switch (facts.kind) {
    case ro::sim::ActorKind::kPlayer:
      if (facts.motion == ro::render::Motion::kDie) {
        return false;
      }
      return facts.motion != ro::render::Motion::kSit ||
             ro::render::IsUnionFlightJob(facts.shown_job);
    case ro::sim::ActorKind::kNonPlayer:
    case ro::sim::ActorKind::kMonster:
      return facts.body_alpha == kOpaque;
    case ro::sim::ActorKind::kItem:
      return true;
    case ro::sim::ActorKind::kSkillUnit:
    case ro::sim::ActorKind::kUnknown:
      return false;
  }
  return false;
}

}  // namespace moonclient

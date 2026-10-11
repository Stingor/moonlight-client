#include "game/companion_target_marks.h"

#include <algorithm>
#include <utility>

namespace moonclient {

void CompanionTargetMarkClock::Update(std::span<const std::uint32_t> bearer_ids,
                                      std::uint32_t now_ms) {
  std::vector<Bearer> kept;
  kept.reserve(bearer_ids.size());
  for (const std::uint32_t actor_id : bearer_ids) {
    if (actor_id == 0 || std::ranges::any_of(kept, [actor_id](const Bearer& bearer) {
          return bearer.actor_id == actor_id;
        })) {
      continue;
    }
    kept.push_back(Bearer{actor_id, MarkedSince(actor_id).value_or(now_ms)});
  }
  bearers_ = std::move(kept);
}

std::optional<std::uint32_t> CompanionTargetMarkClock::MarkedSince(
    std::uint32_t actor_id) const noexcept {
  const auto found = std::ranges::find(bearers_, actor_id, &Bearer::actor_id);
  if (found == bearers_.end()) {
    return std::nullopt;
  }
  return found->since_ms;
}

}  // namespace moonclient

#include "ro/render/tint_recovery_clock.h"

#include <algorithm>

#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pool.h"

namespace ro::render {
namespace {

// Le plus long retard qu'une image rattrape : celui de la troupe des nœuds.
constexpr auto kMaxCatchUpMs =
    static_cast<std::uint32_t>(kEffectNodeMaxCatchUpTicks * kEffectTickMs);

}  // namespace

void TintRecoveryClock::NoteFrameAt(std::uint32_t now_ms) {
  if (!last_frame_ms_.has_value()) {
    last_frame_ms_ = now_ms;
    return;
  }
  const std::uint32_t elapsed_ms = now_ms - *last_frame_ms_;
  last_frame_ms_ = now_ms;
  owed_ms_ += std::min(elapsed_ms, kMaxCatchUpMs);
  const auto tick_ms = static_cast<std::uint32_t>(kEffectTickMs);
  const std::uint32_t ticks = owed_ms_ / tick_ms;
  owed_ms_ %= tick_ms;
  if (ticks == 0) {
    ++idle_total_;
    return;
  }
  // Chaque tic franchi retient le compte du moment : les images sans tic qui
  // le suivent se comptent à partir de lui.
  const std::uint32_t remembered = std::min<std::uint32_t>(ticks, kRememberedTicks);
  tick_count_ += ticks - remembered;
  for (std::uint32_t tick = 0; tick < remembered; ++tick) {
    ++tick_count_;
    idle_total_at_tick_[tick_count_ % kRememberedTicks] = idle_total_;
  }
}

std::uint32_t TintRecoveryClock::IdleFramesSince(std::uint32_t ticks_since) const {
  // Au-delà de la mémoire, le plus ancien tic retenu : la teinte est de toute
  // façon revenue au blanc.
  const std::uint32_t back =
      std::min<std::uint32_t>(ticks_since, static_cast<std::uint32_t>(kRememberedTicks - 1));
  if (back > tick_count_) {
    return idle_total_;
  }
  return idle_total_ - idle_total_at_tick_[(tick_count_ - back) % kRememberedTicks];
}

}  // namespace ro::render

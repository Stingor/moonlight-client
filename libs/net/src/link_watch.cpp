#include "ro/net/link_watch.h"

namespace ro::net {
namespace {

// Le temps écoulé depuis `since_ms`. ⚠ JAMAIS NÉGATIF : un instant passé dans
// le désordre ne doit pas se lire comme quarante-neuf jours de silence.
[[nodiscard]] std::uint32_t ElapsedMs(std::uint32_t since_ms, std::uint32_t now_ms) noexcept {
  return now_ms >= since_ms ? now_ms - since_ms : 0;
}

}  // namespace

void LinkWatch::Restart(std::uint32_t now_ms) noexcept {
  heard_ms_ = now_ms;
  loop_ms_ = now_ms;
}

void LinkWatch::NoteLoop(std::uint32_t now_ms) noexcept {
  const bool stalled = loop_ms_.has_value() && ElapsedMs(*loop_ms_, now_ms) > kLoopStallMs;
  if (stalled || !heard_ms_.has_value()) {
    heard_ms_ = now_ms;
  }
  loop_ms_ = now_ms;
}

void LinkWatch::NoteReceived(std::uint32_t now_ms) noexcept { heard_ms_ = now_ms; }

LinkHealth LinkWatch::Health(std::uint32_t now_ms) const noexcept {
  if (!heard_ms_.has_value()) {
    return LinkHealth::kSteady;
  }
  const std::uint32_t silent_ms = ElapsedMs(*heard_ms_, now_ms);
  if (silent_ms >= kLinkSilentLossMs) {
    return LinkHealth::kLost;
  }
  if (silent_ms >= kLinkUnsteadyMs) {
    return LinkHealth::kUnsteady;
  }
  return LinkHealth::kSteady;
}

bool ZoneMoveOverdue(std::uint32_t started_ms, std::uint32_t now_ms) noexcept {
  return ElapsedMs(started_ms, now_ms) >= kLinkSilentLossMs;
}

}  // namespace ro::net

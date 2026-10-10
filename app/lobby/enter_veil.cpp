#include "lobby/enter_veil.h"

namespace moonclient::lobby {
namespace {

constexpr float kMsPerSecond = 1000.0F;

}  // namespace

void EnterVeil::Follow(bool entering, std::uint32_t now_ms) {
  if (!entering) {
    stage_ = Stage::kNone;
    return;
  }
  if (stage_ == Stage::kNone) {
    stage_ = Stage::kRising;
    since_ms_ = now_ms;
  }
}

void EnterVeil::Drawn(std::uint32_t now_ms, float fade_seconds) {
  if (stage_ == Stage::kRising && seconds(now_ms) >= fade_seconds) {
    stage_ = Stage::kCovering;
  }
}

float EnterVeil::seconds(std::uint32_t now_ms) const {
  if (stage_ == Stage::kNone) {
    return 0.0F;
  }
  return static_cast<float>(now_ms - since_ms_) / kMsPerSecond;
}

}  // namespace moonclient::lobby

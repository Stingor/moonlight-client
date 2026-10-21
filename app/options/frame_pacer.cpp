#include "options/frame_pacer.h"

namespace moonclient {

std::optional<std::uint64_t> FramePacer::NextFrameStartNs(std::uint64_t now_ns,
                                                          int frames_per_second) {
  if (frames_per_second <= 0) {
    anchored_frames_per_second_ = 0;
    return std::nullopt;
  }
  const auto per_second = static_cast<std::uint64_t>(frames_per_second);
  if (frames_per_second != anchored_frames_per_second_) {
    anchored_frames_per_second_ = frames_per_second;
    anchor_ns_ = now_ns;
    frames_since_anchor_ = 0;
  }
  ++frames_since_anchor_;
  if (frames_since_anchor_ == per_second) {
    anchor_ns_ += kNanosecondsPerSecond;
    frames_since_anchor_ = 0;
  }
  const std::uint64_t deadline_ns =
      anchor_ns_ + frames_since_anchor_ * kNanosecondsPerSecond / per_second;
  if (now_ns >= deadline_ns) {
    const std::uint64_t frame_ns = kNanosecondsPerSecond / per_second;
    if (now_ns - deadline_ns > frame_ns) {
      anchor_ns_ = now_ns;
      frames_since_anchor_ = 0;
    }
    return std::nullopt;
  }
  return deadline_ns;
}

}  // namespace moonclient

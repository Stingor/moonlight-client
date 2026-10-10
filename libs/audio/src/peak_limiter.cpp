#include "ro/audio/peak_limiter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace ro::audio {
namespace {

constexpr float kFullScale = 1.0F;
constexpr float kMillisecondsPerSecond = 1000.0F;

}  // namespace

void PeakLimiter::Process(std::span<float> pcm, int channels, int rate_hz) {
  if (channels <= 0 || rate_hz <= 0) {
    return;
  }
  if (rate_hz != timing_rate_hz_) {
    timing_rate_hz_ = rate_hz;
    const float frames_per_ms = static_cast<float>(rate_hz) / kMillisecondsPerSecond;
    hold_frames_ = static_cast<int>(std::ceil(kLimiterHoldMs * frames_per_ms));
    release_per_frame_ = std::exp(-1.0F / (kLimiterReleaseMs * frames_per_ms));
  }
  const auto width = static_cast<std::size_t>(channels);
  for (std::size_t first = 0; first + width <= pcm.size(); first += width) {
    const std::span<float> frame = pcm.subspan(first, width);
    float frame_peak = 0.0F;
    for (float& sample : frame) {
      sample *= kMixHeadroom;
      frame_peak = std::max(frame_peak, std::abs(sample));
    }
    if (frame_peak >= envelope_) {
      envelope_ = frame_peak;
      hold_frames_left_ = hold_frames_;
    } else if (hold_frames_left_ > 0) {
      --hold_frames_left_;
    } else {
      envelope_ = std::max(frame_peak, envelope_ * release_per_frame_);
    }
    last_gain_ = envelope_ > kFullScale ? kFullScale / envelope_ : 1.0F;
    if (last_gain_ < 1.0F) {
      // La borne rattrape l'arrondi du quotient, qui peut laisser la crête un
      // cheveu au-dessus de la pleine échelle.
      for (float& sample : frame) {
        sample = std::clamp(sample * last_gain_, -kFullScale, kFullScale);
      }
    }
  }
}

}  // namespace ro::audio

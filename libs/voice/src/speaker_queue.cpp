#include "ro/voice/speaker_queue.h"

#include <utility>

namespace ro::voice {

std::int64_t SpeakerQueue::Unwrap(std::uint16_t sequence) {
  if (!last_received_.has_value()) {
    last_received_ = sequence;
    last_sequence_ = sequence;
    return *last_received_;
  }
  const auto step =
      static_cast<std::int16_t>(static_cast<std::uint16_t>(sequence - last_sequence_));
  const std::int64_t unwrapped = *last_received_ + step;
  if (step > 0) {
    last_received_ = unwrapped;
    last_sequence_ = sequence;
  }
  return unwrapped;
}

void SpeakerQueue::Push(std::uint16_t sequence, std::span<const std::uint8_t> packet) {
  const std::int64_t at = Unwrap(sequence);
  // Trop tard : sa place est déjà jouée.
  if (playing_ && at < next_) return;
  queued_.insert_or_assign(at, std::vector<std::uint8_t>(packet.begin(), packet.end()));
  while (queued_.size() > kSpeakerMostQueuedFrames) {
    queued_.erase(queued_.begin());
  }
}

SpeakerFrame SpeakerQueue::Pop() {
  if (!playing_) {
    if (queued_.size() < kSpeakerPrimeFrames) return {};
    playing_ = true;
    missed_ = 0;
    next_ = queued_.begin()->first;
  }
  // Ce qui précède le paquet attendu ne se jouera plus.
  while (!queued_.empty() && queued_.begin()->first < next_) {
    queued_.erase(queued_.begin());
  }
  // UN TROU TROP LONG, OU UNE FILE PLEINE, ne se comble pas trame à trame : la
  // lecture saute au premier paquet présent et rattrape le direct.
  if (!queued_.empty()) {
    const std::int64_t gap = queued_.begin()->first - next_;
    if (gap > static_cast<std::int64_t>(kSpeakerMostMissedFrames) ||
        queued_.size() >= kSpeakerMostQueuedFrames) {
      next_ = queued_.begin()->first;
    }
  }
  SpeakerFrame frame;
  if (const auto expected = queued_.find(next_); expected != queued_.end()) {
    frame.step = SpeakerStep::kPacket;
    frame.packet = std::move(expected->second);
    queued_.erase(expected);
    missed_ = 0;
  } else if (!queued_.empty()) {
    frame.step = SpeakerStep::kLost;
    if (queued_.begin()->first == next_ + 1) {
      frame.next = queued_.begin()->second;
    }
  } else if (++missed_ >= kSpeakerMostMissedFrames) {
    playing_ = false;
    missed_ = 0;
  }
  ++next_;
  return frame;
}

}  // namespace ro::voice

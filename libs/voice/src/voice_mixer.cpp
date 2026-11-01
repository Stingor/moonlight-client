#include "ro/voice/voice_mixer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ro::voice {
namespace {

constexpr float kSampleCeiling = static_cast<float>(std::numeric_limits<std::int16_t>::max());

}  // namespace

void VoiceMixer::Hear(const HeardVoice& heard, std::uint32_t now_ms) {
  Speaker& speaker = speakers_[heard.speaker_id];
  if (!speaker.decoder.has_value()) {
    core::Result<OpusVoiceDecoder> made = OpusVoiceDecoder::Create();
    if (!made.has_value()) {
      speakers_.erase(heard.speaker_id);
      return;
    }
    speaker.decoder.emplace(std::move(*made));
  }
  if (now_ms - speaker.last_heard_ms > kVoiceSpeakingHoldMs || speaker.began_ms == 0) {
    speaker.began_ms = now_ms;
  }
  speaker.name = heard.name;
  speaker.volume = heard.volume;
  speaker.x = heard.x;
  speaker.last_heard_ms = now_ms;
  speaker.queue.Push(heard.sequence, heard.opus);
}

std::pair<float, float> VoiceMixer::SidesOf(const Speaker& speaker, const MixSettings& settings) {
  if (!settings.spatial || !settings.listener_x.has_value()) {
    return {1.0F, 1.0F};
  }
  const float offset = static_cast<float>(speaker.x - *settings.listener_x) / kVoicePanReachCells;
  const float pan = std::clamp(offset, -1.0F, 1.0F);
  // Le côté opposé s'efface, celui de la voix reste plein : au centre, les
  // deux le sont, et une voix ne perd rien à venir de face.
  return {std::min(1.0F, 1.0F - pan), std::min(1.0F, 1.0F + pan)};
}

void VoiceMixer::MixFrame(std::span<std::int16_t> stereo, const MixSettings& settings,
                          std::uint32_t now_ms) {
  std::fill(sum_.begin(), sum_.end(), 0.0F);
  for (auto it = speakers_.begin(); it != speakers_.end();) {
    Speaker& speaker = it->second;
    if (now_ms - speaker.last_heard_ms > kVoiceForgetSpeakerMs && !speaker.queue.playing()) {
      it = speakers_.erase(it);
      continue;
    }
    const SpeakerFrame frame = speaker.queue.Pop();
    bool sounded = false;
    if (frame.step == SpeakerStep::kPacket) {
      sounded = speaker.decoder->Decode(frame.packet, decoded_).has_value();
    } else if (frame.step == SpeakerStep::kLost) {
      sounded = speaker.decoder->Conceal(frame.next, decoded_).has_value();
    }
    // ⚠ UN MUET SE DÉCODE QUAND MÊME : son décodeur suit le fil, et sa voix
    // reprend juste quand on la rétablit.
    if (sounded && !settings.muted.contains(it->first)) {
      const float gain = speaker.volume * settings.output_gain;
      const auto [left, right] = SidesOf(speaker, settings);
      for (std::size_t at = 0; at < kVoiceFrameSamples; ++at) {
        const float sample = static_cast<float>(decoded_[at]) * gain;
        sum_[at * kVoiceOutputChannels] += sample * left;
        sum_[at * kVoiceOutputChannels + 1] += sample * right;
      }
    }
    ++it;
  }
  const std::size_t count = std::min(stereo.size(), sum_.size());
  for (std::size_t at = 0; at < count; ++at) {
    stereo[at] = static_cast<std::int16_t>(
        std::lround(std::clamp(sum_[at], -kSampleCeiling, kSampleCeiling)));
  }
  std::fill(stereo.begin() + static_cast<std::ptrdiff_t>(count), stereo.end(), std::int16_t{0});
}

std::vector<SpeakingVoice> VoiceMixer::speaking(std::uint32_t now_ms) const {
  std::vector<std::pair<std::uint32_t, SpeakingVoice>> heard;
  for (const auto& [speaker_id, speaker] : speakers_) {
    if (now_ms - speaker.last_heard_ms <= kVoiceSpeakingHoldMs) {
      heard.push_back(
          {speaker.began_ms, SpeakingVoice{.speaker_id = speaker_id, .name = speaker.name}});
    }
  }
  std::ranges::sort(heard, {}, [](const auto& entry) { return entry.first; });
  std::vector<SpeakingVoice> out;
  out.reserve(heard.size());
  for (auto& entry : heard) {
    out.push_back(std::move(entry.second));
  }
  return out;
}

}  // namespace ro::voice

// LE CODEC ET LE MIXAGE : une voix encodée puis décodée garde son énergie, se
// pose du côté d'où elle vient, se tait pour qui l'a rendue muette, et le nom
// de qui parle paraît puis s'efface.

#include "ro/voice/voice_mixer.h"

#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/voice/opus_codec.h"

using ro::voice::HeardVoice;
using ro::voice::kVoiceFrameMs;
using ro::voice::kVoiceFrameSamples;
using ro::voice::kVoiceOutputFrameSamples;
using ro::voice::MixSettings;
using ro::voice::OpusVoiceEncoder;
using ro::voice::VoiceMixer;

namespace {

// Un la à 440 Hz, assez fort pour se mesurer de loin.
constexpr double kToneHz = 440.0;
constexpr double kToneAmplitude = 8000.0;

[[nodiscard]] std::vector<std::int16_t> Tone(std::size_t frame_index) {
  std::vector<std::int16_t> samples(kVoiceFrameSamples);
  for (std::size_t at = 0; at < samples.size(); ++at) {
    const double t = static_cast<double>(frame_index * kVoiceFrameSamples + at) /
                     static_cast<double>(ro::voice::kVoiceSampleRate);
    samples[at] = static_cast<std::int16_t>(
        std::lround(kToneAmplitude * std::sin(2.0 * std::numbers::pi * kToneHz * t)));
  }
  return samples;
}

// L'énergie d'un côté d'une trame stéréo.
[[nodiscard]] double SideEnergy(const std::vector<std::int16_t>& stereo, std::size_t side) {
  double sum = 0.0;
  for (std::size_t at = side; at < stereo.size(); at += 2) {
    sum += static_cast<double>(stereo[at]) * static_cast<double>(stereo[at]);
  }
  return sum;
}

// Une voix de `frames` trames venue de la case `x`, entendue et jouée ; rend
// l'énergie des deux côtés sur les dernières trames.
struct Played {
  double left = 0.0;
  double right = 0.0;
};

[[nodiscard]] Played PlayTone(const MixSettings& settings, std::int32_t x) {
  auto encoder = OpusVoiceEncoder::Create();
  REQUIRE(encoder.has_value());
  VoiceMixer mixer;
  constexpr std::size_t kFrames = 12;
  std::uint32_t now_ms = 1000;
  for (std::size_t frame = 0; frame < kFrames; ++frame) {
    auto packet = encoder->Encode(Tone(frame));
    REQUIRE(packet.has_value());
    mixer.Hear(HeardVoice{.speaker_id = 7,
                          .volume = 1.0F,
                          .x = x,
                          .y = 0,
                          .name = "Sting",
                          .sequence = static_cast<std::uint16_t>(frame),
                          .opus = *packet},
               now_ms);
  }
  Played played;
  std::vector<std::int16_t> stereo(kVoiceOutputFrameSamples);
  for (std::size_t frame = 0; frame < kFrames; ++frame) {
    mixer.MixFrame(stereo, settings, now_ms);
    now_ms += kVoiceFrameMs;
    // Les premières trames portent la montée du décodeur.
    if (frame >= kFrames / 2) {
      played.left += SideEnergy(stereo, 0);
      played.right += SideEnergy(stereo, 1);
    }
  }
  return played;
}

}  // namespace

TEST_CASE("Voix : une voix de face s'entend des deux côtés", "[voice][mixer]") {
  MixSettings settings;
  settings.listener_x = 100;
  const Played played = PlayTone(settings, 100);
  CHECK(played.left > 0.0);
  CHECK(played.right > 0.0);
  CHECK(std::abs(played.left - played.right) < played.left * 0.01);
}

TEST_CASE("Voix : une voix à droite s'efface à gauche", "[voice][mixer]") {
  MixSettings settings;
  settings.listener_x = 100;
  const Played played = PlayTone(settings, 100 + 14);
  CHECK(played.right > 0.0);
  CHECK(played.left == 0.0);

  settings.spatial = false;
  const Played centered = PlayTone(settings, 100 + 14);
  CHECK(centered.left > 0.0);
}

TEST_CASE("Voix : un muet ne s'entend pas", "[voice][mixer]") {
  MixSettings settings;
  settings.muted = {7};
  const Played played = PlayTone(settings, 0);
  CHECK(played.left == 0.0);
  CHECK(played.right == 0.0);
}

TEST_CASE("Voix : qui parle paraît, puis s'efface", "[voice][mixer]") {
  VoiceMixer mixer;
  auto encoder = OpusVoiceEncoder::Create();
  REQUIRE(encoder.has_value());
  auto packet = encoder->Encode(Tone(0));
  REQUIRE(packet.has_value());
  mixer.Hear(HeardVoice{.speaker_id = 9, .volume = 1.0F, .name = "Ami", .opus = *packet}, 5000);
  REQUIRE(mixer.speaking(5000).size() == 1);
  CHECK(mixer.speaking(5000)[0].name == "Ami");
  CHECK(mixer.speaking(5000 + ro::voice::kVoiceSpeakingHoldMs + 1).empty());
}

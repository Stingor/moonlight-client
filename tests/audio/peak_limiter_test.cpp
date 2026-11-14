#include "ro/audio/peak_limiter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::audio::kLimiterReleaseMs;
using ro::audio::kMixHeadroom;
using ro::audio::PeakLimiter;

namespace {

constexpr int kRateHz = 48000;
constexpr int kStereo = 2;
constexpr float kMillisecondsPerSecond = 1000.0F;

// Une sinusoïde grave, comme le grondement de Quagmire, entrelacée sur deux
// voies égales, d'amplitude `peak`.
std::vector<float> Rumble(float peak, std::size_t frames) {
  constexpr float kFrequencyHz = 80.0F;
  std::vector<float> pcm;
  pcm.reserve(frames * kStereo);
  for (std::size_t frame = 0; frame < frames; ++frame) {
    const float phase = 2.0F * std::numbers::pi_v<float> * kFrequencyHz *
                        static_cast<float>(frame) / static_cast<float>(kRateHz);
    const float sample = peak * std::sin(phase);
    pcm.push_back(sample);
    pcm.push_back(sample);
  }
  return pcm;
}

// Ce que le mélangeur remet au limiteur : le mélange, réduit de la réserve.
std::vector<float> Reduced(std::vector<float> pcm) {
  for (float& sample : pcm) {
    sample /= kMixHeadroom;
  }
  return pcm;
}

float PeakOf(const std::vector<float>& pcm) {
  float peak = 0.0F;
  for (const float sample : pcm) {
    peak = std::max(peak, std::abs(sample));
  }
  return peak;
}

}  // namespace

TEST_CASE("un mélange sous la pleine échelle traverse le limiteur BIT POUR BIT",
          "[audio][limiter]") {
  // La preuve qu'un son propre n'est pas touché : réduit de la réserve par le
  // mélangeur, remonté par le limiteur, il ressort identique à l'échantillon.
  constexpr float kCleanPeak = 0.999F;
  constexpr std::size_t kFrames = 4800;
  const std::vector<float> clean = Rumble(kCleanPeak, kFrames);
  std::vector<float> pcm = Reduced(clean);

  PeakLimiter limiter;
  limiter.Process(pcm, kStereo, kRateHz);
  CHECK(pcm == clean);
  CHECK(limiter.last_gain() == 1.0F);
}

TEST_CASE("vingt-cinq copies alignées ne dépassent plus la pleine échelle", "[audio][limiter]") {
  // Le défaut mesuré : vingt-cinq copies d'un grondement à 0,513 de crête,
  // alignées à l'échantillon, font une crête à près de treize. Écrêtées, un
  // quart des échantillons restaient collés à la pleine échelle.
  constexpr float kQuagmirePeak = 0.513F;
  constexpr float kCopies = 25.0F;
  constexpr float kSummedPeak = kQuagmirePeak * kCopies;
  constexpr std::size_t kFrames = 4800;
  const std::vector<float> summed = Rumble(kSummedPeak, kFrames);
  std::vector<float> pcm = Reduced(summed);

  PeakLimiter limiter;
  limiter.Process(pcm, kStereo, kRateHz);
  CHECK(PeakOf(pcm) <= 1.0F);

  // UN GAIN, ET NON UN ÉCRÊTAGE : passé la première crête, la sortie est le
  // grondement entier, réduit d'un seul facteur — la forme d'onde est gardée.
  // Écrêté, un quart de ses échantillons serait collé à la pleine échelle.
  constexpr std::size_t kFirstCrestFrames = 150;
  constexpr float kTolerance = 1.0e-4F;
  const float gain = 1.0F / kSummedPeak;
  std::size_t deformed = 0;
  for (std::size_t index = kFirstCrestFrames * kStereo; index < pcm.size(); ++index) {
    if (std::abs(pcm[index] - summed[index] * gain) > kTolerance) {
      ++deformed;
    }
  }
  CHECK(deformed == 0);
  CHECK(limiter.last_gain() < 1.0F);
}

TEST_CASE("après un dépassement, le gain revient à un", "[audio][limiter]") {
  // Un éclat trop fort, puis un son propre : quelques constantes de temps plus
  // tard, le son propre ressort de nouveau inchangé.
  constexpr float kBurstPeak = 4.0F;
  constexpr float kCleanPeak = 0.5F;
  constexpr std::size_t kBurstFrames = 480;
  constexpr float kReleaseSpans = 10.0F;
  const auto recovery_frames = static_cast<std::size_t>(
      kReleaseSpans * kLimiterReleaseMs * static_cast<float>(kRateHz) / kMillisecondsPerSecond);

  PeakLimiter limiter;
  std::vector<float> burst = Reduced(Rumble(kBurstPeak, kBurstFrames));
  limiter.Process(burst, kStereo, kRateHz);
  CHECK(limiter.last_gain() < 1.0F);

  std::vector<float> tail = Reduced(Rumble(kCleanPeak, recovery_frames));
  limiter.Process(tail, kStereo, kRateHz);
  CHECK(limiter.last_gain() == 1.0F);
}

TEST_CASE("une disposition absurde ne touche à rien", "[audio][limiter]") {
  std::vector<float> pcm = {0.25F, -0.25F};
  const std::vector<float> before = pcm;
  PeakLimiter limiter;
  limiter.Process(pcm, 0, kRateHz);
  limiter.Process(pcm, kStereo, 0);
  CHECK(pcm == before);
}

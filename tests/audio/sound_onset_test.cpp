#include "ro/audio/sound_onset.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "support/wav_builder.h"

using ro::audio::kOnsetMergeWindowMs;
using ro::audio::kOpeningStepThreshold;
using ro::audio::MergedOnset;
using ro::audio::MergeOnset;
using ro::audio::OpeningStep;
using ro::audio::SameOnset;
using ro::test_support::BuildWav;

namespace {

constexpr int kRateHz = 22050;
constexpr float kSixteenBitScale = 32768.0F;

}  // namespace

TEST_CASE("deux départs du même bruit dans la fenêtre n'en font qu'un", "[audio][onset]") {
  constexpr std::uint64_t kStartMs = 1000;
  CHECK(SameOnset(kStartMs, kStartMs));
  CHECK(SameOnset(kStartMs, kStartMs + kOnsetMergeWindowMs));
  CHECK_FALSE(SameOnset(kStartMs, kStartMs + kOnsetMergeWindowMs + 1));
  // Une horloge qui recule ne fusionne pas.
  CHECK_FALSE(SameOnset(kStartMs, kStartMs - 1));
}

TEST_CASE("vingt-cinq cases égales valent cinq fois une seule, pas vingt-cinq", "[audio][onset]") {
  // Le cas de Quagmire : vingt-cinq cases, toutes à plein volume sous
  // l'auditeur. Sommées en amplitude, elles donneraient vingt-cinq ; en
  // énergie, cinq — et le gain de la voix est ensuite plafonné à un.
  constexpr int kCells = 25;
  constexpr float kExpectedGain = 5.0F;
  MergedOnset held{.gain = 1.0F, .pan = 0.0F};
  for (int cell = 1; cell < kCells; ++cell) {
    held = MergeOnset(held.gain, held.pan, 1.0F, 0.0F);
  }
  CHECK(held.gain == Catch::Approx(kExpectedGain));
  CHECK(held.pan == Catch::Approx(0.0F));
}

TEST_CASE("fusionner un à un revient à fusionner d'un coup, le plus fort tirant le côté",
          "[audio][onset]") {
  // Trois départs : fort à droite, faible à gauche, moyen au centre.
  constexpr std::array<float, 3> kGains = {0.8F, 0.2F, 0.5F};
  constexpr std::array<float, 3> kPans = {1.0F, -1.0F, 0.0F};
  MergedOnset held{.gain = kGains[0], .pan = kPans[0]};
  held = MergeOnset(held.gain, held.pan, kGains[1], kPans[1]);
  held = MergeOnset(held.gain, held.pan, kGains[2], kPans[2]);

  float energy = 0.0F;
  float weighted_pan = 0.0F;
  for (std::size_t index = 0; index < kGains.size(); ++index) {
    energy += kGains[index] * kGains[index];
    weighted_pan += kGains[index] * kGains[index] * kPans[index];
  }
  CHECK(held.gain == Catch::Approx(std::sqrt(energy)));
  CHECK(held.pan == Catch::Approx(weighted_pan / energy));
  CHECK(held.pan > 0.0F);
}

TEST_CASE("fusionner des départs muets ne divise pas par zéro", "[audio][onset]") {
  const MergedOnset merged = MergeOnset(0.0F, 0.3F, 0.0F, -0.5F);
  CHECK(merged.gain == 0.0F);
  CHECK(merged.pan == -0.5F);
}

TEST_CASE("le premier échantillon d'un bruit se lit en 16 et en 8 bits", "[audio][onset]") {
  // Le relevé de `wizard_quagmire.wav` : 16 bits, 22 050 Hz, mono, et un
  // premier échantillon à 4 411 — 0,1346 de la pleine échelle.
  constexpr std::int16_t kQuagmireFirstSample = 4411;
  const std::vector<std::int16_t> quagmire = {kQuagmireFirstSample, 4000, 3000};
  CHECK(OpeningStep(BuildWav(quagmire, kRateHz)) ==
        Catch::Approx(static_cast<float>(kQuagmireFirstSample) / kSixteenBitScale));
  CHECK(OpeningStep(BuildWav(quagmire, kRateHz)) > kOpeningStepThreshold);

  // Un bruit qui part du silence ne claque pas.
  const std::vector<std::int16_t> silent_start = {0, 12000, -12000};
  CHECK(OpeningStep(BuildWav(silent_start, kRateHz)) == 0.0F);

  // En 8 bits, le silence vaut 128 : 192 est à mi-course.
  constexpr std::uint16_t kEightBits = 8;
  const std::vector<std::int16_t> eight_bit = {192, 128};
  CHECK(OpeningStep(BuildWav(eight_bit, kRateHz, 1, kEightBits)) == Catch::Approx(0.5F));

  // En stéréo, la plus forte des deux voies.
  constexpr std::uint16_t kStereo = 2;
  const std::vector<std::int16_t> stereo = {100, -16384, 0, 0};
  CHECK(OpeningStep(BuildWav(stereo, kRateHz, kStereo)) == Catch::Approx(0.5F));
}

TEST_CASE("ce qui n'est pas du PCM lisible ne claque jamais", "[audio][onset]") {
  const std::vector<std::int16_t> loud = {30000, 30000};
  // L'ADPCM de Microsoft : on ne sait pas lire son premier échantillon.
  constexpr std::uint16_t kAdpcmTag = 2;
  constexpr std::uint16_t kSixteenBits = 16;
  CHECK(OpeningStep(BuildWav(loud, kRateHz, 1, kSixteenBits, kAdpcmTag)) == 0.0F);
  // Des octets qui ne sont pas un WAV, et un WAV tronqué avant ses données.
  const std::vector<std::uint8_t> garbage = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
  CHECK(OpeningStep(garbage) == 0.0F);
  std::vector<std::uint8_t> truncated = BuildWav(loud, kRateHz);
  constexpr std::size_t kCutBeforeData = 40;
  truncated.resize(kCutBeforeData);
  CHECK(OpeningStep(truncated) == 0.0F);
}

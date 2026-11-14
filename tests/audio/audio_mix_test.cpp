// La chaîne de mélange ENTIÈRE, sans carte son : un mélangeur sans
// périphérique, que l'essai fait tourner et dont il lit la sortie. Les bruits
// sont des WAV fabriqués en mémoire, reproduisant ce qui a été mesuré sur
// `wizard_quagmire.wav` : 16 bits, 22 050 Hz, mono, un grondement grave à
// 0,513 de crête, et un premier échantillon à 0,1346.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/audio/audio_engine.h"
#include "ro/audio/sound_onset.h"

#include "support/wav_builder.h"

using ro::audio::AudioEngine;
using ro::audio::SoundChannel;
using ro::test_support::BuildWav;

namespace {

constexpr int kRateHz = 22050;
constexpr int kStereo = 2;
constexpr float kSixteenBitScale = 32768.0F;
constexpr std::size_t kFrames = 2205;
constexpr float kRumbleHz = 80.0F;

// Un grondement de `kFrames` trames, de crête `peak`, qui démarre à la phase
// `start_phase` : zéro pour un bruit qui part du silence.
std::vector<std::int16_t> Rumble(float peak, float start_phase) {
  std::vector<std::int16_t> samples;
  samples.reserve(kFrames);
  for (std::size_t frame = 0; frame < kFrames; ++frame) {
    const float phase = start_phase + 2.0F * std::numbers::pi_v<float> * kRumbleHz *
                                          static_cast<float>(frame) / static_cast<float>(kRateHz);
    samples.push_back(
        static_cast<std::int16_t>(std::lround(peak * kSixteenBitScale * std::sin(phase))));
  }
  return samples;
}

// Le grondement de Quagmire : sa crête et son premier échantillon, mesurés.
std::vector<std::int16_t> QuagmireRumble() {
  constexpr float kPeak = 0.513F;
  constexpr float kFirstSample = 0.1346F;
  return Rumble(kPeak, std::asin(kFirstSample / kPeak));
}

// Ouvre un mélangeur sans périphérique, ou passe l'essai si la bibliothèque ne
// sait pas en ouvrir un sur cette machine.
std::optional<AudioEngine> OfflineEngine() {
  auto opened = AudioEngine::OpenOffline(kRateHz, kStereo);
  if (!opened) {
    return std::nullopt;
  }
  return std::move(*opened);
}

std::vector<float> RenderFrames(AudioEngine& engine, std::size_t frames) {
  std::vector<float> pcm(frames * kStereo, 0.0F);
  REQUIRE(engine.Render(pcm));
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

TEST_CASE("un bruit propre traverse toute la chaîne BIT POUR BIT", "[audio][mix]") {
  // La preuve que la réserve et le limiteur n'ont aucun effet sur un son qui
  // tient sous la pleine échelle : ce qui sort est l'échantillon du fichier,
  // divisé par 32 768, sur les deux voies.
  auto engine = OfflineEngine();
  if (!engine) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  constexpr float kCleanPeak = 0.9F;
  const std::vector<std::int16_t> clean = Rumble(kCleanPeak, 0.0F);
  REQUIRE(engine->LoadSound("clean", BuildWav(clean, kRateHz)).has_value());
  engine->PlaySound("clean", 1.0F, 0.0F, SoundChannel::kEffects);

  const std::vector<float> pcm = RenderFrames(*engine, kFrames);
  std::size_t differing = 0;
  for (std::size_t frame = 0; frame < kFrames; ++frame) {
    const float expected = static_cast<float>(clean[frame]) / kSixteenBitScale;
    if (pcm[frame * kStereo] != expected || pcm[frame * kStereo + 1] != expected) {
      ++differing;
    }
  }
  CHECK(differing == 0);
}

TEST_CASE("les vingt-cinq cases de Quagmire sonnent comme une seule, sans saturer",
          "[audio][mix]") {
  // LE DÉFAUT ENTENDU : vingt-cinq cases lancent le même bruit au même tic.
  // Sommées, les copies alignées montaient à près de treize fois la pleine
  // échelle, et prenaient vingt-cinq voix sur trente-deux.
  auto together = OfflineEngine();
  auto alone = OfflineEngine();
  if (!together || !alone) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  const std::vector<std::uint8_t> wav = BuildWav(QuagmireRumble(), kRateHz);
  REQUIRE(together->LoadSound("quagmire", wav).has_value());
  REQUIRE(alone->LoadSound("quagmire", wav).has_value());

  constexpr int kCells = 25;
  for (int cell = 0; cell < kCells; ++cell) {
    together->PlaySound("quagmire", 1.0F, 0.0F, SoundChannel::kEffects);
  }
  alone->PlaySound("quagmire", 1.0F, 0.0F, SoundChannel::kEffects);

  const std::vector<float> merged = RenderFrames(*together, kFrames);
  const std::vector<float> single = RenderFrames(*alone, kFrames);
  CHECK(PeakOf(merged) <= 1.0F);
  // Plafonnée à la pleine échelle, la somme des énergies rend exactement une
  // voix à plein volume.
  CHECK(merged == single);
}

TEST_CASE("deux bruits forts distincts se superposent sans dépasser la pleine échelle",
          "[audio][mix]") {
  // Deux bruits DIFFÉRENTS ne fusionnent pas : c'est le limiteur qui les
  // tient. Leurs crêtes s'ajoutent à 1,8.
  auto engine = OfflineEngine();
  if (!engine) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  constexpr float kLoudPeak = 0.9F;
  const std::vector<std::uint8_t> wav = BuildWav(Rumble(kLoudPeak, 0.0F), kRateHz);
  REQUIRE(engine->LoadSound("first", wav).has_value());
  REQUIRE(engine->LoadSound("second", wav).has_value());
  engine->PlaySound("first", 1.0F, 0.0F, SoundChannel::kEffects);
  engine->PlaySound("second", 1.0F, 0.0F, SoundChannel::kEffects);

  const std::vector<float> pcm = RenderFrames(*engine, kFrames);
  CHECK(PeakOf(pcm) <= 1.0F);
  // Et ce n'est pas le silence : le mélange monte bien à la pleine échelle.
  constexpr float kAlmostFull = 0.99F;
  CHECK(PeakOf(pcm) > kAlmostFull);
}

TEST_CASE("un bruit qui commence en pleine forme d'onde entre en fondu", "[audio][mix]") {
  // Le premier échantillon de Quagmire saute du silence à 0,1346 : un
  // claquement à chaque départ. Il part désormais de zéro, et rejoint le
  // fichier une fois le fondu passé.
  auto engine = OfflineEngine();
  if (!engine) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  const std::vector<std::int16_t> quagmire = QuagmireRumble();
  REQUIRE(engine->LoadSound("quagmire", BuildWav(quagmire, kRateHz)).has_value());
  engine->PlaySound("quagmire", 1.0F, 0.0F, SoundChannel::kEffects);

  const std::vector<float> pcm = RenderFrames(*engine, kFrames);
  constexpr float kNoStep = 0.01F;
  CHECK(std::abs(pcm[0]) < kNoStep);
  // Passé le fondu, le fichier tel quel.
  constexpr std::size_t kMillisecondsPerSecond = 1000;
  const std::size_t after_fade =
      2 * static_cast<std::size_t>(ro::audio::kOpeningFadeMs) * kRateHz / kMillisecondsPerSecond;
  const float expected = static_cast<float>(quagmire[after_fade]) / kSixteenBitScale;
  CHECK(pcm[after_fade * kStereo] == expected);
}

TEST_CASE("faire taire l'ambiance laisse jouer les autres familles", "[audio][mix]") {
  // LE DÉFAUT ENTENDU : l'eau et la foule d'une carte sonnaient encore sur la
  // suivante, jusqu'à la fin de leur fichier. Une fois le fondu passé, ce qui
  // sort est exactement ce que le bruit de sprite rend seul.
  auto both = OfflineEngine();
  auto sprite_only = OfflineEngine();
  if (!both || !sprite_only) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  constexpr float kQuietPeak = 0.3F;
  const std::vector<std::uint8_t> wav = BuildWav(Rumble(kQuietPeak, 0.0F), kRateHz);
  REQUIRE(both->LoadSound("water", wav).has_value());
  REQUIRE(both->LoadSound("sword", wav).has_value());
  REQUIRE(sprite_only->LoadSound("sword", wav).has_value());
  both->PlaySound("water", 1.0F, 0.0F, SoundChannel::kAmbience);
  both->PlaySound("sword", 1.0F, 0.0F, SoundChannel::kSprites);
  sprite_only->PlaySound("sword", 1.0F, 0.0F, SoundChannel::kSprites);

  constexpr std::size_t kBeforeStop = kFrames / 5;
  const std::vector<float> heard = RenderFrames(*both, kBeforeStop);
  REQUIRE(PeakOf(heard) > PeakOf(RenderFrames(*sprite_only, kBeforeStop)));

  both->StopSounds(SoundChannel::kAmbience);
  const std::vector<float> after = RenderFrames(*both, kFrames - kBeforeStop);
  const std::vector<float> alone = RenderFrames(*sprite_only, kFrames - kBeforeStop);
  constexpr std::size_t kMillisecondsPerSecond = 1000;
  const std::size_t after_fade =
      2 * static_cast<std::size_t>(ro::audio::kSilencingFadeMs) * kRateHz / kMillisecondsPerSecond;
  REQUIRE(after_fade < after.size() / kStereo);
  CHECK(std::equal(after.begin() + static_cast<std::ptrdiff_t>(after_fade * kStereo), after.end(),
                   alone.begin() + static_cast<std::ptrdiff_t>(after_fade * kStereo)));
  // Et le bruit de sprite n'a pas été coupé avec : il sonne encore.
  CHECK(PeakOf(after) > 0.0F);
}

TEST_CASE("un bruit oublié sans avoir joué ne se joue plus", "[audio][mix]") {
  auto engine = OfflineEngine();
  if (!engine) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  constexpr float kPeak = 0.5F;
  REQUIRE(engine->LoadSound("beep", BuildWav(Rumble(kPeak, 0.0F), kRateHz)).has_value());
  engine->ForgetSound("beep");
  CHECK_FALSE(engine->KnowsSound("beep"));
  CHECK(engine->HeldSoundNames().empty());
  engine->PlaySound("beep", 1.0F, 0.0F, SoundChannel::kEffects);
  CHECK(PeakOf(RenderFrames(*engine, kFrames)) == 0.0F);
  // Un nom inconnu s'oublie sans rien faire.
  engine->ForgetSound("beep");
}

TEST_CASE("un bruit oublié PENDANT qu'il joue s'achève tel quel", "[audio][mix]") {
  auto forgotten = OfflineEngine();
  auto kept = OfflineEngine();
  if (!forgotten || !kept) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  constexpr float kPeak = 0.5F;
  const std::vector<std::uint8_t> wav = BuildWav(Rumble(kPeak, 0.0F), kRateHz);
  REQUIRE(forgotten->LoadSound("beep", wav).has_value());
  REQUIRE(kept->LoadSound("beep", wav).has_value());
  forgotten->PlaySound("beep", 1.0F, 0.0F, SoundChannel::kEffects);
  kept->PlaySound("beep", 1.0F, 0.0F, SoundChannel::kEffects);

  constexpr std::size_t kBeforeForget = kFrames / 4;
  REQUIRE(RenderFrames(*forgotten, kBeforeForget) == RenderFrames(*kept, kBeforeForget));
  forgotten->ForgetSound("beep");
  CHECK_FALSE(forgotten->KnowsSound("beep"));
  const std::vector<float> rest = RenderFrames(*forgotten, kFrames - kBeforeForget);
  CHECK(PeakOf(rest) > 0.0F);
  CHECK(rest == RenderFrames(*kept, kFrames - kBeforeForget));
}

TEST_CASE("une voix qui achevait un bruit oublié rend son décodé au bruit suivant",
          "[audio][mix]") {
  // La première voix libre sert toujours : sans détachement, la seconde voix
  // garderait le bruit oublié aussi longtemps que la première suffit.
  auto engine = OfflineEngine();
  if (!engine) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  constexpr float kPeak = 0.25F;
  const std::vector<std::uint8_t> wav = BuildWav(Rumble(kPeak, 0.0F), kRateHz);
  REQUIRE(engine->LoadSound("kept", wav).has_value());
  REQUIRE(engine->LoadSound("forgotten", wav).has_value());
  engine->PlaySound("kept", 1.0F, 0.0F, SoundChannel::kEffects);
  engine->PlaySound("forgotten", 1.0F, 0.0F, SoundChannel::kEffects);
  engine->ForgetSound("forgotten");
  CHECK(engine->VoicesHoldingForgottenSounds() == 1);

  // Les deux achèvent : la voix du bruit oublié le tient encore.
  (void)RenderFrames(*engine, kFrames * 2);
  CHECK(engine->VoicesHoldingForgottenSounds() == 1);
  // Le bruit suivant prend la première voix, et la seconde rend le sien.
  engine->PlaySound("kept", 1.0F, 0.0F, SoundChannel::kEffects);
  CHECK(engine->VoicesHoldingForgottenSounds() == 0);
}

TEST_CASE("un bruit oublié se recharge sous le même nom, et c'est le nouveau qui joue",
          "[audio][mix]") {
  auto engine = OfflineEngine();
  auto fresh = OfflineEngine();
  if (!engine || !fresh) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  constexpr float kOldPeak = 0.5F;
  constexpr float kNewPeak = 0.25F;
  const std::vector<std::uint8_t> new_wav = BuildWav(Rumble(kNewPeak, 0.0F), kRateHz);
  REQUIRE(engine->LoadSound("beep", BuildWav(Rumble(kOldPeak, 0.0F), kRateHz)).has_value());
  engine->PlaySound("beep", 1.0F, 0.0F, SoundChannel::kEffects);
  // Le premier joue jusqu'au bout : sa voix est au repos quand il s'oublie.
  (void)RenderFrames(*engine, kFrames * 2);

  engine->ForgetSound("beep");
  REQUIRE(engine->LoadSound("beep", new_wav).has_value());
  CHECK(engine->HeldSoundNames().size() == 1);
  REQUIRE(fresh->LoadSound("beep", new_wav).has_value());
  engine->PlaySound("beep", 1.0F, 0.0F, SoundChannel::kEffects);
  fresh->PlaySound("beep", 1.0F, 0.0F, SoundChannel::kEffects);
  CHECK(RenderFrames(*engine, kFrames) == RenderFrames(*fresh, kFrames));
}

TEST_CASE("un bruit retenu dit sa durée décodée ; un nom inconnu ou oublié n'en a pas",
          "[audio][mix]") {
  // Le grondement compte 2 205 trames à 22 050 Hz : un dixième de seconde.
  constexpr std::uint64_t kRumbleLengthMs = 100;
  auto engine = OfflineEngine();
  if (!engine) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  REQUIRE(engine->LoadSound("rumble", BuildWav(QuagmireRumble(), kRateHz)).has_value());
  REQUIRE(engine->SoundLengthMs("rumble").has_value());
  CHECK(*engine->SoundLengthMs("rumble") == kRumbleLengthMs);
  CHECK_FALSE(engine->SoundLengthMs("absent").has_value());

  engine->ForgetSound("rumble");
  CHECK_FALSE(engine->SoundLengthMs("rumble").has_value());
}

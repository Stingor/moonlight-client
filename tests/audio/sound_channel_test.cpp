// LES FAMILLES DE BRUITS : chacune sous le volume général, qu'elle multiplie.

#include "ro/audio/sound_channel.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using ro::audio::MixedSoundGain;
using ro::audio::VolumeAmplitude;

namespace {

constexpr float kHalf = 0.5F;

}  // namespace

TEST_CASE("tout à fond, le bruit sonne comme le site le demande", "[audio][channel]") {
  // C'est la conduite du client d'origine, qui n'a qu'un volume.
  CHECK(MixedSoundGain(0.7F, 1.0F, 1.0F) == Approx(0.7F));
}

TEST_CASE("le volume général reste un maître", "[audio][channel]") {
  // À zéro, plus rien ne sonne, quelle que soit la famille.
  CHECK(MixedSoundGain(1.0F, 0.0F, 1.0F) == 0.0F);
  // Et une famille à zéro se tait seule.
  CHECK(MixedSoundGain(1.0F, 1.0F, 0.0F) == 0.0F);
}

TEST_CASE("la famille suit la même courbe que le général", "[audio][channel]") {
  // Un curseur à moitié rend un huitième : les deux curseurs à moitié rendent
  // donc un soixante-quatrième, et non un quart.
  CHECK(VolumeAmplitude(kHalf) == Approx(kHalf * kHalf * kHalf));
  CHECK(MixedSoundGain(1.0F, kHalf, kHalf) == Approx(1.0F / 64.0F));
}

TEST_CASE("chaque famille a son rang, et elles sont quatre", "[audio][channel]") {
  CHECK(ro::audio::IndexOf(ro::audio::SoundChannel::kSprites) == 0U);
  CHECK(ro::audio::IndexOf(ro::audio::SoundChannel::kEffects) == 1U);
  CHECK(ro::audio::IndexOf(ro::audio::SoundChannel::kAmbience) == 2U);
  CHECK(ro::audio::IndexOf(ro::audio::SoundChannel::kInterface) == 3U);
  CHECK(ro::audio::kSoundChannelCount == 4U);
  // Chacune part à fond : tous les curseurs à fond rendent le client d'origine.
  for (const float volume : ro::audio::kFullSoundChannelVolumes) {
    CHECK(volume == 1.0F);
  }
}

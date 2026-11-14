// LES SONS DE L'INTERFACE : leur famille, son volume, le son de bouton lu sous
// son nom coréen, et l'échec dit une seule fois. Les sons sont joués sur un
// mélangeur sans périphérique, avec des WAV fabriqués en mémoire.

#include "game/interface_sound.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/audio/audio_engine.h"
#include "ro/audio/sound_channel.h"
#include "ro/core/log.h"
#include "ro/vfs/vfs.h"

#include "support/grf_builder.h"
#include "support/temp_directory.h"
#include "support/wav_builder.h"

using moonclient::ButtonSoundFile;
using moonclient::InterfaceSoundFile;
using moonclient::InterfaceSoundPlace;
using moonclient::PlayInterfaceSound;
using moonclient::PlayInterfaceSounds;
using ro::audio::AudioEngine;
using ro::audio::SoundChannel;
using ro::test_support::TempDirectory;

namespace {

constexpr int kRateHz = 22050;
constexpr int kStereo = 2;
// Un dixième de seconde de son, rendu deux fois plus longtemps : de quoi
// l'entendre en entier.
constexpr std::size_t kBeepFrames = kRateHz / 10;
constexpr std::size_t kRenderedFrames = 2 * kBeepFrames;
constexpr std::int16_t kBeepSample = 1000;
constexpr char kArchiveName[] = "data.grf";

// « 버튼소리.wav » sous le dossier des sons, en CP949 : le nom que porte
// l'archive.
const std::string kButtonCp949 = {static_cast<char>(0xB9), static_cast<char>(0xF6),
                                  static_cast<char>(0xC6), static_cast<char>(0xB0),
                                  static_cast<char>(0xBC), static_cast<char>(0xD2),
                                  static_cast<char>(0xB8), static_cast<char>(0xAE)};

[[nodiscard]] std::optional<AudioEngine> OfflineEngine() {
  auto opened = AudioEngine::OpenOffline(kRateHz, kStereo);
  if (!opened) {
    return std::nullopt;
  }
  return std::move(*opened);
}

[[nodiscard]] std::vector<std::uint8_t> ShortBeep() {
  const std::vector<std::int16_t> samples(kBeepFrames, kBeepSample);
  return ro::test_support::BuildWav(samples, kRateHz);
}

// Une installation dont l'archive porte le son de bouton sous son nom CP949.
[[nodiscard]] ro::vfs::Vfs ButtonInArchive(const TempDirectory& install) {
  install.WriteFile(kArchiveName,
                    ro::test_support::BuildGrf(
                        {{.name = "data\\wav\\" + kButtonCp949 + ".wav", .content = ShortBeep()}}));
  auto vfs = ro::vfs::Vfs::Mount(
      ro::vfs::MountOptions{.root = install.path(), .archives = {kArchiveName}});
  REQUIRE(vfs.has_value());
  return std::move(*vfs);
}

// Le son de bouton joué sur un mélangeur neuf, et la crête de ce qui en sort.
[[nodiscard]] float ButtonPeak(AudioEngine& audio, ro::vfs::Vfs& vfs,
                               const std::filesystem::path& root) {
  REQUIRE(PlayInterfaceSound(audio, vfs, root, ButtonSoundFile()).has_value());
  std::vector<float> pcm(kRenderedFrames * kStereo, 0.0F);
  REQUIRE(audio.Render(pcm));
  float peak = 0.0F;
  for (const float sample : pcm) {
    peak = std::max(peak, std::abs(sample));
  }
  return peak;
}

}  // namespace

TEST_CASE("Sons d'interface : le bouton se lit sous son nom coréen, dans sa famille",
          "[app][son]") {
  const TempDirectory install;
  ro::vfs::Vfs vfs = ButtonInArchive(install);

  // Les trois familles du monde à zéro : le son d'interface n'en dépend pas.
  auto heard = OfflineEngine();
  if (!heard) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  heard->SetChannelVolume(SoundChannel::kSprites, 0.0F);
  heard->SetChannelVolume(SoundChannel::kEffects, 0.0F);
  heard->SetChannelVolume(SoundChannel::kAmbience, 0.0F);
  CHECK(ButtonPeak(*heard, vfs, install.path()) > 0.0F);

  // Sa famille à zéro : il se tait.
  auto muted = OfflineEngine();
  REQUIRE(muted.has_value());
  muted->SetChannelVolume(SoundChannel::kInterface, 0.0F);
  CHECK(ButtonPeak(*muted, vfs, install.path()) == 0.0F);
}

TEST_CASE("Sons d'interface : le volume de leur famille suit la courbe des curseurs",
          "[app][son]") {
  const TempDirectory install;
  ro::vfs::Vfs vfs = ButtonInArchive(install);
  auto full = OfflineEngine();
  auto half = OfflineEngine();
  if (!full || !half) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  constexpr float kHalf = 0.5F;
  // Un pour cent : l'arrondi du mélange sur seize bits.
  constexpr double kMixTolerance = 0.01;
  half->SetChannelVolume(SoundChannel::kInterface, kHalf);
  const float full_peak = ButtonPeak(*full, vfs, install.path());
  REQUIRE(full_peak > 0.0F);
  CHECK(ButtonPeak(*half, vfs, install.path()) ==
        Catch::Approx(full_peak * ro::audio::VolumeAmplitude(kHalf)).epsilon(kMixTolerance));
}

#ifdef _WIN32
TEST_CASE("Sons d'interface : le bouton posé sur le disque se lit sous ses deux orthographes",
          "[app][son]") {
  auto audio = OfflineEngine();
  if (!audio) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  // Le vrai nom Unicode, celui qu'écrit un outil d'extraction moderne.
  {
    const TempDirectory install;
    install.WriteFile(std::filesystem::path(u8"data/wav/버튼소리.wav"), ShortBeep());
    auto vfs = ro::vfs::Vfs::Mount(ro::vfs::MountOptions{.root = install.path()});
    REQUIRE(vfs.has_value());
    CHECK(PlayInterfaceSound(*audio, *vfs, install.path(), ButtonSoundFile()).has_value());
  }
  // Les octets CP949 relus dans la page de code du système, celle qu'écrit le
  // client d'origine en extrayant ses archives.
  audio->ForgetSound(ButtonSoundFile().path);
  {
    const TempDirectory install;
    install.WriteFile(std::filesystem::path("data/wav/" + kButtonCp949 + ".wav"), ShortBeep());
    auto vfs = ro::vfs::Vfs::Mount(ro::vfs::MountOptions{.root = install.path()});
    REQUIRE(vfs.has_value());
    CHECK(PlayInterfaceSound(*audio, *vfs, install.path(), ButtonSoundFile()).has_value());
  }
}
#endif

TEST_CASE("Sons d'interface : l'échec d'un son qui manque ne se dit qu'une fois", "[app][son]") {
  const TempDirectory install;
  ro::vfs::Vfs vfs = ButtonInArchive(install);
  auto audio = OfflineEngine();
  if (!audio) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  const InterfaceSoundFile missing{.place = InterfaceSoundPlace::kSoundArchive,
                                   .path = "absent.wav"};
  const std::array<InterfaceSoundFile, 3> asked = {missing, ButtonSoundFile(), missing};
  std::vector<InterfaceSoundFile> failures_said;
  const std::size_t lines_before = ro::core::LoggedLineCount();
  PlayInterfaceSounds(*audio, &vfs, install.path(), asked, "d'essai", failures_said);
  PlayInterfaceSounds(*audio, &vfs, install.path(), asked, "d'essai", failures_said);
  CHECK(failures_said == std::vector<InterfaceSoundFile>{missing});
  std::size_t lines_said = 0;
  for (const ro::core::LoggedLine& line : ro::core::RecentLogLines()) {
    if (line.serial > lines_before && line.text.starts_with("son d'essai")) {
      ++lines_said;
    }
  }
  CHECK(lines_said == 1);
}

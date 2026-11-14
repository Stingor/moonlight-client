// LE SON DE L'ALERTE DU CARNET MVP : le fichier de chaque choix, le nom
// personnel tenu dans `savedata\`, et l'échec qui ne se journalise qu'une fois.
// Les sons joués le sont sur un mélangeur sans périphérique, avec des WAV
// fabriqués en mémoire.

#include "game/mvp_alert_sound.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "game/interface_sound.h"
#include "options/mvp_tracker_options.h"
#include "support/temp_directory.h"
#include "support/wav_builder.h"

using moonclient::ButtonSoundFile;
using moonclient::InterfaceSoundFile;
using moonclient::InterfaceSoundPlace;
using moonclient::MessengerSoundFile;
using moonclient::MvpAlertSoundOutcome;
using moonclient::MvpAlertTone;
using moonclient::MvpAlertToneFile;
using moonclient::MvpAlertTrouble;
using moonclient::PlayMvpAlertSound;
using moonclient::SaveFolderSoundFile;
using ro::test_support::TempDirectory;

namespace {

constexpr int kRateHz = 22050;
constexpr int kStereo = 2;
constexpr int kCustom = static_cast<int>(MvpAlertTone::kCustom);

[[nodiscard]] std::optional<ro::audio::AudioEngine> OfflineEngine() {
  auto opened = ro::audio::AudioEngine::OpenOffline(kRateHz, kStereo);
  if (!opened) {
    return std::nullopt;
  }
  return std::move(*opened);
}

// Une installation sans archive : tout se lit sur son disque.
[[nodiscard]] ro::vfs::Vfs MountedOn(const TempDirectory& install) {
  install.WriteText("data/readme.txt", "vide");
  auto vfs = ro::vfs::Vfs::Mount(ro::vfs::MountOptions{.root = install.path()});
  REQUIRE(vfs.has_value());
  return std::move(*vfs);
}

[[nodiscard]] std::vector<std::uint8_t> ShortBeep() {
  const std::vector<std::int16_t> samples(kRateHz / 10, std::int16_t{1000});
  return ro::test_support::BuildWav(samples, kRateHz);
}

}  // namespace

TEST_CASE("Son de l'alerte MVP : le fichier de chaque choix", "[app][mvp]") {
  // Le son du système n'a pas de fichier tant qu'il existe ; sans lui, c'est
  // « Nouveau message ».
  CHECK(MvpAlertToneFile(0, true, {}).place == InterfaceSoundPlace::kNowhere);
  CHECK(MvpAlertToneFile(0, false, {}) == MessengerSoundFile());
  CHECK(MvpAlertToneFile(1, true, {}) == MessengerSoundFile());
  CHECK(MvpAlertToneFile(2, true, {}) == ButtonSoundFile());
  CHECK(MvpAlertToneFile(kCustom, true, "alerte.wav") ==
        InterfaceSoundFile{.place = InterfaceSoundPlace::kInstallation,
                           .path = "savedata/alerte.wav"});
  // Hors plage : le son du système, jamais le fichier personnel.
  CHECK(MvpAlertToneFile(kCustom + 1, true, "alerte.wav").place == InterfaceSoundPlace::kNowhere);
  CHECK(MvpAlertToneFile(-1, false, "alerte.wav") == MessengerSoundFile());
  // Les deux sons du jeu : l'arrivée d'un message sur le disque, le bouton
  // dans les archives.
  CHECK(MessengerSoundFile().place == InterfaceSoundPlace::kInstallation);
  CHECK(ButtonSoundFile().place == InterfaceSoundPlace::kSoundArchive);
}

TEST_CASE("Son de l'alerte MVP : le fichier personnel reste dans savedata", "[app][mvp]") {
  CHECK(SaveFolderSoundFile("sons\\alerte.wav").path == "savedata/sons/alerte.wav");
  CHECK(SaveFolderSoundFile("").place == InterfaceSoundPlace::kNowhere);
  CHECK(SaveFolderSoundFile("..\\BGM\\Msg_NewMsg.wav").place == InterfaceSoundPlace::kNowhere);
  CHECK(SaveFolderSoundFile("sons/../../x.wav").place == InterfaceSoundPlace::kNowhere);
  CHECK(SaveFolderSoundFile("C:\\Windows\\Media\\chord.wav").place ==
        InterfaceSoundPlace::kNowhere);
  CHECK(SaveFolderSoundFile("/etc/alerte.wav").place == InterfaceSoundPlace::kNowhere);
  // Deux points dans un NOM ne remontent rien.
  CHECK(SaveFolderSoundFile("alerte..wav").place == InterfaceSoundPlace::kInstallation);
}

TEST_CASE("Son de l'alerte MVP : le fichier personnel joue, et efface l'échec retenu",
          "[app][mvp]") {
  auto audio = OfflineEngine();
  if (!audio) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  const TempDirectory install;
  install.WriteFile("savedata/alerte.wav", ShortBeep());
  ro::vfs::Vfs vfs = MountedOn(install);
  std::string reported = "un échec d'avant";
  const MvpAlertSoundOutcome outcome = PlayMvpAlertSound({.audio = *audio,
                                                          .vfs = vfs,
                                                          .client_root = install.path(),
                                                          .tone = kCustom,
                                                          .custom_file = "alerte.wav",
                                                          .reported = &reported});
  CHECK(outcome == MvpAlertSoundOutcome::kPlayed);
  // Un son qui joue efface l'échec retenu : le prochain se redira.
  CHECK(reported.empty());
}

TEST_CASE("Son de l'alerte MVP : un nom accentué se lit sur le disque", "[app][mvp]") {
  auto audio = OfflineEngine();
  if (!audio) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  const TempDirectory install;
  install.WriteFile(std::filesystem::path(u8"savedata/alérte.wav"), ShortBeep());
  ro::vfs::Vfs vfs = MountedOn(install);
  CHECK(PlayMvpAlertSound({.audio = *audio,
                           .vfs = vfs,
                           .client_root = install.path(),
                           .tone = kCustom,
                           .custom_file = "alérte.wav"}) == MvpAlertSoundOutcome::kPlayed);
}

TEST_CASE("Son de l'alerte MVP : un fichier personnel remplacé se relit", "[app][mvp]") {
  auto audio = OfflineEngine();
  if (!audio) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  const TempDirectory install;
  install.WriteFile("savedata/alerte.wav", ShortBeep());
  ro::vfs::Vfs vfs = MountedOn(install);
  std::string reported;
  const moonclient::MvpAlertSoundSources custom{.audio = *audio,
                                                .vfs = vfs,
                                                .client_root = install.path(),
                                                .tone = kCustom,
                                                .custom_file = "alerte.wav",
                                                .reported = &reported};
  REQUIRE(PlayMvpAlertSound(custom) == MvpAlertSoundOutcome::kPlayed);
  // Remplacé par un fichier qui ne se décode pas : le son retenu ne le cache
  // pas.
  install.WriteText("savedata/alerte.wav", "ceci n'est pas un son");
  CHECK(PlayMvpAlertSound(custom) == MvpAlertSoundOutcome::kSilentReported);
  // Rétabli : il sonne de nouveau.
  install.WriteFile("savedata/alerte.wav", ShortBeep());
  CHECK(PlayMvpAlertSound(custom) == MvpAlertSoundOutcome::kPlayed);
  // Supprimé : il manque, même déjà entendu.
  std::filesystem::remove(install.path() / "savedata/alerte.wav");
  CHECK(PlayMvpAlertSound(custom) == MvpAlertSoundOutcome::kSilentReported);
}

TEST_CASE("Son de l'alerte MVP : remplacer le fichier ne fait pas grossir les sons tenus",
          "[app][mvp]") {
  auto audio = OfflineEngine();
  if (!audio) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  const TempDirectory install;
  install.WriteFile("BGM/Msg_NewMsg.wav", ShortBeep());
  install.WriteFile("savedata/alerte.wav", ShortBeep());
  ro::vfs::Vfs vfs = MountedOn(install);
  const InterfaceSoundFile custom = SaveFolderSoundFile("alerte.wav");
  // Un autre son du disque, tenu en même temps : il ne s'oublie pas avec.
  REQUIRE(moonclient::PlayInterfaceSound(*audio, vfs, install.path(), MessengerSoundFile())
              .has_value());
  REQUIRE(moonclient::PlayInterfaceSound(*audio, vfs, install.path(), custom).has_value());
  const std::size_t held_sound_count = audio->HeldSoundNames().size();
  REQUIRE(held_sound_count == 2);

  // Chaque remplacement change la taille, donc le nom de la version retenue.
  constexpr std::size_t kReplacements = 3;
  for (std::size_t replacement = 1; replacement <= kReplacements; ++replacement) {
    const std::vector<std::int16_t> longer(static_cast<std::size_t>(kRateHz / 10) + replacement,
                                           std::int16_t{1000});
    install.WriteFile("savedata/alerte.wav", ro::test_support::BuildWav(longer, kRateHz));
    REQUIRE(moonclient::PlayInterfaceSound(*audio, vfs, install.path(), custom).has_value());
    CHECK(audio->HeldSoundNames().size() == held_sound_count);
  }
  // L'autre son est toujours le même : rejoué, il ne s'ajoute pas.
  REQUIRE(moonclient::PlayInterfaceSound(*audio, vfs, install.path(), MessengerSoundFile())
              .has_value());
  CHECK(audio->HeldSoundNames().size() == held_sound_count);
}

TEST_CASE("Son de l'alerte MVP : changer de fichier oublie le précédent", "[app][mvp]") {
  auto audio = OfflineEngine();
  if (!audio) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  const TempDirectory install;
  install.WriteFile("BGM/Msg_NewMsg.wav", ShortBeep());
  install.WriteFile("savedata/a.wav", ShortBeep());
  install.WriteFile("savedata/b.wav", ShortBeep());
  ro::vfs::Vfs vfs = MountedOn(install);
  std::string custom_held;
  const auto play = [&](int tone, std::string_view custom_file) {
    return PlayMvpAlertSound({.audio = *audio,
                              .vfs = vfs,
                              .client_root = install.path(),
                              .tone = tone,
                              .custom_file = custom_file,
                              .custom_held = &custom_held});
  };
  // L'arrivée d'un message, tenue aussi pour les chuchotements.
  REQUIRE(moonclient::PlayInterfaceSound(*audio, vfs, install.path(), MessengerSoundFile())
              .has_value());
  REQUIRE(play(kCustom, "a.wav") == MvpAlertSoundOutcome::kPlayed);
  CHECK(audio->HeldSoundNames().size() == 2);
  // Un autre fichier : l'ancien s'oublie.
  REQUIRE(play(kCustom, "b.wav") == MvpAlertSoundOutcome::kPlayed);
  CHECK(audio->HeldSoundNames().size() == 2);
  // Un son du jeu : le fichier personnel s'oublie, l'arrivée d'un message reste.
  REQUIRE(play(static_cast<int>(MvpAlertTone::kNewMessage), "b.wav") ==
          MvpAlertSoundOutcome::kPlayed);
  CHECK(audio->HeldSoundNames().size() == 1);
  CHECK(custom_held.empty());
}

TEST_CASE("Son de l'alerte MVP : un fichier illisible se dit une fois", "[app][mvp]") {
  auto audio = OfflineEngine();
  if (!audio) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  const TempDirectory install;
  install.WriteText("savedata/abime.wav", "ceci n'est pas un son");
  ro::vfs::Vfs vfs = MountedOn(install);
  std::string reported;
  const moonclient::MvpAlertSoundSources broken{.audio = *audio,
                                                .vfs = vfs,
                                                .client_root = install.path(),
                                                .tone = kCustom,
                                                .custom_file = "abime.wav",
                                                .reported = &reported};
  CHECK(PlayMvpAlertSound(broken) == MvpAlertSoundOutcome::kSilentReported);
  CHECK_FALSE(reported.empty());
  CHECK(PlayMvpAlertSound(broken) == MvpAlertSoundOutcome::kSilentAlreadyReported);

  // UN AUTRE ÉCHEC SE DIT : un fichier absent n'est pas le fichier abîmé.
  const moonclient::MvpAlertSoundSources missing{.audio = *audio,
                                                 .vfs = vfs,
                                                 .client_root = install.path(),
                                                 .tone = kCustom,
                                                 .custom_file = "absent.wav",
                                                 .reported = &reported};
  CHECK(PlayMvpAlertSound(missing) == MvpAlertSoundOutcome::kSilentReported);
  CHECK(PlayMvpAlertSound(missing) == MvpAlertSoundOutcome::kSilentAlreadyReported);

  // UN NOM REFUSÉ aussi, sans rien lire.
  const moonclient::MvpAlertSoundSources refused{.audio = *audio,
                                                 .vfs = vfs,
                                                 .client_root = install.path(),
                                                 .tone = kCustom,
                                                 .custom_file = "../abime.wav",
                                                 .reported = &reported};
  CHECK(PlayMvpAlertSound(refused) == MvpAlertSoundOutcome::kSilentReported);
  CHECK(PlayMvpAlertSound(refused) == MvpAlertSoundOutcome::kSilentAlreadyReported);
}

TEST_CASE("Son de l'alerte MVP : la raison de chaque échec, que la page montre", "[app][mvp]") {
  auto audio = OfflineEngine();
  if (!audio) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  const TempDirectory install;
  install.WriteFile("savedata/alerte.wav", ShortBeep());
  install.WriteText("savedata/abime.wav", "ceci n'est pas un son");
  ro::vfs::Vfs vfs = MountedOn(install);
  std::string reported;
  MvpAlertTrouble trouble = MvpAlertTrouble::kNone;
  const auto play = [&](int tone, std::string_view custom_file) {
    return PlayMvpAlertSound({.audio = *audio,
                              .vfs = vfs,
                              .client_root = install.path(),
                              .tone = tone,
                              .custom_file = custom_file,
                              .reported = &reported,
                              .trouble = &trouble});
  };
  CHECK(play(kCustom, "absent.wav") == MvpAlertSoundOutcome::kSilentReported);
  CHECK(trouble == MvpAlertTrouble::kFileMissing);
  // Un son qui joue l'efface.
  CHECK(play(kCustom, "alerte.wav") == MvpAlertSoundOutcome::kPlayed);
  CHECK(trouble == MvpAlertTrouble::kNone);
  CHECK(play(kCustom, "abime.wav") == MvpAlertSoundOutcome::kSilentReported);
  CHECK(trouble == MvpAlertTrouble::kFileUnreadable);
  // ⚠ LA MÊME RAISON, DÉJÀ JOURNALISÉE, SE POSE ENCORE : la page l'a peut-être
  // effacée entre-temps.
  trouble = MvpAlertTrouble::kNone;
  CHECK(play(kCustom, "abime.wav") == MvpAlertSoundOutcome::kSilentAlreadyReported);
  CHECK(trouble == MvpAlertTrouble::kFileUnreadable);
  CHECK(play(kCustom, "..\\alerte.wav") == MvpAlertSoundOutcome::kSilentReported);
  CHECK(trouble == MvpAlertTrouble::kNameRefused);
  CHECK(play(kCustom, "") == MvpAlertSoundOutcome::kSilentReported);
  CHECK(trouble == MvpAlertTrouble::kNameEmpty);
  // Un son du jeu qui manque à l'installation.
  CHECK(play(static_cast<int>(MvpAlertTone::kNewMessage), {}) ==
        MvpAlertSoundOutcome::kSilentReported);
  CHECK(trouble == MvpAlertTrouble::kFileMissing);
  // Bruitages coupés : c'est cela qui empêche d'entendre, pas le fichier.
  audio->SetSoundAudible(false);
  CHECK(play(kCustom, "absent.wav") == MvpAlertSoundOutcome::kMuted);
  CHECK(trouble == MvpAlertTrouble::kMuted);
  audio->SetSoundAudible(true);
  CHECK(play(kCustom, "alerte.wav") == MvpAlertSoundOutcome::kPlayed);
  CHECK(trouble == MvpAlertTrouble::kNone);
}

TEST_CASE("Son de l'alerte MVP : « Nouveau message » se lit sur le disque", "[app][mvp]") {
  auto audio = OfflineEngine();
  if (!audio) {
    SKIP("pas de mélangeur sans périphérique sur cette machine");
  }
  const TempDirectory install;
  install.WriteFile("BGM/Msg_NewMsg.wav", ShortBeep());
  ro::vfs::Vfs vfs = MountedOn(install);
  CHECK(PlayMvpAlertSound({.audio = *audio,
                           .vfs = vfs,
                           .client_root = install.path(),
                           .tone = static_cast<int>(MvpAlertTone::kNewMessage)}) ==
        MvpAlertSoundOutcome::kPlayed);
  // BRUITS COUPÉS : rien ne se lit, et rien n'échoue — ni ne se retient.
  audio->SetSoundAudible(false);
  std::string reported;
  CHECK(PlayMvpAlertSound({.audio = *audio,
                           .vfs = vfs,
                           .client_root = install.path(),
                           .tone = kCustom,
                           .custom_file = "absent.wav",
                           .reported = &reported}) == MvpAlertSoundOutcome::kMuted);
  CHECK(reported.empty());
}

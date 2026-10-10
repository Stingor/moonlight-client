// Le son de l'alerte du carnet de chasse MVP. Voir l'en-tête.

#include "game/mvp_alert_sound.h"

#include <expected>
#include <string>
#include <utility>

#include "ro/core/log.h"

#include "options/mvp_tracker_options.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace moonclient {
namespace {

#if defined(_WIN32)
constexpr bool kSystemBeepAvailable = true;
#else
constexpr bool kSystemBeepAvailable = false;
#endif

void SystemBeep() {
#if defined(_WIN32)
  // Le refus du système — aucun son configuré — ne se rattrape pas : l'alerte
  // reste écrite à l'écran.
  static_cast<void>(MessageBeep(MB_ICONASTERISK));
#endif
}

// LE FICHIER PERSONNEL D'AVANT S'OUBLIE dès que l'alerte a joué autre chose.
// Seul le fichier personnel se retient : les sons du jeu se partagent avec
// d'autres sites — l'arrivée d'un message —, et les oublier les ferait relire.
void ForgetReplacedCustom(const MvpAlertSoundSources& from, const std::string& played_name) {
  if (from.custom_held == nullptr) {
    return;
  }
  if (!from.custom_held->empty() && *from.custom_held != played_name) {
    from.audio.ForgetSound(*from.custom_held);
  }
  *from.custom_held =
      MvpAlertToneOf(from.tone) == MvpAlertTone::kCustom ? played_name : std::string();
}

void NoteTrouble(const MvpAlertSoundSources& from, MvpAlertTrouble trouble) {
  if (from.trouble != nullptr) {
    *from.trouble = trouble;
  }
}

// LA RAISON D'UN ÉCHEC DE LECTURE : un fichier qui manque, ou qui ne se décode
// pas. Le nom refusé ne passe pas par ici.
[[nodiscard]] MvpAlertTrouble TroubleOfError(const ro::core::Error& error) {
  return error.code == ro::core::ErrorCode::kNotFound ? MvpAlertTrouble::kFileMissing
                                                      : MvpAlertTrouble::kFileUnreadable;
}

}  // namespace

InterfaceSoundFile MvpAlertToneFile(int tone, bool system_beep_available,
                                    std::string_view custom_file) {
  switch (MvpAlertToneOf(tone)) {
    case MvpAlertTone::kSystem:
      return system_beep_available ? InterfaceSoundFile{} : MessengerSoundFile();
    case MvpAlertTone::kNewMessage:
      return MessengerSoundFile();
    case MvpAlertTone::kButton:
      return ButtonSoundFile();
    case MvpAlertTone::kCustom:
      return SaveFolderSoundFile(custom_file);
  }
  return {};
}

MvpAlertSoundOutcome PlayMvpAlertSound(const MvpAlertSoundSources& from) {
  const InterfaceSoundFile file =
      MvpAlertToneFile(from.tone, kSystemBeepAvailable, from.custom_file);
  const bool system_beep = file.place == InterfaceSoundPlace::kNowhere &&
                           MvpAlertToneOf(from.tone) != MvpAlertTone::kCustom;
  if (system_beep) {
    // Le bip ne passe pas par le moteur : le fichier personnel joué avant lui
    // s'oublie quand même.
    ForgetReplacedCustom(from, std::string());
    SystemBeep();
    NoteTrouble(from, MvpAlertTrouble::kNone);
    return MvpAlertSoundOutcome::kSystemBeep;
  }
  if (!from.audio.sound_audible()) {
    NoteTrouble(from, MvpAlertTrouble::kMuted);
    return MvpAlertSoundOutcome::kMuted;
  }
  ro::core::Result<void> played;
  MvpAlertTrouble trouble = MvpAlertTrouble::kNone;
  if (file.place != InterfaceSoundPlace::kNowhere) {
    std::string held_name;
    played = PlayInterfaceSound(from.audio, from.vfs, from.client_root, file, &held_name);
    if (played) {
      ForgetReplacedCustom(from, held_name);
    } else {
      trouble = TroubleOfError(played.error());
    }
  } else {
    played = std::unexpected(
        ro::core::Error{ro::core::ErrorCode::kInvalidArgument,
                        "fichier personnel refusé : un nom relatif à savedata, sans « .. » — « " +
                            std::string(from.custom_file) + " »"});
    trouble =
        from.custom_file.empty() ? MvpAlertTrouble::kNameEmpty : MvpAlertTrouble::kNameRefused;
  }
  NoteTrouble(from, trouble);
  if (played) {
    if (from.reported != nullptr) {
      from.reported->clear();
    }
    return MvpAlertSoundOutcome::kPlayed;
  }
  std::string said = ro::core::Describe(played.error());
  if (from.reported != nullptr && *from.reported == said) {
    return MvpAlertSoundOutcome::kSilentAlreadyReported;
  }
  ro::core::LogWarning("carnet MVP : le son de l'alerte ne sonne pas — {}", said);
  if (from.reported != nullptr) {
    *from.reported = std::move(said);
  }
  return MvpAlertSoundOutcome::kSilentReported;
}

}  // namespace moonclient

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"

namespace moonclient {

// ── LA VOIX EN JEU, DANS LA PAGE « SON » ────────────────────────────────────
//
// L'interrupteur, la touche « Parler », le canal où l'on parle et où l'on
// écoute, le volume des voix et celui du micro, les deux appareils, l'icône
// au-dessus de qui parle, et comment le micro part : à la touche ou à la voix,
// avec un délai au relâchement et un seuil qu'on règle devant le niveau du
// micro. Le côté d'où vient une voix suit « Son spatialisé ».
// Voir l'ADR 0135.
inline constexpr int kVoiceRowBase = 9400;

enum class VoiceRow : int {
  kEnabled = 0,
  kChannel,
  kOutputVolume,
  kInputVolume,
  kMicrophone,
  kSpeakers,
  kTalkKey,
  kSpeakingIcon,
  kTalkMode,
  kRelease,
  kActivation,
  kInputLevel,
  kCount,
};

// COMMENT LE MICRO PART. ⚠ LE RANG EST GARDÉ DANS LE FICHIER.
enum class VoiceTalkMode : std::uint8_t {
  kPushToTalk,  // en tenant la touche « Parler »
  kVoice,       // dès que la voix dépasse le seuil ; la touche marche aussi
  kCount,
};

// LE SEUIL ET LE NIVEAU DU MICRO se montrent en pour cent de ce niveau, une
// moyenne quadratique sur une échelle où 1 est le plus fort possible : une voix
// ordinaire tient dans la première moitié.
inline constexpr float kVoiceLevelAtFullScale = 0.2F;
inline constexpr float kVoiceDefaultActivationPercent = 15.0F;
inline constexpr int kVoiceDefaultReleaseMs = 300;
inline constexpr int kVoiceMostReleaseMs = 1000;

// L'ICÔNE AU-DESSUS DE QUI PARLE, au choix du joueur : elle ne change que ce
// qu'il voit. ⚠ LE RANG EST GARDÉ DANS LE FICHIER : on ajoute à la fin.
enum class SpeakingIcon : std::uint8_t {
  kMicrophone,
  kSpeakingHead,
  kSpeechBalloon,
  kSpeaker,
  kMusicalNote,
  kNone,
  kCount,
};

// L'image d'une icône, sous le dossier de nos icônes, ou vide pour « aucune ».
[[nodiscard]] std::string_view SpeakingIconPath(SpeakingIcon icon);

// CE QUE LA PAGE MONTRE ET QUE LA BOUCLE SEULE SAIT : la touche « Parler », et
// le niveau du micro à l'instant.
struct VoiceLiveView {
  std::string talk_key;    // vide quand aucune touche n'est posée
  bool capturing = false;  // la capture de la touche attend son geste
  // En pour cent de `kVoiceLevelAtFullScale` ; vide quand la voix est fermée.
  std::optional<float> input_percent;
};

[[nodiscard]] constexpr int VoiceRowId(VoiceRow row) {
  return kVoiceRowBase + static_cast<int>(row);
}

// LES CANAUX QU'ON CHOISIT, au rang du fil : le salon s'impose de lui-même.
inline constexpr std::size_t kVoiceChosenChannelCount = 3;
// Le micro peut monter au double : un micro discret se rattrape sans réglage
// du système.
inline constexpr float kVoiceMostInputGain = 2.0F;

struct VoiceSettings {
  // ALLUMÉE par défaut : rien ne part sans la touche « Parler », et entendre
  // les autres est ce qu'on attend d'un serveur qui ouvre la voix.
  bool enabled = true;
  int channel = 0;  // 0 proximité, 1 groupe, 2 guilde
  float output_volume = 1.0F;
  float input_gain = 1.0F;
  // LE NOM DE L'APPAREIL, vide pour celui du système.
  std::string microphone;
  std::string speakers;
  int speaking_icon = 0;  // une `SpeakingIcon`
  int talk_mode = 0;      // un `VoiceTalkMode`
  int release_ms = kVoiceDefaultReleaseMs;
  float activation_percent = kVoiceDefaultActivationPercent;
};

// UN CHOIX D'APPAREIL tel que la ligne le montre : « celui du système » en
// tête, puis ceux que le système connaît, puis celui qu'on avait gardé s'il
// est débranché, pour que la ligne dise vrai.
struct VoiceDeviceChoice {
  std::vector<std::string> names;   // ce que la ligne choisie garde
  std::vector<std::string> labels;  // ce qu'elle montre
  std::vector<const char*> shown;   // les mêmes, pour la ligne
  int chosen = 0;
};

class VoiceOptions {
 public:
  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);
  // Les noms des appareils que le système connaît, demandés à chaque
  // `ComposeInto` : un casque se branche pendant que la page est ouverte.
  using DeviceLister = std::function<std::vector<std::string>()>;
  void ListDevicesWith(DeviceLister microphones, DeviceLister speakers);
  // LA TOUCHE « PARLER » ET LE NIVEAU DU MICRO, que la boucle seule sait : elle
  // les redit à chaque image, et reprend la demande de capture que la ligne
  // de la touche arme.
  void ShowLive(VoiceLiveView live) { live_ = std::move(live); }
  [[nodiscard]] bool TakeTalkKeyRequest() noexcept { return std::exchange(talk_key_asked_, false); }
  // Ajoute ses lignes à celles de la page Son, à chaque image où elle est
  // ouverte.
  void ComposeInto(std::vector<ro::ui::OptionRowView>& rows);
  // Rend vrai quand la ligne était l'une des siennes.
  [[nodiscard]] bool Change(const ro::ui::OptionsWindowAction& asked);
  // Bornées : le fichier peut porter n'importe quoi.
  [[nodiscard]] VoiceSettings settings() const noexcept;

 private:
  VoiceSettings settings_;
  std::array<const char*, kVoiceChosenChannelCount> channel_names_{};
  DeviceLister list_microphones_;
  DeviceLister list_speakers_;
  VoiceDeviceChoice microphone_choice_;
  VoiceDeviceChoice speakers_choice_;
  std::array<const char*, static_cast<std::size_t>(SpeakingIcon::kCount)> icon_names_{};
  std::array<const char*, static_cast<std::size_t>(VoiceTalkMode::kCount)> mode_names_{};
  VoiceLiveView live_;
  std::string talk_key_text_;
  std::string level_text_;
  bool talk_key_asked_ = false;
};

}  // namespace moonclient

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
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
// écoute, le volume des voix et celui du micro, les deux appareils et l'icône
// au-dessus de qui parle. Le côté d'où vient une voix suit « Son spatialisé ».
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
  kCount,
};

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

// La touche « Parler » telle que la ligne la montre.
struct VoiceTalkKeyView {
  std::string label;       // vide quand aucune touche n'est posée
  bool capturing = false;  // la capture attend son geste
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
  // LA TOUCHE « PARLER », que la boucle seule sait nommer et capturer : elle la
  // redit à chaque image, et reprend la demande de capture que la ligne arme.
  void ShowTalkKey(VoiceTalkKeyView talk_key) { talk_key_ = std::move(talk_key); }
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
  VoiceTalkKeyView talk_key_;
  std::string talk_key_text_;
  bool talk_key_asked_ = false;
};

}  // namespace moonclient

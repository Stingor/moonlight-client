#pragma once

#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"

namespace moonclient {

// ── LA VOIX EN JEU, DANS LA PAGE « SON » ────────────────────────────────────
//
// L'interrupteur, le canal où l'on parle et où l'on écoute, le volume des voix
// et celui du micro, et les deux appareils. Le côté d'où vient une voix suit « Son spatialisé ».
// Voir l'ADR 0135.
inline constexpr int kVoiceRowBase = 9400;

enum class VoiceRow : int {
  kEnabled = 0,
  kChannel,
  kOutputVolume,
  kInputVolume,
  kMicrophone,
  kSpeakers,
  kCount,
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
};

}  // namespace moonclient

#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include "ro/ui/options_window.h"

#include "game/settings_file.h"

namespace moonclient {

// ── LA VOIX EN JEU, DANS LA PAGE « SON » ────────────────────────────────────
//
// L'interrupteur, le canal où l'on parle et où l'on écoute, le volume des voix
// et celui du micro. Le côté d'où vient une voix suit « Son spatialisé ».
// Voir l'ADR 0135.
inline constexpr int kVoiceRowBase = 9400;

enum class VoiceRow : int {
  kEnabled = 0,
  kChannel,
  kOutputVolume,
  kInputVolume,
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
};

class VoiceOptions {
 public:
  // ⚠ AVANT LA LECTURE DU FICHIER : la valeur présente ici est le défaut.
  void Declare(SettingsFile& into);
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
};

}  // namespace moonclient

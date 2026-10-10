#pragma once

// CE QUE LE CLIENT RETIENT DU PANNEAU DE SUIVI, commun aux quêtes et aux
// succès : ses réglages, et la place de chacun des deux panneaux quand on les
// a déplacés. Pour toute l'installation, dans `savedata\suivi.yml`.

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include "ro/ui/tracker_panel.h"

namespace moonclient {

inline constexpr std::string_view kTrackerMemoryFileName = "suivi.yml";

// Le panneau commun, puis celui des succès séparés.
inline constexpr std::size_t kTrackerPanelCount = 2;

struct TrackerMemory {
  // Le joueur veut le suivi à l'écran.
  bool shown = true;
  ro::ui::TrackerLook look;
  // Absente tant qu'on ne l'a pas déplacé.
  std::array<std::optional<ro::ui::TrackerPlace>, kTrackerPanelCount> places;
};

[[nodiscard]] std::filesystem::path TrackerMemoryPath(const std::filesystem::path& client_root);

// Un fichier absent ou illisible laisse `into` tel quel : les défauts valent.
void LoadTrackerMemory(const std::filesystem::path& client_root, TrackerMemory& into);
bool SaveTrackerMemory(const std::filesystem::path& client_root, const TrackerMemory& from);

// Lecture et écriture du TEXTE, pour les essais.
void ParseTrackerMemory(std::string_view text, TrackerMemory& into);
[[nodiscard]] std::string TrackerMemoryText(const TrackerMemory& from);

}  // namespace moonclient

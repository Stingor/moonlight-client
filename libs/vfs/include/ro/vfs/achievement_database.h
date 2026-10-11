#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LES SUCCÈS QUE LE SERVEUR CONNAÎT, et ce qu'il en détient.
//
// `data\yaml_files\achievements.yml`, exporté de l'achievement_db du serveur
// par `scripts/yaml_tables/achievements.py`. Le serveur ne pousse que des
// numéros, des compteurs et des drapeaux ; c'est cette table qui donne le nom,
// les points, le compte qui achève chaque objectif, les récompenses et les
// succès requis :
//
//     version: 1
//     achievements: {
//       128001: {name: "Specialist", score: 20, targets: [[0, 10]],
//                reward: {item: 22876, title: 1001, script: true},
//                dependents: [128000]},
//       …
//     }
//
// 🔴 UN SUCCÈS ABSENT D'ICI NE S'OBTIENT PAS : le client ne le montre pas, ni
// ne propose le titre qu'il donnerait.
inline constexpr std::string_view kAchievementDatabasePath = "data\\yaml_files\\achievements.yml";

// Le plus d'objectifs qu'un succès porte : le serveur tient dix compteurs, et la
// fenêtre dix lignes.
inline constexpr std::size_t kAchievementObjectiveCount = 10;

// ⚠ LE PLUS GRAND NUMÉRO ACCEPTÉ, de succès comme de titre ou d'item : tous
// voyagent en trente-deux bits signés, et le client les range ainsi.
inline constexpr std::uint32_t kMostAchievementNumber =
    static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());

struct ServerAchievement {
  std::uint32_t achievement_id = 0;
  std::string name;
  std::int32_t score = 0;
  // Le compte qui achève chaque objectif, par son numéro ; rien quand le
  // serveur ne suit pas cet objectif.
  std::array<std::optional<std::uint32_t>, kAchievementObjectiveCount> targets{};
  // L'item et le titre donnés, 0 si aucun.
  std::int32_t reward_item = 0;
  std::int32_t reward_title = 0;
  // Le succès exécute un script : l'aide que la fenêtre montre en récompense.
  bool reward_script = false;
  // Les succès à achever d'abord, dans l'ordre du serveur.
  std::vector<std::uint32_t> dependents;
};

class AchievementDatabase {
 public:
  [[nodiscard]] static core::Result<AchievementDatabase> Load(Vfs& vfs);

  // La MÊME lecture, à partir du texte : tout ou rien, un texte mal formé est
  // refusé entier.
  [[nodiscard]] static core::Result<AchievementDatabase> Parse(std::string_view yaml_text);

  // Le succès de ce numéro, ou nullptr.
  [[nodiscard]] const ServerAchievement* Find(std::uint32_t achievement_id) const;

  // Tous les succès, par numéro croissant.
  [[nodiscard]] std::span<const ServerAchievement> entries() const noexcept { return entries_; }

 private:
  std::vector<ServerAchievement> entries_;
};

}  // namespace ro::vfs

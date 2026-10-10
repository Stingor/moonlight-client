#pragma once

// LE PANNEAU DE SUIVI, COMMUN AUX QUÊTES ET AUX SUCCÈS : un panneau, ou deux
// quand le joueur sépare les succès. Chaque étage compose ses entrées ; celui-ci
// les range en sections, pose réglages et places, et renvoie à chaque étage ce
// que le joueur a demandé sur ses entrées.

#include <filesystem>
#include <utility>

#include "game/achievement_stage.h"
#include "game/quest_stage.h"
#include "quest/tracker_memory.h"

namespace moonclient {

// Le décalage vers le bas de la place d'origine d'un panneau, en pixels d'art :
// le panneau des succès séparés naît sous celui des quêtes.
[[nodiscard]] float TrackerPanelFirstTop(std::size_t panel);

class TrackerStage {
 public:
  // Où se trouve l'installation : la mémoire vit sous son `savedata\`.
  void SeeSaveRoot(std::filesystem::path client_root) { client_root_ = std::move(client_root); }

  // Une partie commence : les places retenues se reprennent.
  void StartSession();

  // L'IMAGE : ce que les panneaux et les fenêtres ont rendu, puis les sections
  // des prochains. ⚠ APRÈS les étages des quêtes et des succès, qui composent
  // leurs entrées.
  void Advance(TrackerWindows& tracker, QuestStage& quests, QuestWindows& quest_windows,
               AchievementWindows& achievement_windows);

 private:
  void Save();
  void FollowPanel(std::size_t panel, TrackerWindows& tracker, QuestStage& quests,
                   QuestWindows& quest_windows, AchievementWindows& achievement_windows);
  void Settle(TrackerWindows& tracker, const ro::ui::TrackerSettingsAnswer& asked);

  std::filesystem::path client_root_;
  TrackerMemory memory_;
  bool places_pending_ = true;
  // Les en-têtes de section et leurs comptes.
  std::string quest_label_;
  std::string achievement_label_;
  std::array<std::array<ro::ui::TrackerSection, ro::ui::kTrackerKindCount>, kTrackerPanelCount>
      sections_{};
};

}  // namespace moonclient

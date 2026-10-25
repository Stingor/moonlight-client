#pragma once

#include <filesystem>
#include <vector>

#include "ro/sim/minigame_trophies.h"

#include "game/minigame_stage.h"
#include "survivors/survivors_stage.h"

namespace moonclient {

// LES TROPHÉES D'UNE PARTIE DE SURVIVORS, comptés une fois par partie : à son
// entrée dans le bilan, quelle que soit son issue, ou quand elle se fige —
// liaison perdue, partie qui se ferme (Q3). Les trophées s'inscrivent dans
// l'étage des mini-jeux, qui les écrit aussitôt dans `minigames.yml`, et ceux
// que la partie a gagnés se gardent pour son écran de fin (Q13).
//
// ⚠ SANS ÉTAGE DES MINI-JEUX (`minigames` nul), RIEN NE COMPTE, et la partie
// reste à compter : la prochaine occasion la prendra.
class SurvivorsTrophyCount {
 public:
  // Une partie neuve commence : elle comptera à son tour.
  void Restart() noexcept {
    counted_ = false;
    won_.clear();
  }
  // L'IMAGE QUI VIENT DE S'AVANCER : une partie entrée dans le bilan compte.
  void FollowEntered(const SurvivorsStage& stage, MinigameStage* minigames,
                     const std::filesystem::path& client_root);
  // FIGE L'ÉTAGE ET ÉCRIT LA PARTIE INTERROMPUE (`FreezeAndSave`), puis la
  // compte. Vrai quand une partie s'est close là.
  bool Freeze(SurvivorsStage& stage, MinigameStage* minigames,
              const std::filesystem::path& client_root);
  // La partie en cours a compté.
  [[nodiscard]] bool counted() const noexcept { return counted_; }
  // Les trophées que la partie en cours a gagnés, dans l'ordre du catalogue.
  [[nodiscard]] const std::vector<ro::sim::MinigameTrophy>& won() const noexcept { return won_; }

 private:
  void Count(const SurvivorsStage& stage, MinigameStage* minigames,
             const std::filesystem::path& client_root);

  bool counted_ = false;
  std::vector<ro::sim::MinigameTrophy> won_;
};

}  // namespace moonclient

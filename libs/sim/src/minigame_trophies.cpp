#include "ro/sim/minigame_trophies.h"

#include <algorithm>

#include "ro/sim/match3_game.h"
#include "ro/sim/peg_game.h"

namespace ro::sim {
namespace {

// Les seuils des trophées.
constexpr int kRojeweledLowScore = 5000;
constexpr int kRojeweledHighScore = 15000;
constexpr int kRojeweledLongCombo = 4;
constexpr int kRoggleCatches = 3;
constexpr int kRoggleSpareBalls = 5;
constexpr int kRoggleHighScore = 20000;

constexpr std::array<MinigameTrophyText, kMinigameTrophyCount> kTrophies = {{
    {MinigameTrophy::kRojeweledFinished, "rojeweled_finished", core::TrText("Jusqu'au bout"),
     core::TrText("Finir une partie de Rojeweled.")},
    {MinigameTrophy::kRojeweledScore5000, "rojeweled_score_5000",
     core::TrText("Éleveur de Porings"),
     core::TrText("Atteindre 5 000 points dans une partie de Rojeweled.")},
    {MinigameTrophy::kRojeweledScore15000, "rojeweled_score_15000",
     core::TrText("Dompteur de Porings"),
     core::TrText("Atteindre 15 000 points dans une partie de Rojeweled.")},
    {MinigameTrophy::kRojeweledCombo4, "rojeweled_combo_4", core::TrText("Réaction en chaîne"),
     core::TrText("Enchaîner une cascade de quatre dans Rojeweled.")},
    {MinigameTrophy::kRoggleWon, "roggle_won", core::TrText("Tir groupé"),
     core::TrText("Effacer tous les plots orange dans Roggle.")},
    {MinigameTrophy::kRoggleFlawless, "roggle_flawless", core::TrText("Sans bavure"),
     core::TrText("Gagner une partie de Roggle avec cinq billes en réserve.")},
    {MinigameTrophy::kRoggleThreeCatches, "roggle_three_catches", core::TrText("Main sûre"),
     core::TrText("Rattraper trois billes dans le seau en une partie de Roggle.")},
    {MinigameTrophy::kRoggleScore20000, "roggle_score_20000", core::TrText("Pluie de gemmes"),
     core::TrText("Atteindre 20 000 points dans une partie de Roggle.")},
}};

// Gagne `trophy` si `earned` ; l'ajoute alors à `fresh`.
void Earn(MinigameRecord& record, MinigameTrophy trophy, bool earned,
          std::vector<MinigameTrophy>& fresh) {
  bool& unlocked = record.unlocked[static_cast<std::size_t>(trophy)];
  if (earned && !unlocked) {
    unlocked = true;
    fresh.push_back(trophy);
  }
}

}  // namespace

std::span<const MinigameTrophyText, kMinigameTrophyCount> MinigameTrophies() { return kTrophies; }

std::vector<MinigameTrophy> RecordRojeweled(MinigameRecord& record, const Match3Game& game) {
  std::vector<MinigameTrophy> fresh;
  if (!game.started()) {
    return fresh;
  }
  const bool over = game.phase() == Match3Phase::kOver;
  if (over && game.score() > record.rojeweled_best) {
    record.rojeweled_best = game.score();
  }
  Earn(record, MinigameTrophy::kRojeweledFinished, over, fresh);
  Earn(record, MinigameTrophy::kRojeweledScore5000, game.score() >= kRojeweledLowScore, fresh);
  Earn(record, MinigameTrophy::kRojeweledScore15000, game.score() >= kRojeweledHighScore, fresh);
  Earn(record, MinigameTrophy::kRojeweledCombo4, game.best_combo() >= kRojeweledLongCombo, fresh);
  return fresh;
}

std::vector<MinigameTrophy> RecordRoggle(MinigameRecord& record, const PegGame& game) {
  std::vector<MinigameTrophy> fresh;
  if (!game.started()) {
    return fresh;
  }
  const bool won = game.phase() == PegPhase::kWon;
  const bool over = won || game.phase() == PegPhase::kLost;
  if (over && game.score() > record.roggle_best) {
    record.roggle_best = game.score();
  }
  Earn(record, MinigameTrophy::kRoggleWon, won, fresh);
  Earn(record, MinigameTrophy::kRoggleFlawless, won && game.balls_left() >= kRoggleSpareBalls,
       fresh);
  Earn(record, MinigameTrophy::kRoggleThreeCatches, game.catches() >= kRoggleCatches, fresh);
  Earn(record, MinigameTrophy::kRoggleScore20000, game.score() >= kRoggleHighScore, fresh);
  return fresh;
}

void MergeMinigameRecord(MinigameRecord& into, const MinigameRecord& from) {
  into.rojeweled_best = std::max(into.rojeweled_best, from.rojeweled_best);
  into.roggle_best = std::max(into.roggle_best, from.roggle_best);
  for (std::size_t trophy = 0; trophy < kMinigameTrophyCount; ++trophy) {
    into.unlocked[trophy] = into.unlocked[trophy] || from.unlocked[trophy];
  }
  for (const std::string& key : from.foreign_trophy_keys) {
    if (std::ranges::find(into.foreign_trophy_keys, key) == into.foreign_trophy_keys.end()) {
      into.foreign_trophy_keys.push_back(key);
    }
  }
}

}  // namespace ro::sim

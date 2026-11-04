#include "ro/sim/minigame_trophies.h"

#include <algorithm>

#include "ro/sim/match3_game.h"
#include "ro/sim/peg_game.h"

#include "minigame_trophy_earn.h"

namespace ro::sim {
namespace {

// Les seuils des trophées.
constexpr int kRojeweledLowScore = 5000;
constexpr int kRojeweledHighScore = 15000;
constexpr int kRojeweledLongCombo = 4;
constexpr int kRoggleCatches = 3;
constexpr int kRoggleSpareBalls = 5;
constexpr int kRoggleHighScore = 20000;
constexpr int kRogglePowers = 3;
constexpr int kRoggleBigShot = 10000;

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
    {MinigameTrophy::kSurvivorsFiveMinutes, "survivors_5min", core::TrText("Tenir bon"),
     core::TrText("Survivre 5:00 dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsVictory, "survivors_victory", core::TrText("Tombeur de Baphomet"),
     core::TrText("Vaincre Baphomet dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsFirstEvolution, "survivors_first_evolution",
     core::TrText("Métamorphose"),
     core::TrText("Faire évoluer un skill dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsKills1000, "survivors_kills_1000", core::TrText("Marée repoussée"),
     core::TrText("Vaincre 1 000 monstres dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsLevel40, "survivors_level_40", core::TrText("Mage aguerri"),
     core::TrText("Atteindre le niveau 40 dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsFirstCard, "survivors_first_card", core::TrText("Première carte"),
     core::TrText("Ramasser une carte tombée dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsPyromaniac, "survivors_pyromaniac", core::TrText("Pyromane"),
     core::TrText("Vaincre Baphomet avec des skills Fire seulement.")},
    {MinigameTrophy::kSurvivorsPureMind, "survivors_pure_mind", core::TrText("Esprit pur"),
     core::TrText("Vaincre Baphomet sans aucune carte.")},
    {MinigameTrophy::kSurvivorsStormGust, "survivors_storm_gust", core::TrText("Blizzard"),
     core::TrText("Obtenir Storm Gust dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsJupitelThunder, "survivors_jupitel_thunder",
     core::TrText("Foudre de Jupiter"),
     core::TrText("Obtenir Jupitel Thunder dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsLordOfVermilion, "survivors_lord_of_vermilion",
     core::TrText("Seigneur du tonnerre"),
     core::TrText("Obtenir Lord of Vermilion dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsHeavensDrive, "survivors_heavens_drive", core::TrText("Ciel en feu"),
     core::TrText("Obtenir Heaven's Drive dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsNapalmVulcan, "survivors_napalm_vulcan", core::TrText("Union"),
     core::TrText("Obtenir Napalm Vulcan, l'union de Soul Strike et de Napalm Beat.")},
    {MinigameTrophy::kSurvivorsGrandWizard, "survivors_grand_wizard", core::TrText("Grand sorcier"),
     core::TrText("Obtenir chacun des skills évolués, au fil des parties.")},
    {MinigameTrophy::kSurvivorsDeepFreeze, "survivors_deep_freeze", core::TrText("Hiver éternel"),
     core::TrText("Geler 1 000 monstres dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsBlindVictory, "survivors_blind_victory", core::TrText("À l'aveugle"),
     core::TrText("Vaincre Baphomet dans une partie lancée sans minimap.")},
    {MinigameTrophy::kSurvivorsFireBall, "survivors_fire_ball", core::TrText("Souffle ardent"),
     core::TrText("Porter Fire Ball au niveau maximum dans une partie.")},
    {MinigameTrophy::kSurvivorsWaterBall, "survivors_water_ball", core::TrText("Déluge"),
     core::TrText("Porter Water Ball au niveau maximum dans une partie.")},
    {MinigameTrophy::kSurvivorsIceWall, "survivors_ice_wall", core::TrText("Rempart de glace"),
     core::TrText("Porter Ice Wall au niveau maximum dans une partie.")},
    {MinigameTrophy::kSurvivorsCrimsonRock, "survivors_crimson_rock", core::TrText("Roche pourpre"),
     core::TrText("Obtenir Crimson Rock dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsJackFrost, "survivors_jack_frost", core::TrText("Givre"),
     core::TrText("Obtenir Jack Frost, l'union de Water Ball et de Cold Bolt.")},
    {MinigameTrophy::kRoggleThreePowers, "roggle_three_powers", core::TrText("Arsenal"),
     core::TrText("Déclencher trois pouvoirs dans une partie de Roggle.")},
    {MinigameTrophy::kRoggleDailyWon, "roggle_daily_won", core::TrText("Défi relevé"),
     core::TrText("Gagner le défi du jour de Roggle.")},
    {MinigameTrophy::kRoggleBigShot, "roggle_big_shot", core::TrText("Coup d'éclat"),
     core::TrText("Marquer 10 000 points en un seul tir de Roggle.")},
    {MinigameTrophy::kRoggleFreeBall, "roggle_free_ball", core::TrText("Bille offerte"),
     core::TrText("Gagner une bille par un tir de Roggle.")},
}};

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
  EarnMinigameTrophy(record, MinigameTrophy::kRojeweledFinished, over, fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kRojeweledScore5000,
                     game.score() >= kRojeweledLowScore, fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kRojeweledScore15000,
                     game.score() >= kRojeweledHighScore, fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kRojeweledCombo4,
                     game.best_combo() >= kRojeweledLongCombo, fresh);
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
  // LE MEILLEUR DU JOUR repart de zéro avec un jour neuf.
  if (over && game.daily()) {
    if (game.day() != record.roggle_daily_day) {
      record.roggle_daily_day = game.day();
      record.roggle_daily_best = 0;
    }
    record.roggle_daily_best = std::max(record.roggle_daily_best, game.score());
  }
  EarnMinigameTrophy(record, MinigameTrophy::kRoggleWon, won, fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kRoggleFlawless,
                     won && game.balls_left() >= kRoggleSpareBalls, fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kRoggleThreeCatches, game.catches() >= kRoggleCatches,
                     fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kRoggleScore20000, game.score() >= kRoggleHighScore,
                     fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kRoggleThreePowers,
                     game.powers_used() >= kRogglePowers, fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kRoggleDailyWon, won && game.daily(), fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kRoggleBigShot, game.best_shot() >= kRoggleBigShot,
                     fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kRoggleFreeBall, game.free_balls() > 0, fresh);
  return fresh;
}

void MergeMinigameRecord(MinigameRecord& into, const MinigameRecord& from) {
  into.rojeweled_best = std::max(into.rojeweled_best, from.rojeweled_best);
  into.roggle_best = std::max(into.roggle_best, from.roggle_best);
  // Le meilleur du jour le plus récent l'emporte ; à jour égal, le plus haut.
  if (from.roggle_daily_day > into.roggle_daily_day) {
    into.roggle_daily_day = from.roggle_daily_day;
    into.roggle_daily_best = from.roggle_daily_best;
  } else if (from.roggle_daily_day == into.roggle_daily_day) {
    into.roggle_daily_best = std::max(into.roggle_daily_best, from.roggle_daily_best);
  }
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

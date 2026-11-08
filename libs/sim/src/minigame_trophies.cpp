#include "ro/sim/minigame_trophies.h"

#include <algorithm>
#include <iterator>

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
constexpr std::size_t kRoggleManyItems = 10;
constexpr std::size_t kRoggleMostItems = 25;

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
     core::TrText("Atteindre le niveau 40 avec le Magicien dans Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsFirstCard, "survivors_first_card", core::TrText("Première carte"),
     core::TrText("Ramasser une carte tombée dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsPyromaniac, "survivors_pyromaniac", core::TrText("Pyromane"),
     core::TrText("Gagner une partie avec le Magicien et des skills Fire seulement.")},
    {MinigameTrophy::kSurvivorsPureMind, "survivors_pure_mind", core::TrText("Esprit pur"),
     core::TrText("Gagner une partie sans aucune carte.")},
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
     core::TrText("Obtenir chacun des skills évolués du Magicien, au fil des parties.")},
    {MinigameTrophy::kSurvivorsDeepFreeze, "survivors_deep_freeze", core::TrText("Hiver éternel"),
     core::TrText("Geler 1 000 monstres dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsBlindVictory, "survivors_blind_victory", core::TrText("À l'aveugle"),
     core::TrText("Gagner une partie lancée sans minimap.")},
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
    {MinigameTrophy::kRoggleFirstItem, "roggle_first_item", core::TrText("Première pièce"),
     core::TrText("Dévoiler un item dans Roggle.")},
    {MinigameTrophy::kRoggleTenItems, "roggle_ten_items", core::TrText("Collectionneur"),
     core::TrText("Dévoiler 10 items différents dans Roggle.")},
    {MinigameTrophy::kRoggleTwentyFiveItems, "roggle_twenty_five_items",
     core::TrText("Conservateur"), core::TrText("Dévoiler 25 items différents dans Roggle.")},
    {MinigameTrophy::kSurvivorsHeadCrush, "survivors_head_crush", core::TrText("Brise-crâne"),
     core::TrText("Obtenir Head Crush dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsSpiralPierce, "survivors_spiral_pierce",
     core::TrText("Lance en spirale"),
     core::TrText("Obtenir Spiral Pierce dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsJointBeat, "survivors_joint_beat", core::TrText("Brise-genoux"),
     core::TrText("Obtenir Joint Beat dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsAuraBlade, "survivors_aura_blade", core::TrText("Lame d'aura"),
     core::TrText("Obtenir Aura Blade dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsConcentration, "survivors_concentration",
     core::TrText("Esprit affûté"),
     core::TrText("Obtenir Concentration dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsFrenzy, "survivors_frenzy", core::TrText("Berserker"),
     core::TrText("Obtenir Frenzy dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsIgnitionBreak, "survivors_ignition_break",
     core::TrText("Terre embrasée"),
     core::TrText("Obtenir Ignition Break, l'union de Bowling Bash et de Brandish Spear.")},
    {MinigameTrophy::kSurvivorsGrandKnight, "survivors_grand_knight",
     core::TrText("Grand chevalier"),
     core::TrText("Obtenir chacun des skills évolués du Chevalier, au fil des parties.")},
    {MinigameTrophy::kSurvivorsSonicBlow, "survivors_sonic_blow", core::TrText("Lame sonique"),
     core::TrText("Obtenir Sonic Blow dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsSoulDestroyer, "survivors_soul_destroyer",
     core::TrText("Briseur d'âmes"),
     core::TrText("Obtenir Soul Destroyer dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsVenomSplasher, "survivors_venom_splasher",
     core::TrText("Éclats de venin"),
     core::TrText("Obtenir Venom Splasher dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsEnchantDeadlyPoison, "survivors_enchant_deadly_poison",
     core::TrText("Venin mortel"),
     core::TrText("Obtenir Enchant Deadly Poison dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsCloaking, "survivors_cloaking", core::TrText("Insaisissable"),
     core::TrText("Obtenir Cloaking dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsThrowVenomKnife, "survivors_throw_venom_knife",
     core::TrText("Couteau empoisonné"),
     core::TrText("Obtenir Throw Venom Knife dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsMeteorAssault, "survivors_meteor_assault",
     core::TrText("Assaut météore"),
     core::TrText("Obtenir Meteor Assault, l'union d'Envenom et de Sprinkle Sand.")},
    {MinigameTrophy::kSurvivorsGrandThief, "survivors_grand_thief", core::TrText("Grand voleur"),
     core::TrText("Obtenir chacun des skills évolués du Voleur, au fil des parties.")},
    {MinigameTrophy::kSurvivorsBeastStrafing, "survivors_beast_strafing",
     core::TrText("Tir de chasse"),
     core::TrText("Obtenir Beast Strafing dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsSharpShooting, "survivors_sharp_shooting",
     core::TrText("Tireur d'élite"),
     core::TrText("Obtenir Sharp Shooting dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsFalconAssault, "survivors_falcon_assault",
     core::TrText("Assaut du faucon"),
     core::TrText("Obtenir Falcon Assault dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsTrueSight, "survivors_true_sight", core::TrText("Œil de lynx"),
     core::TrText("Obtenir True Sight dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsClaymoreTrap, "survivors_claymore_trap",
     core::TrText("Piège Claymore"),
     core::TrText("Obtenir Claymore Trap dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsBlastMine, "survivors_blast_mine", core::TrText("Mine de vent"),
     core::TrText("Obtenir Blast Mine dans une partie de Survivants de Midgard.")},
    {MinigameTrophy::kSurvivorsPhantasmicArrow, "survivors_phantasmic_arrow",
     core::TrText("Flèche fantôme"),
     core::TrText("Obtenir Phantasmic Arrow, l'union de Charge Arrow et de Skid Trap.")},
    {MinigameTrophy::kSurvivorsGrandArcher, "survivors_grand_archer", core::TrText("Grand archer"),
     core::TrText("Obtenir chacun des skills évolués de l'Archer, au fil des parties.")},
}};

// Range un objet dans le carnet, croissant et sans doublon.
void Reveal(std::vector<std::uint32_t>& revealed, std::uint32_t item_id) {
  const auto at = std::ranges::lower_bound(revealed, item_id);
  if (at == revealed.end() || *at != item_id) {
    revealed.insert(at, item_id);
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
  if (won && game.layout() == PegLayout::kItem && game.item_id() != 0) {
    Reveal(record.roggle_revealed, game.item_id());
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
  const std::size_t revealed = record.roggle_revealed.size();
  EarnMinigameTrophy(record, MinigameTrophy::kRoggleFirstItem, revealed > 0, fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kRoggleTenItems, revealed >= kRoggleManyItems, fresh);
  EarnMinigameTrophy(record, MinigameTrophy::kRoggleTwentyFiveItems, revealed >= kRoggleMostItems,
                     fresh);
  return fresh;
}

std::vector<std::uint32_t> RoggleItemChoices(std::span<const std::uint32_t> items,
                                             const MinigameRecord& record) {
  const std::vector<std::uint32_t> all(items.begin(), items.end());
  if (!record.roggle_skip_revealed) {
    return all;
  }
  std::vector<std::uint32_t> unrevealed;
  std::ranges::copy_if(all, std::back_inserter(unrevealed), [&record](std::uint32_t item_id) {
    return !std::ranges::binary_search(record.roggle_revealed, item_id);
  });
  return unrevealed.empty() ? all : unrevealed;
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
  for (const std::uint32_t item_id : from.roggle_revealed) {
    Reveal(into.roggle_revealed, item_id);
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

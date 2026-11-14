#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "support/survivors_fixture.h"

namespace ro::test_support {

// LA PARTIE EN OR : les règles livrées SAUF leurs plantes (celles des rulesets
// antérieurs, ci-dessous), une arène à piliers, un départ fixé et le joueur
// scripté, joués cinq minutes. C'est un or sur règles d'essai : la règle des
// plantes livrée s'éprouve sur piliers dans survivors_plants_test.cpp. Elle ne dépend ni de Catch2
// ni d'aucune I/O, pour qu'un exécutable bâti par un autre compilateur la rejoue à l'identique et
// compare son bilan au bilan épinglé ici.
//
// La graine est choisie pour la COUVERTURE : la partie tient ses cinq
// minutes, voit l'essaim de 1:30, l'anneau de 2:30 et l'essaim de 4:30, tue
// deux élites et ouvre un coffre, voit naître Mastering, monte Thunderstorm et
// Frost Diver au niveau 2 et acquiert un mur, Fire Wall, et une barrière, Ice
// Wall. Un événement compte quand il a fait naître au moins un monstre. Une
// partie tombée après l'essaim de 4:30 compterait aussi.

inline constexpr std::int32_t kGoldenArenaCells = 160;
inline constexpr std::uint16_t kGoldenArenaId = 1;
inline constexpr std::uint64_t kGoldenSeed = 20261706;
inline constexpr std::uint16_t kGoldenClassId = 1;        // mage
inline constexpr std::uint16_t kGoldenFirstSkillId = 19;  // Fire Bolt
// 15 000 pas de 20 ms : les cinq premières minutes de la partie.
inline constexpr std::uint32_t kGoldenSteps = 15000;

inline constexpr std::array<sim::ArenaRule, 1> kGoldenArenas{{
    {.arena_id = kGoldenArenaId,
     .key = "pillar_arena",
     .map_name = "",
     .start = std::nullopt,
     .ranked = true},
}};

// LES PLANTES DES RULESETS ANTÉRIEURS : posées au départ, une au plus par
// bloc de 12 cases, une chance par 150 cases praticables du bloc ; aucune ne
// naît au fil des tués ni ne se perd. Leurs espèces et leurs objets gardent
// leurs poids et leurs gains d'alors.
inline constexpr std::int32_t kLegacyPlantBlockCells = 12;
inline constexpr std::uint32_t kLegacyPlantCellsPerPlant = 150;
inline constexpr std::uint16_t kLegacyChestItemId = 603;
inline constexpr std::array<sim::PickupRule, 7> kLegacyPickups{{
    {.kind = sim::PickupKind::kChest, .item_id = kLegacyChestItemId},
    {.kind = sim::PickupKind::kMeat, .item_id = 517, .amount = 30},
    {.kind = sim::PickupKind::kGold, .item_id = 969, .amount = 10},
    {.kind = sim::PickupKind::kManual, .item_id = 12208, .amount = 100, .duration_ms = 30000},
    {.kind = sim::PickupKind::kHaste, .item_id = 12016, .amount = 50, .duration_ms = 10000},
    {.kind = sim::PickupKind::kHolyWater, .item_id = 523, .reach_units = sim::Cells(12)},
    {.kind = sim::PickupKind::kMagnetAll, .item_id = 12210},
}};
inline constexpr std::array<sim::PlantRule, 6> kLegacyPlants{{
    {.foe_id = 1078, .pickup = sim::PickupKind::kMeat, .weight = 30},
    {.foe_id = 1081, .pickup = sim::PickupKind::kGold, .weight = 25},
    {.foe_id = 1080, .pickup = sim::PickupKind::kHaste, .weight = 15},
    {.foe_id = 1079, .pickup = sim::PickupKind::kManual, .weight = 15},
    {.foe_id = 1083, .pickup = sim::PickupKind::kMagnetAll, .weight = 10},
    {.foe_id = 1082, .pickup = sim::PickupKind::kHolyWater, .weight = 5},
}};
inline void UseLegacyPlants(sim::SurvivorsTuning& tuning) {
  tuning.plant_block_cells = kLegacyPlantBlockCells;
  tuning.plant_cells_per_plant = kLegacyPlantCellsPerPlant;
  tuning.plant_kills_per_minute = 0;
  tuning.plant_start_count = 0;
  tuning.plant_start_cells = 0;
  tuning.most_live_plants = 0;
  tuning.plant_forget_units = 0;
}

// Les règles livrées, sur l'arène d'essai : la liste des arènes change, et
// les plantes et leurs objets restent ceux des rulesets antérieurs, pour que
// les ors sur piliers gardent leur bilan au bit près.
[[nodiscard]] inline const sim::SurvivorsRules& GoldenRules() {
  static const sim::SurvivorsRules kRules = [] {
    sim::SurvivorsRules rules = sim::ShippedSurvivorsRules();
    rules.arenas = kGoldenArenas;
    UseLegacyPlants(rules.tuning);
    rules.plants = kLegacyPlants;
    rules.pickups = kLegacyPickups;
    return rules;
  }();
  return kRules;
}

[[nodiscard]] inline sim::SurvivorsStart GoldenStart(std::uint64_t seed) {
  return sim::SurvivorsStart{.seed = seed,
                             .class_id = kGoldenClassId,
                             .arena_id = kGoldenArenaId,
                             .first_skill_id = kGoldenFirstSkillId,
                             .loadout = {}};
}

// `steps` pas joués par PlayScripted depuis le départ de graine `seed` ; rien
// si Start refuse.
[[nodiscard]] inline std::optional<sim::SurvivorsPlay> PlayGolden(std::uint64_t seed,
                                                                  std::uint32_t steps) {
  auto run =
      sim::SurvivorsRun::Start(GoldenRules(), PillarArena(kGoldenArenaCells), GoldenStart(seed));
  if (!run) return std::nullopt;
  std::optional<sim::SurvivorsPlay> play(std::in_place, std::move(*run));
  PlayScripted(*play, steps);
  return play;
}

// Ce qu'on épingle d'un monstre.
struct GoldenFoe {
  std::uint32_t serial = 0;
  std::uint16_t foe_id = 0;
  sim::FieldPoint position;
  std::int32_t hp = 0;
  [[nodiscard]] bool operator==(const GoldenFoe&) const = default;
};
inline constexpr std::size_t kGoldenFoeCount = 5;

// Les cinq premiers monstres de `field`, dans l'ordre de naissance ; des
// entrées vides s'il y en a moins.
[[nodiscard]] inline std::array<GoldenFoe, kGoldenFoeCount> FirstFoes(
    const sim::SurvivorsField& field) {
  std::array<GoldenFoe, kGoldenFoeCount> first{};
  for (std::size_t index = 0; index < first.size() && index < field.foes.size(); ++index) {
    const sim::SurvivorsFoe& foe = field.foes[index];
    first[index] = GoldenFoe{
        .serial = foe.serial, .foe_id = foe.foe_id, .position = foe.position, .hp = foe.hp};
  }
  return first;
}

// Ce qu'on épingle de ce que le héros possède : l'Id et le niveau, dans
// l'ordre des emplacements.
struct GoldenOwned {
  std::uint16_t id = 0;
  std::uint8_t level = 0;
  [[nodiscard]] bool operator==(const GoldenOwned&) const = default;
};

// ⚠ LE BILAN EN OR, après kGoldenSteps pas de la partie en or. Il est épinglé
// AVEC la version des règles : si cet essai change, montez
// kSurvivorsRulesetVersion et kGoldenRulesetVersion ensemble, car un journal ne
// se rejoue que contre les règles qui l'ont produit. Les mêmes valeurs sous
// MSVC et sous g++-14.
inline constexpr std::uint16_t kGoldenRulesetVersion = 8;
inline constexpr sim::SurvivorsTally kGoldenTally{
    .survived_steps = 15000,
    .kills = 316,
    .boss_kills = 0,
    .kill_points = 871,
    .level = 9,
    .exp_total = 404,
    .hp_left = 360,
    .damage_dealt = 12891,
    .gems_gathered = 241,
    .foes_alive = 400,
    .outcome = sim::SurvivorsOutcome::kRunning,
    .elite_kills = 2,
    .chests_opened = 1,
    .evolutions = 0,
};
inline constexpr sim::FieldPoint kGoldenHeroPosition{83, 114004};
inline constexpr std::array<GoldenFoe, kGoldenFoeCount> kGoldenFoes{{
    {.serial = 169, .foe_id = 1007, .position = {37039, 93300}, .hp = 22},
    {.serial = 170, .foe_id = 1063, .position = {34897, 92380}, .hp = 11},
    {.serial = 172, .foe_id = 1007, .position = {34517, 95108}, .hp = 22},
    {.serial = 177, .foe_id = 1007, .position = {23976, 87597}, .hp = 22},
    {.serial = 182, .foe_id = 1007, .position = {35775, 108512}, .hp = 22},
}};
inline constexpr std::array<GoldenOwned, 7> kGoldenSkills{{
    {.id = 60001, .level = 1},  // le coup de baguette
    {.id = 19, .level = 1},     // Fire Bolt
    {.id = 21, .level = 2},     // Thunderstorm
    {.id = 15, .level = 2},     // Frost Diver
    {.id = 18, .level = 1},     // Fire Wall
    {.id = 86, .level = 1},     // Water Ball
    {.id = 87, .level = 1},     // Ice Wall
}};
inline constexpr std::array<GoldenOwned, 2> kGoldenCards{{
    {.id = 4003, .level = 1},  // Pupa Card
    {.id = 4019, .level = 1},  // Hornet Card
}};

}  // namespace ro::test_support

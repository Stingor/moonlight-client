#include <array>
#include <cstdint>
#include <span>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "survivors_shipped.h"

namespace ro::sim::survivors {

namespace {

// ── LES BOSS DE L'ARÈNE 1 ───────────────────────────────────────────────────
// Quatre mini-boss, un toutes les deux minutes de 3:00 à 9:00, puis Baphomet à
// 15:00, dont la chute est la victoire. Chacun marche à sa propre vitesse, hors
// de l'échelle de la horde ; les plus rapides du jeu sont ralentis pour que le
// héros puisse encore les fuir. Un coup part pour la première fois une période
// après l'arrivée du boss.

// Mastering invoque des Porings.
constexpr std::array<BossMoveRule, 1> kMasteringMoves{{
    {.kind = BossMoveKind::kSummon,
     .first_ms = 6000,
     .period_ms = 6000,
     .foe_id = kPoringId,
     .count = 6},
}};

// Eclipse bondit vers le héros, après une annonce.
constexpr std::array<BossMoveRule, 1> kEclipseMoves{{
    {.kind = BossMoveKind::kLeap,
     .first_ms = 5000,
     .period_ms = 5000,
     .telegraph_ms = 400,
     .reach_units = Cells(4)},
}};

// Dragon Fly, touché, s'éloigne une seconde.
constexpr std::array<BossMoveRule, 1> kDragonFlyMoves{{
    {.kind = BossMoveKind::kFlee, .period_ms = 3000, .effect_ms = 1000},
}};

// Vagabond Wolf hurle : ses loups pressent le pas.
constexpr std::array<BossMoveRule, 1> kVagabondWolfMoves{{
    {.kind = BossMoveKind::kHowl,
     .first_ms = 10000,
     .period_ms = 10000,
     .effect_ms = 4000,
     .foe_id = kWolfId,
     .boost_pct = 30},
}};

// Baphomet, par phase de PV restants : 100–60 %, 60–25 %, 25–0 %. La charge
// manque à la deuxième phase ; l'invocation double et le séisme presse à la
// troisième.
constexpr std::uint8_t kBaphometRagePct = 25;
constexpr std::uint8_t kBaphometQuakePct = 60;
// Une charge de six cases, courue en 400 ms.
constexpr BossMoveRule kBaphometCharge{.kind = BossMoveKind::kCharge,
                                       .first_ms = 8000,
                                       .period_ms = 8000,
                                       .telegraph_ms = 800,
                                       .effect_ms = 400,
                                       .reach_units = Cells(6)};
constexpr BossMoveRule kBaphometSummon{.kind = BossMoveKind::kSummon,
                                       .first_ms = 12000,
                                       .period_ms = 12000,
                                       .foe_id = kBaphometJrId,
                                       .count = 4};
// Trois cercles, le premier sur le héros, les autres à quatre cases au plus.
constexpr BossMoveRule kBaphometQuake{.kind = BossMoveKind::kQuake,
                                      .period_ms = 6000,
                                      .telegraph_ms = 1000,
                                      .count = 3,
                                      .reach_units = Cells(4),
                                      .radius_units = CellTenths(25),
                                      .damage = 40};

// Le coup `move`, actif de `from_hp_pct` (compris) à `to_hp_pct` (exclu).
[[nodiscard]] constexpr BossMoveRule Phased(BossMoveRule move, std::uint8_t from_hp_pct,
                                            std::uint8_t to_hp_pct) noexcept {
  move.from_hp_pct = from_hp_pct;
  move.to_hp_pct = to_hp_pct;
  return move;
}

// Le coup `move`, à une autre période et à un autre compte.
[[nodiscard]] constexpr BossMoveRule Pressed(BossMoveRule move, std::uint32_t period_ms,
                                             std::uint8_t count) noexcept {
  move.period_ms = period_ms;
  move.count = count;
  return move;
}

constexpr std::uint32_t kRageQuakeMs = 4000;
constexpr std::uint8_t kRageSummons = 8;

constexpr std::array<BossMoveRule, 6> kBaphometMoves{{
    Phased(kBaphometCharge, kWholeHpPct, kBaphometQuakePct),
    Phased(kBaphometSummon, kWholeHpPct, kBaphometRagePct),
    Phased(kBaphometQuake, kBaphometQuakePct, kBaphometRagePct),
    Phased(kBaphometCharge, kBaphometRagePct, 0),
    Phased(Pressed(kBaphometSummon, kBaphometSummon.period_ms, kRageSummons), kBaphometRagePct, 0),
    Phased(Pressed(kBaphometQuake, kRageQuakeMs, kBaphometQuake.count), kBaphometRagePct, 0),
}};

constexpr std::uint8_t kEclipseEscort = 8;
constexpr std::uint8_t kVagabondWolfEscort = 4;

constexpr std::array<BossRule, 5> kBosses{{
    {.at_ms = 3 * kMinuteMs,
     .foe_id = kMasteringId,
     .walk_ms_per_cell = 300,
     .moves = kMasteringMoves},
    {.at_ms = 5 * kMinuteMs,
     .foe_id = kEclipseId,
     .walk_ms_per_cell = 200,
     .escort_foe_id = kLunaticId,
     .escort_count = kEclipseEscort,
     .moves = kEclipseMoves},
    {.at_ms = 7 * kMinuteMs,
     .foe_id = kDragonFlyId,
     .walk_ms_per_cell = 100,
     .moves = kDragonFlyMoves},
    {.at_ms = 9 * kMinuteMs,
     .foe_id = kVagabondWolfId,
     .walk_ms_per_cell = 180,
     .escort_foe_id = kWolfId,
     .escort_count = kVagabondWolfEscort,
     .moves = kVagabondWolfMoves},
    {.at_ms = 15 * kMinuteMs,
     .foe_id = kBaphometId,
     .final = true,
     .walk_ms_per_cell = 200,
     .moves = kBaphometMoves},
}};

}  // namespace

std::span<const BossRule> ShippedBosses() { return kBosses; }

}  // namespace ro::sim::survivors

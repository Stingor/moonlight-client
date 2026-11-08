#include <array>
#include <cstdint>
#include <span>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "survivors_shipped.h"

namespace ro::sim::survivors {

namespace {

using enum FoeSize;
using enum SurvivorsElement;

// ── LES NEIGES DE LUTIE : leurs monstres, leurs vagues, leurs boss ─────────
// La courbe de la prairie, avec les monstres du champ de Lutie, de la fabrique
// de jouets et de la grotte de glace : les petits de Noël, puis les jouets,
// puis la nuit de la glace. La horde est surtout d'Eau, où la foudre mord et le
// Feu non. Le niveau, la taille, l'élément, la marche et l'intervalle de
// contact sont ceux de db/pre-re/mob_db.yml du serveur ; les PV, les dégâts et
// l'XP sont ceux du mode. Poring et Poporing viennent du bestiaire de la
// prairie.

constexpr std::uint16_t kPoporingId = 1031;
constexpr std::uint16_t kSantaPoringId = 1062;
constexpr std::uint16_t kMarinId = 1242;
constexpr std::uint16_t kCookieId = 1265;
constexpr std::uint16_t kChristmasCookieId = 1246;
constexpr std::uint16_t kSiromaId = 1776;
constexpr std::uint16_t kCruiserId = 1248;
constexpr std::uint16_t kSasquatchId = 1243;
constexpr std::uint16_t kGazetiId = 1778;
constexpr std::uint16_t kRoweenId = 1782;
constexpr std::uint16_t kSnowierId = 1775;
constexpr std::uint16_t kBabyHatiiId = 1515;
constexpr std::uint16_t kIceTitanId = 1777;
constexpr std::uint16_t kIceicleId = 1789;
constexpr std::uint16_t kChepetId = 1250;
constexpr std::uint16_t kMystCaseId = 1249;
constexpr std::uint16_t kStormyKnightId = 1251;
constexpr std::uint16_t kFreezerId = 1319;
constexpr std::uint16_t kAntonioId = 1247;
constexpr std::uint16_t kHatiiId = 1252;

// Les glaçons ne bougent ni ne touchent : des murs vivants, comme Pupa.
constexpr std::uint32_t kStillWalkMs = 0;
constexpr std::int32_t kNoTouchDamage = 0;
constexpr std::uint32_t kNoTouchMs = 0;

constexpr FoeBehaviour kStill{.move = FoeMove::kStill};
constexpr FoeBehaviour kSiromaPack{.pack = 5};
constexpr FoeBehaviour kRoweenPack{.pack = 4};
constexpr FoeBehaviour kBabyHatiiPack{.pack = 4};
constexpr FoeBehaviour kKnockbackImmune{.traits = kFoeKnockbackImmune};
// Sasquatch, à moins de six cases, annonce puis file quatre cases à seize
// cases par seconde, comme Savage.
constexpr FoeBehaviour kSasquatchCharge{.move = FoeMove::kCharger,
                                        .period_ms = 5000,
                                        .telegraph_ms = 500,
                                        .active_ms = 250,
                                        .reach_units = Cells(6),
                                        .dash_units_per_s = Cells(16)};
// Gazeti se tient à six cases et lance ses éclats de glace, une fois toutes
// les quatre de ses attaques.
constexpr FoeBehaviour kGazetiShards{.move = FoeMove::kRanged,
                                     .period_ms = 2304,
                                     .telegraph_ms = 500,
                                     .reach_units = Cells(6),
                                     .shot_damage = 6,
                                     .shot_units_per_s = Cells(10),
                                     .shot_radius_units = CellTenths(3)};
constexpr FoeBehaviour kBossImmunities{
    .traits = static_cast<std::uint8_t>(kFoeKnockbackImmune | kFoeFreezeImmune)};

// Un monstre commun : niveau RO, taille, élément, PV, marche en ms par case,
// dégâts et intervalle de contact, XP, comportement.
[[nodiscard]] constexpr FoeRule Common(std::uint16_t foe_id, std::uint8_t ro_level, FoeSize size,
                                       SurvivorsElement element, std::int32_t hp,
                                       std::uint32_t walk_ms, std::int32_t touch_damage,
                                       std::uint32_t touch_ms, std::uint32_t exp,
                                       const FoeBehaviour& behaviour = {}) noexcept {
  return FoeRule{.foe_id = foe_id,
                 .ro_level = ro_level,
                 .rank = FoeRank::kCommon,
                 .size = size,
                 .element = element,
                 .hp = hp,
                 .ro_walk_ms_per_cell = walk_ms,
                 .touch_damage = touch_damage,
                 .touch_interval_ms = touch_ms,
                 .exp = exp,
                 .behaviour = behaviour};
}

// Un boss : comme un commun, à son rang, avec son armure.
[[nodiscard]] constexpr FoeRule Boss(FoeRank rank, std::uint16_t foe_id, std::uint8_t ro_level,
                                     FoeSize size, SurvivorsElement element, std::int32_t hp,
                                     std::uint32_t walk_ms, std::int32_t touch_damage,
                                     std::uint32_t touch_ms, std::uint32_t exp,
                                     const FoeBehaviour& behaviour = {},
                                     std::int32_t armor_pct = 0) noexcept {
  FoeRule rule =
      Common(foe_id, ro_level, size, element, hp, walk_ms, touch_damage, touch_ms, exp, behaviour);
  rule.rank = rank;
  rule.armor_pct = armor_pct;
  return rule;
}

// Common(Id, niveau, taille, élément, PV, ms par case, contact, ms, XP[, comportement]).
constexpr std::array<FoeRule, kSnowFoeCount> kSnowFoes{{
    Common(kSantaPoringId, 3, kMedium, kHoly, 16, 400, 4, 1672, 1),
    Common(kMarinId, 15, kMedium, kWater, 22, 400, 5, 1872, 1),
    Common(kCookieId, 25, kSmall, kNeutral, 20, 200, 5, 1036, 1),
    Common(kChristmasCookieId, 28, kSmall, kHoly, 40, 400, 7, 1248, 3),
    Common(kSiromaId, 42, kSmall, kWater, 30, 180, 4, 864, 2, kSiromaPack),
    Common(kCruiserId, 35, kMedium, kNeutral, 50, 400, 7, 1296, 3),
    Common(kSasquatchId, 30, kLarge, kNeutral, 70, 300, 9, 1260, 5, kSasquatchCharge),
    Common(kGazetiId, 55, kMedium, kWater, 55, 190, 6, 1152, 6, kGazetiShards),
    Common(kRoweenId, 31, kMedium, kWind, 45, 200, 6, 824, 4, kRoweenPack),
    Common(kSnowierId, 60, kLarge, kWater, 110, 220, 12, 936, 10),
    Common(kBabyHatiiId, 61, kMedium, kWater, 90, 300, 13, 879, 10, kBabyHatiiPack),
    Common(kIceTitanId, 60, kLarge, kWater, 170, 250, 20, 861, 18, kKnockbackImmune),
    Common(kIceicleId, 38, kSmall, kWater, 40, kStillWalkMs, kNoTouchDamage, kNoTouchMs, 2, kStill),
    // Les mini-boss et le boss final : PV fixes, marche de leur BossRule.
    Boss(FoeRank::kMiniBoss, kChepetId, 42, kMedium, kFire, 700, 300, 12, 1072, 25),
    Boss(FoeRank::kMiniBoss, kMystCaseId, 38, kMedium, kNeutral, 900, 250, 12, 1248, 30),
    Boss(FoeRank::kMiniBoss, kStormyKnightId, 77, kLarge, kWind, 1300, 200, 13, 936, 40),
    Boss(FoeRank::kMiniBoss, kFreezerId, 72, kMedium, kWater, 2000, 180, 12, 1452, 55),
    Boss(FoeRank::kMiniBoss, kAntonioId, 10, kMedium, kHoly, 900, 100, 5, 1440, 40),
    Boss(FoeRank::kBoss, kHatiiId, 73, kLarge, kWater, 11000, 125, 35, 868, 0, kBossImmunities),
}};

// ── Les vagues ─────────────────────────────────────────────────────────────
// Les planchers, les intervalles et les lots de la prairie : seul le
// bestiaire change.

constexpr std::uint32_t kActBreatherMs = 20000;
constexpr std::uint32_t kSilenceMs = 14 * kMinuteMs + 50000;

constexpr std::array<std::uint16_t, 3> kWave0Foes{kSantaPoringId, kMarinId, kPoringId};
constexpr std::array<std::uint32_t, 3> kWave0Weights{50, 30, 20};
constexpr std::array<std::uint16_t, 4> kWave1Foes{kSantaPoringId, kMarinId, kPoringId, kCookieId};
constexpr std::array<std::uint32_t, 4> kWave1Weights{30, 30, 20, 20};
constexpr std::array<std::uint16_t, 4> kWave2Foes{kMarinId, kCookieId, kPoporingId, kSantaPoringId};
constexpr std::array<std::uint32_t, 4> kWave2Weights{25, 30, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave3Foes{kCookieId, kChristmasCookieId, kPoporingId,
                                                  kMarinId};
constexpr std::array<std::uint32_t, 4> kWave3Weights{25, 30, 20, 25};
constexpr std::array<std::uint16_t, 4> kWave4Foes{kChristmasCookieId, kSiromaId, kCookieId,
                                                  kCruiserId};
constexpr std::array<std::uint32_t, 4> kWave4Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave5Foes{kSiromaId, kCruiserId, kChristmasCookieId,
                                                  kRoweenId};
constexpr std::array<std::uint32_t, 4> kWave5Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave6Foes{kCruiserId, kRoweenId, kSasquatchId, kSiromaId};
constexpr std::array<std::uint32_t, 4> kWave6Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 3> kWave7Foes{kSasquatchId, kGazetiId, kRoweenId};
constexpr std::array<std::uint32_t, 3> kWave7Weights{40, 30, 30};
constexpr std::array<std::uint16_t, 4> kWave8Foes{kGazetiId, kSasquatchId, kCruiserId, kSiromaId};
constexpr std::array<std::uint32_t, 4> kWave8Weights{25, 25, 30, 20};
constexpr std::array<std::uint16_t, 4> kWave9Foes{kSnowierId, kGazetiId, kRoweenId, kBabyHatiiId};
constexpr std::array<std::uint32_t, 4> kWave9Weights{30, 25, 30, 15};
constexpr std::array<std::uint16_t, 4> kWave10Foes{kSnowierId, kBabyHatiiId, kGazetiId,
                                                   kSasquatchId};
constexpr std::array<std::uint32_t, 4> kWave10Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave11Foes{kSnowierId, kBabyHatiiId, kGazetiId,
                                                   kIceTitanId};
constexpr std::array<std::uint32_t, 4> kWave11Weights{25, 30, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave12Foes{kIceTitanId, kSnowierId, kBabyHatiiId,
                                                   kGazetiId};
constexpr std::array<std::uint32_t, 4> kWave12Weights{25, 30, 25, 20};
constexpr std::array<std::uint16_t, 5> kWave13Foes{kIceTitanId, kSnowierId, kBabyHatiiId, kGazetiId,
                                                   kSiromaId};
constexpr std::array<std::uint32_t, 5> kWave13Weights{25, 25, 20, 15, 15};
constexpr std::array<std::uint16_t, 4> kWave14Foes{kIceTitanId, kSnowierId, kBabyHatiiId,
                                                   kGazetiId};
constexpr std::array<std::uint32_t, 4> kWave14Weights{30, 25, 25, 20};

[[nodiscard]] constexpr WaveRule Wave(std::uint32_t from_ms, std::span<const std::uint16_t> foe_ids,
                                      std::span<const std::uint32_t> weights, std::uint16_t floor,
                                      std::uint32_t interval_ms, std::uint8_t batch) noexcept {
  return WaveRule{.from_ms = from_ms,
                  .foe_ids = foe_ids,
                  .weights = weights,
                  .floor = floor,
                  .interval_ms = interval_ms,
                  .batch = batch};
}

// Wave(début, réserve, poids, plancher, intervalle en ms, lot).
constexpr std::array<WaveRule, 18> kSnowWaves{{
    Wave(0, kWave0Foes, kWave0Weights, 15, 1000, 2),
    Wave(1 * kMinuteMs, kWave1Foes, kWave1Weights, 30, 1000, 3),
    Wave(2 * kMinuteMs, kWave2Foes, kWave2Weights, 45, 900, 3),
    Wave(3 * kMinuteMs, kWave3Foes, kWave3Weights, 60, 900, 4),
    Wave(4 * kMinuteMs, kWave4Foes, kWave4Weights, 75, 800, 4),
    Wave(5 * kMinuteMs, kWave5Foes, kWave5Weights, 90, 800, 5),
    Wave(6 * kMinuteMs, kWave6Foes, kWave6Weights, 110, 700, 5),
    Wave(7 * kMinuteMs, kWave7Foes, kWave7Weights, 80, 700, 6),
    Wave(7 * kMinuteMs + kActBreatherMs, kWave7Foes, kWave7Weights, 130, 700, 6),
    Wave(8 * kMinuteMs, kWave8Foes, kWave8Weights, 150, 600, 6),
    Wave(9 * kMinuteMs, kWave9Foes, kWave9Weights, 170, 600, 7),
    Wave(10 * kMinuteMs, kWave10Foes, kWave10Weights, 190, 500, 7),
    Wave(11 * kMinuteMs, kWave11Foes, kWave11Weights, 160, 500, 8),
    Wave(11 * kMinuteMs + kActBreatherMs, kWave11Foes, kWave11Weights, 210, 500, 8),
    Wave(12 * kMinuteMs, kWave12Foes, kWave12Weights, 230, 450, 8),
    Wave(13 * kMinuteMs, kWave13Foes, kWave13Weights, 260, 400, 9),
    Wave(14 * kMinuteMs, kWave14Foes, kWave14Weights, 300, 350, 10),
    // La vague silencieuse : sa réserve ne sert qu'à garder des poids valides.
    Wave(kSilenceMs, kWave14Foes, kWave14Weights, 0, 1000, 0),
}};

// ── Les événements ─────────────────────────────────────────────────────────

constexpr std::array<EventRule, 7> kSnowEvents{{
    {.at_ms = 90000, .kind = EventKind::kSwarm, .foe_id = kSantaPoringId, .count = 24},
    {.at_ms = 150000,
     .kind = EventKind::kRing,
     .foe_id = kIceicleId,
     .count = 20,
     .radius_units = Cells(6)},
    {.at_ms = 270000, .kind = EventKind::kRaid, .foe_id = kCookieId, .count = 24},
    {.at_ms = 390000,
     .kind = EventKind::kRing,
     .foe_id = kIceicleId,
     .count = 14,
     .radius_units = Cells(7)},
    {.at_ms = 510000, .kind = EventKind::kSwarm, .foe_id = kSiromaId, .count = 30},
    {.at_ms = 630000, .kind = EventKind::kRaid, .foe_id = kBabyHatiiId, .count = 18},
    {.at_ms = 810000,
     .kind = EventKind::kRing,
     .foe_id = kIceTitanId,
     .count = 20,
     .radius_units = Cells(8)},
}};

// ── Les boss ───────────────────────────────────────────────────────────────

// Chepet, la poupée de la fabrique, appelle des Christmas Cookie.
constexpr std::array<BossMoveRule, 1> kChepetMoves{{
    {.kind = BossMoveKind::kSummon,
     .first_ms = 6000,
     .period_ms = 6000,
     .foe_id = kChristmasCookieId,
     .count = 5},
}};

// Le Myst Case, la boîte à surprises, lâche des Cookie.
constexpr std::array<BossMoveRule, 1> kMystCaseMoves{{
    {.kind = BossMoveKind::kSummon,
     .first_ms = 7000,
     .period_ms = 7000,
     .foe_id = kCookieId,
     .count = 6},
}};

// LE GEL de Storm Gust et du souffle de Hatii : le héros que le cercle blesse
// marche 40 % moins vite pendant deux secondes.
constexpr std::int32_t kFrostSlowPct = 40;
constexpr std::uint32_t kFrostMs = 2000;

// Stormy Knight lève Storm Gust : trois cercles annoncés, qui gèlent.
constexpr std::array<BossMoveRule, 1> kStormyKnightMoves{{
    {.kind = BossMoveKind::kQuake,
     .first_ms = 7000,
     .period_ms = 7000,
     .telegraph_ms = 1000,
     .count = 3,
     .reach_units = Cells(4),
     .radius_units = CellTenths(25),
     .damage = 25,
     .slow_pct = kFrostSlowPct,
     .slow_ms = kFrostMs},
}};

// Freezer hurle : les Hatii Babe pressent le pas.
constexpr std::array<BossMoveRule, 1> kFreezerMoves{{
    {.kind = BossMoveKind::kHowl,
     .first_ms = 10000,
     .period_ms = 10000,
     .effect_ms = 4000,
     .foe_id = kBabyHatiiId,
     .boost_pct = 30},
}};

// Antonio, touché, s'enfuit une seconde : on le rattrape pour son coffre.
constexpr std::array<BossMoveRule, 1> kAntonioMoves{{
    {.kind = BossMoveKind::kFlee, .period_ms = 3000, .effect_ms = 1000},
}};

// Hatii, par phase de PV restants : 100–60 %, 60–25 %, 25–0 %. Ses Hatii Babe
// remplacent les Baphomet Jr. ; son souffle gèle là où tremblait le sol.
constexpr std::uint8_t kHatiiRagePct = 25;
constexpr std::uint8_t kHatiiBreathPct = 60;
constexpr BossMoveRule kHatiiCharge{.kind = BossMoveKind::kCharge,
                                    .first_ms = 8000,
                                    .period_ms = 8000,
                                    .telegraph_ms = 800,
                                    .effect_ms = 400,
                                    .reach_units = Cells(6)};
constexpr BossMoveRule kHatiiSummon{.kind = BossMoveKind::kSummon,
                                    .first_ms = 12000,
                                    .period_ms = 12000,
                                    .foe_id = kBabyHatiiId,
                                    .count = 4};
constexpr BossMoveRule kHatiiBreath{.kind = BossMoveKind::kQuake,
                                    .period_ms = 6000,
                                    .telegraph_ms = 1000,
                                    .count = 3,
                                    .reach_units = Cells(4),
                                    .radius_units = CellTenths(25),
                                    .damage = 40,
                                    .slow_pct = kFrostSlowPct,
                                    .slow_ms = kFrostMs};

[[nodiscard]] constexpr BossMoveRule Phased(BossMoveRule move, std::uint8_t from_hp_pct,
                                            std::uint8_t to_hp_pct) noexcept {
  move.from_hp_pct = from_hp_pct;
  move.to_hp_pct = to_hp_pct;
  return move;
}

[[nodiscard]] constexpr BossMoveRule Pressed(BossMoveRule move, std::uint32_t period_ms,
                                             std::uint8_t count) noexcept {
  move.period_ms = period_ms;
  move.count = count;
  return move;
}

constexpr std::uint32_t kRageBreathMs = 4000;
constexpr std::uint8_t kRageSummons = 8;

constexpr std::array<BossMoveRule, 6> kHatiiMoves{{
    Phased(kHatiiCharge, kWholeHpPct, kHatiiBreathPct),
    Phased(kHatiiSummon, kWholeHpPct, kHatiiRagePct),
    Phased(kHatiiBreath, kHatiiBreathPct, kHatiiRagePct),
    Phased(kHatiiCharge, kHatiiRagePct, 0),
    Phased(Pressed(kHatiiSummon, kHatiiSummon.period_ms, kRageSummons), kHatiiRagePct, 0),
    Phased(Pressed(kHatiiBreath, kRageBreathMs, kHatiiBreath.count), kHatiiRagePct, 0),
}};

constexpr std::uint8_t kFreezerEscort = 4;

// Un mini-boss toutes les deux minutes de 3:00 à 11:00, puis Hatii à 15:00.
// Antonio, à 11:00, ne frappe presque pas : il fuit, et son coffre est le
// répit avant la nuit de la glace.
constexpr std::array<BossRule, 6> kSnowBosses{{
    {.at_ms = 3 * kMinuteMs, .foe_id = kChepetId, .walk_ms_per_cell = 300, .moves = kChepetMoves},
    {.at_ms = 5 * kMinuteMs,
     .foe_id = kMystCaseId,
     .walk_ms_per_cell = 250,
     .moves = kMystCaseMoves},
    {.at_ms = 7 * kMinuteMs,
     .foe_id = kStormyKnightId,
     .walk_ms_per_cell = 200,
     .moves = kStormyKnightMoves},
    {.at_ms = 9 * kMinuteMs,
     .foe_id = kFreezerId,
     .walk_ms_per_cell = 180,
     .escort_foe_id = kBabyHatiiId,
     .escort_count = kFreezerEscort,
     .moves = kFreezerMoves},
    {.at_ms = 11 * kMinuteMs,
     .foe_id = kAntonioId,
     .walk_ms_per_cell = 120,
     .moves = kAntonioMoves},
    {.at_ms = 15 * kMinuteMs,
     .foe_id = kHatiiId,
     .final = true,
     .walk_ms_per_cell = 125,
     .moves = kHatiiMoves},
}};

}  // namespace

std::span<const FoeRule> SnowFoes() { return kSnowFoes; }

ArenaContent SnowContent() {
  return ArenaContent{.waves = kSnowWaves, .bosses = kSnowBosses, .events = kSnowEvents};
}

}  // namespace ro::sim::survivors

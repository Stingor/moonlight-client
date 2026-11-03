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

// ── LE DÉSERT DE MORROC : ses monstres, ses vagues, ses boss ──────────────
// La courbe de la prairie, avec les monstres des champs de Morroc : les petits
// du sable, puis la fourmilière, puis la nuit de la pyramide. Comme pour la
// prairie, le niveau, la taille, l'élément, la marche et l'intervalle de
// contact sont ceux de db/pre-re/mob_db.yml du serveur ; les PV, les dégâts
// et l'XP sont ceux du mode. Drops, Condor, Golem, Mummy, Mastering et
// Vagabond Wolf viennent du bestiaire de la prairie.

constexpr std::uint16_t kPickyId = 1049;
constexpr std::uint16_t kBabyDesertWolfId = 1107;
constexpr std::uint16_t kPecoPecoEggId = 1047;
constexpr std::uint16_t kAntEggId = 1097;
constexpr std::uint16_t kPecoPecoId = 1019;
constexpr std::uint16_t kMukaId = 1055;
constexpr std::uint16_t kHodeId = 1127;
constexpr std::uint16_t kScorpionId = 1001;
constexpr std::uint16_t kDesertWolfId = 1106;
constexpr std::uint16_t kAndreId = 1095;
constexpr std::uint16_t kPiereId = 1160;
constexpr std::uint16_t kDeniroId = 1105;
constexpr std::uint16_t kSandmanId = 1165;
constexpr std::uint16_t kFrilldoraId = 1119;
constexpr std::uint16_t kMardukId = 1140;
constexpr std::uint16_t kIsisId = 1029;
constexpr std::uint16_t kVeritId = 1032;
constexpr std::uint16_t kMatyrId = 1146;
constexpr std::uint16_t kRequiemId = 1164;
constexpr std::uint16_t kPasanaId = 1154;
constexpr std::uint16_t kPhreeoniId = 1159;
constexpr std::uint16_t kMayaId = 1147;
constexpr std::uint16_t kPharaohId = 1157;

// Les œufs ne bougent ni ne touchent : des murs vivants, comme Pupa.
constexpr std::uint32_t kStillWalkMs = 0;
constexpr std::int32_t kNoTouchDamage = 0;
constexpr std::uint32_t kNoTouchMs = 0;

constexpr FoeBehaviour kStill{.move = FoeMove::kStill};
constexpr FoeBehaviour kDesertPack{.pack = 6};
constexpr FoeBehaviour kMatyrPack{.pack = 4};
constexpr FoeBehaviour kKnockbackImmune{.traits = kFoeKnockbackImmune};
// Hode s'enterre une seconde toutes les six, comme Smokie se cache.
constexpr FoeBehaviour kBurrower{.traits = kFoeHides, .period_ms = 6000, .active_ms = 1000};
constexpr FoeBehaviour kBossImmunities{
    .traits = static_cast<std::uint8_t>(kFoeKnockbackImmune | kFoeFreezeImmune)};

// LE PHARAON PORTE UNE ARMURE : il retranche 40 % des coups reçus. Le combat
// final dure ainsi plus que sous Baphomet, au corps à corps comme à distance.
constexpr std::int32_t kPharaohArmorPct = 40;

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
constexpr std::array<FoeRule, kDesertFoeCount> kDesertFoes{{
    Common(kPickyId, 3, kSmall, kFire, 14, 200, 4, 988, 1),
    Common(kBabyDesertWolfId, 9, kSmall, kFire, 20, 300, 5, 1600, 1),
    Common(kPecoPecoEggId, 3, kSmall, kNeutral, 50, kStillWalkMs, kNoTouchDamage, kNoTouchMs, 2,
           kStill),
    Common(kAntEggId, 4, kSmall, kNeutral, 50, kStillWalkMs, kNoTouchDamage, kNoTouchMs, 2, kStill),
    Common(kPecoPecoId, 19, kLarge, kFire, 45, 200, 7, 1564, 3),
    Common(kMukaId, 17, kLarge, kEarth, 50, 300, 7, 1960, 3),
    Common(kHodeId, 26, kMedium, kEarth, 40, 200, 7, 1480, 3, kBurrower),
    Common(kScorpionId, 24, kSmall, kFire, 30, 200, 7, 1564, 2),
    Common(kDesertWolfId, 27, kMedium, kFire, 40, 200, 7, 1120, 5, kDesertPack),
    Common(kAndreId, 17, kSmall, kEarth, 35, 300, 6, 1288, 3),
    Common(kPiereId, 18, kSmall, kEarth, 35, 200, 6, 1288, 3),
    Common(kDeniroId, 19, kSmall, kEarth, 35, 150, 6, 1288, 3),
    Common(kSandmanId, 34, kMedium, kEarth, 55, 250, 8, 1672, 4),
    Common(kFrilldoraId, 30, kMedium, kFire, 60, 300, 8, 1540, 7),
    Common(kMardukId, 40, kLarge, kFire, 100, 300, 10, 1540, 10, kKnockbackImmune),
    Common(kIsisId, 47, kLarge, kDark, 110, 200, 12, 1384, 12),
    Common(kVeritId, 38, kMedium, kUndead, 90, 250, 13, 2468, 12),
    Common(kMatyrId, 31, kMedium, kDark, 60, 150, 8, 864, 10, kMatyrPack),
    Common(kRequiemId, 35, kMedium, kDark, 95, 400, 14, 1516, 13),
    Common(kPasanaId, 61, kMedium, kFire, 160, 165, 22, 976, 20),
    // Les mini-boss et le boss final : PV fixes, marche de leur BossRule.
    Boss(FoeRank::kMiniBoss, kPhreeoniId, 69, kLarge, kNeutral, 900, 200, 12, 1020, 30),
    Boss(FoeRank::kMiniBoss, kMayaId, 81, kLarge, kEarth, 1200, 100, 12, 864, 40),
    Boss(FoeRank::kBoss, kPharaohId, 93, kLarge, kDark, 11000, 125, 35, 868, 0, kBossImmunities,
         kPharaohArmorPct),
}};

// ── Les vagues ─────────────────────────────────────────────────────────────
// Les planchers, les intervalles et les lots de la prairie : seul le
// bestiaire change.

constexpr std::uint32_t kActBreatherMs = 20000;
constexpr std::uint32_t kSilenceMs = 14 * kMinuteMs + 50000;

constexpr std::array<std::uint16_t, 3> kWave0Foes{kPickyId, kBabyDesertWolfId, kPoringId};
constexpr std::array<std::uint32_t, 3> kWave0Weights{50, 30, 20};
constexpr std::array<std::uint16_t, 4> kWave1Foes{kPickyId, kBabyDesertWolfId, kPoringId, kDropsId};
constexpr std::array<std::uint32_t, 4> kWave1Weights{30, 30, 20, 20};
constexpr std::array<std::uint16_t, 4> kWave2Foes{kBabyDesertWolfId, kDropsId, kScorpionId,
                                                  kPickyId};
constexpr std::array<std::uint32_t, 4> kWave2Weights{25, 25, 30, 20};
constexpr std::array<std::uint16_t, 4> kWave3Foes{kDropsId, kPecoPecoId, kScorpionId, kPickyId};
constexpr std::array<std::uint32_t, 4> kWave3Weights{25, 30, 20, 25};
constexpr std::array<std::uint16_t, 4> kWave4Foes{kPecoPecoId, kMukaId, kCondorId, kScorpionId};
constexpr std::array<std::uint32_t, 4> kWave4Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave5Foes{kMukaId, kHodeId, kCondorId, kAndreId};
constexpr std::array<std::uint32_t, 4> kWave5Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave6Foes{kAndreId, kPiereId, kDeniroId, kHodeId};
constexpr std::array<std::uint32_t, 4> kWave6Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 3> kWave7Foes{kSandmanId, kDeniroId, kPiereId};
constexpr std::array<std::uint32_t, 3> kWave7Weights{40, 30, 30};
constexpr std::array<std::uint16_t, 4> kWave8Foes{kSandmanId, kAndreId, kFrilldoraId, kHodeId};
constexpr std::array<std::uint32_t, 4> kWave8Weights{25, 25, 30, 20};
constexpr std::array<std::uint16_t, 4> kWave9Foes{kDesertWolfId, kFrilldoraId, kSandmanId,
                                                  kMardukId};
constexpr std::array<std::uint32_t, 4> kWave9Weights{30, 25, 30, 10};
constexpr std::array<std::uint16_t, 4> kWave10Foes{kMardukId, kFrilldoraId, kGolemId,
                                                   kDesertWolfId};
constexpr std::array<std::uint32_t, 4> kWave10Weights{25, 30, 15, 20};
constexpr std::array<std::uint16_t, 4> kWave11Foes{kIsisId, kVeritId, kMatyrId, kMardukId};
constexpr std::array<std::uint32_t, 4> kWave11Weights{25, 30, 20, 20};
constexpr std::array<std::uint16_t, 4> kWave12Foes{kMummyId, kVeritId, kIsisId, kRequiemId};
constexpr std::array<std::uint32_t, 4> kWave12Weights{30, 25, 20, 25};
constexpr std::array<std::uint16_t, 5> kWave13Foes{kMummyId, kRequiemId, kMatyrId, kIsisId,
                                                   kVeritId};
constexpr std::array<std::uint32_t, 5> kWave13Weights{25, 25, 15, 20, 15};
constexpr std::array<std::uint16_t, 4> kWave14Foes{kPasanaId, kRequiemId, kIsisId, kMummyId};
constexpr std::array<std::uint32_t, 4> kWave14Weights{25, 25, 20, 30};

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
constexpr std::array<WaveRule, 18> kDesertWaves{{
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

constexpr std::array<EventRule, 7> kDesertEvents{{
    {.at_ms = 90000, .kind = EventKind::kSwarm, .foe_id = kBabyDesertWolfId, .count = 24},
    {.at_ms = 150000,
     .kind = EventKind::kRing,
     .foe_id = kPecoPecoEggId,
     .count = 28,
     .radius_units = Cells(6)},
    {.at_ms = 270000, .kind = EventKind::kSwarm, .foe_id = kScorpionId, .count = 30},
    {.at_ms = 390000,
     .kind = EventKind::kRing,
     .foe_id = kAntEggId,
     .count = 12,
     .radius_units = Cells(7)},
    {.at_ms = 510000, .kind = EventKind::kRaid, .foe_id = kDeniroId, .count = 24},
    {.at_ms = 630000, .kind = EventKind::kRaid, .foe_id = kDesertWolfId, .count = 18},
    {.at_ms = 810000,
     .kind = EventKind::kRing,
     .foe_id = kMummyId,
     .count = 30,
     .radius_units = Cells(8)},
}};

// ── Les boss ───────────────────────────────────────────────────────────────

// Mastering invoque des Porings, comme sur la prairie.
constexpr std::array<BossMoveRule, 1> kMasteringMoves{{
    {.kind = BossMoveKind::kSummon,
     .first_ms = 6000,
     .period_ms = 6000,
     .foe_id = kPoringId,
     .count = 6},
}};

// Phreeoni charge, après une annonce.
constexpr std::array<BossMoveRule, 1> kPhreeoniMoves{{
    {.kind = BossMoveKind::kCharge,
     .first_ms = 6000,
     .period_ms = 6000,
     .telegraph_ms = 700,
     .effect_ms = 400,
     .reach_units = Cells(6)},
}};

// Maya, reine de la fourmilière, appelle ses Andre.
constexpr std::array<BossMoveRule, 1> kMayaMoves{{
    {.kind = BossMoveKind::kSummon,
     .first_ms = 7000,
     .period_ms = 7000,
     .foe_id = kAndreId,
     .count = 6},
}};

// Vagabond Wolf hurle : les loups du désert pressent le pas.
constexpr std::array<BossMoveRule, 1> kVagabondWolfMoves{{
    {.kind = BossMoveKind::kHowl,
     .first_ms = 10000,
     .period_ms = 10000,
     .effect_ms = 4000,
     .foe_id = kDesertWolfId,
     .boost_pct = 30},
}};

// Le Pharaon, par phase de PV restants : 100–60 %, 60–25 %, 25–0 %. Ses
// Isis remplacent les Baphomet Jr. ; ses séismes de sable sont ceux de
// Baphomet.
constexpr std::uint8_t kPharaohRagePct = 25;
constexpr std::uint8_t kPharaohQuakePct = 60;
constexpr BossMoveRule kPharaohCharge{.kind = BossMoveKind::kCharge,
                                      .first_ms = 8000,
                                      .period_ms = 8000,
                                      .telegraph_ms = 800,
                                      .effect_ms = 400,
                                      .reach_units = Cells(6)};
constexpr BossMoveRule kPharaohSummon{.kind = BossMoveKind::kSummon,
                                      .first_ms = 12000,
                                      .period_ms = 12000,
                                      .foe_id = kIsisId,
                                      .count = 4};
constexpr BossMoveRule kPharaohQuake{.kind = BossMoveKind::kQuake,
                                     .period_ms = 6000,
                                     .telegraph_ms = 1000,
                                     .count = 3,
                                     .reach_units = Cells(4),
                                     .radius_units = CellTenths(25),
                                     .damage = 40};

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

constexpr std::uint32_t kRageQuakeMs = 4000;
constexpr std::uint8_t kRageSummons = 8;

constexpr std::array<BossMoveRule, 6> kPharaohMoves{{
    Phased(kPharaohCharge, kWholeHpPct, kPharaohQuakePct),
    Phased(kPharaohSummon, kWholeHpPct, kPharaohRagePct),
    Phased(kPharaohQuake, kPharaohQuakePct, kPharaohRagePct),
    Phased(kPharaohCharge, kPharaohRagePct, 0),
    Phased(Pressed(kPharaohSummon, kPharaohSummon.period_ms, kRageSummons), kPharaohRagePct, 0),
    Phased(Pressed(kPharaohQuake, kRageQuakeMs, kPharaohQuake.count), kPharaohRagePct, 0),
}};

constexpr std::uint8_t kVagabondWolfEscort = 4;

constexpr std::array<BossRule, 5> kDesertBosses{{
    {.at_ms = 3 * kMinuteMs,
     .foe_id = kMasteringId,
     .walk_ms_per_cell = 300,
     .moves = kMasteringMoves},
    {.at_ms = 5 * kMinuteMs,
     .foe_id = kPhreeoniId,
     .walk_ms_per_cell = 200,
     .moves = kPhreeoniMoves},
    {.at_ms = 7 * kMinuteMs, .foe_id = kMayaId, .walk_ms_per_cell = 200, .moves = kMayaMoves},
    {.at_ms = 9 * kMinuteMs,
     .foe_id = kVagabondWolfId,
     .walk_ms_per_cell = 180,
     .escort_foe_id = kDesertWolfId,
     .escort_count = kVagabondWolfEscort,
     .moves = kVagabondWolfMoves},
    {.at_ms = 15 * kMinuteMs,
     .foe_id = kPharaohId,
     .final = true,
     .walk_ms_per_cell = 125,
     .moves = kPharaohMoves},
}};

}  // namespace

std::span<const FoeRule> DesertFoes() { return kDesertFoes; }

ArenaContent DesertContent() {
  return ArenaContent{.waves = kDesertWaves, .bosses = kDesertBosses, .events = kDesertEvents};
}

}  // namespace ro::sim::survivors

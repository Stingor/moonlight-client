#include <algorithm>
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

// ── LE LABORATOIRE DE LIGHTHALZEN : ses monstres, ses vagues, ses boss ──────
// La courbe de la neige, avec les monstres des champs de Lighthalzen, puis
// ceux des trois premiers étages du labo : les bêtes des champs, puis Remover
// et les chercheurs, puis leurs gardiens. La horde mêle tous les éléments. Le
// niveau, la taille, l'élément et la marche sont ceux de db/pre-re/mob_db.yml
// du serveur, et l'intervalle de contact son délai d'attaque, jamais sous
// kLeastTouchMs ; les PV, les dégâts et l'XP sont ceux du mode. Aucune carte de
// ces monstres n'est dans la réserve du mode : aucun ne porte de carte.

constexpr std::uint16_t kMetalingId = 1613;
constexpr std::uint16_t kCaramelId = 1103;
constexpr std::uint16_t kHornId = 1128;
constexpr std::uint16_t kAnophelesId = 1627;
constexpr std::uint16_t kHoldenId = 1628;
constexpr std::uint16_t kStemWormId = 1215;
constexpr std::uint16_t kHillWindId = 1680;
constexpr std::uint16_t kRemoverId = 1682;
constexpr std::uint16_t kEgnigemCeniaId = 1652;
constexpr std::uint16_t kKavachIcarusId = 1656;
constexpr std::uint16_t kErrendeEbeceeId = 1655;
constexpr std::uint16_t kWickebineTresId = 1653;
constexpr std::uint16_t kArmeyerDinzeId = 1654;
constexpr std::uint16_t kLaurellWeinderId = 1657;
constexpr std::uint16_t kSeyrenWindsorId = 1634;
constexpr std::uint16_t kEremesGuileId = 1635;
constexpr std::uint16_t kHowardAltEisenId = 1636;
constexpr std::uint16_t kCecilDamonId = 1638;
constexpr std::uint16_t kKathryneKeyronId = 1639;
// Les mini-boss : le robot du premier étage, le MVP du deuxième, et trois des
// gardiens à 99.
constexpr std::uint16_t kGeminiId = 1681;
constexpr std::uint16_t kEgnigemCeniaMvpId = 1658;
constexpr std::uint16_t kWhitesmithHowardId = 1642;
constexpr std::uint16_t kHighPriestMargarethaId = 1643;
constexpr std::uint16_t kAssassinCrossEremesId = 1641;
// Les MVP du troisième étage, dont la partie tire le sien.
constexpr std::uint16_t kLordKnightSeyrenId = 1646;
constexpr std::uint16_t kHighWizardKathryneId = 1651;

// Le plus court intervalle de contact, celui des loups de la prairie : un
// gardien frappe toutes les 76 ms dans mob_db, une horde au contact serait
// une mort sans recours.
constexpr std::uint32_t kLeastTouchMs = 1200;

// Un monstre sans contact n'a pas d'intervalle.
constexpr std::uint32_t kNoTouchMs = 0;

constexpr FoeBehaviour kAnophelesSwarm{.pack = 4};
constexpr FoeBehaviour kHillWindPack{.pack = 3};
constexpr FoeBehaviour kKnockbackImmune{.traits = kFoeKnockbackImmune};
// Holden fouit sous le sol, comme Hode au désert.
constexpr FoeBehaviour kBurrower{.traits = kFoeHides, .period_ms = 6000, .active_ms = 1000};
// Remover, le nettoyeur, laisse une flaque de son seau à sa mort.
constexpr FoeBehaviour kRemoverBucket{.puddle_damage = 3,
                                      .puddle_radius_units = CellTenths(15),
                                      .puddle_ms = 4000,
                                      .puddle_period_ms = 500};
// Les tireurs se tiennent à six cases : Stem Worm crache, Kavach Icarus et
// Cecil Damon décochent, Laurell Weinder et Kathryne Keyron lancent un sort.
constexpr FoeBehaviour kSpitter{.move = FoeMove::kRanged,
                                .period_ms = 3000,
                                .telegraph_ms = 500,
                                .reach_units = Cells(6),
                                .shot_damage = 5,
                                .shot_units_per_s = Cells(9),
                                .shot_radius_units = CellTenths(3)};
constexpr FoeBehaviour kArcher{.move = FoeMove::kRanged,
                               .period_ms = 2400,
                               .telegraph_ms = 400,
                               .reach_units = Cells(7),
                               .shot_damage = 6,
                               .shot_units_per_s = Cells(14),
                               .shot_radius_units = CellTenths(3)};
constexpr FoeBehaviour kCaster{.move = FoeMove::kRanged,
                               .period_ms = 3200,
                               .telegraph_ms = 700,
                               .reach_units = Cells(6),
                               .shot_damage = 10,
                               .shot_units_per_s = Cells(8),
                               .shot_radius_units = CellTenths(5)};
// Wickebine Tres, l'assassin des chercheurs, bondit vers le héros.
constexpr FoeBehaviour kAssassinBlink{
    .move = FoeMove::kBlinker, .period_ms = 4000, .telegraph_ms = 300, .reach_units = Cells(3)};
// Eremes Guile disparaît, puis reparaît au contact.
constexpr FoeBehaviour kCloaker{.traits = kFoeHides, .period_ms = 5000, .active_ms = 1500};
// Seyren Windsor, le chevalier, annonce puis charge comme Savage.
constexpr FoeBehaviour kKnightCharge{.move = FoeMove::kCharger,
                                     .period_ms = 5000,
                                     .telegraph_ms = 500,
                                     .active_ms = 250,
                                     .reach_units = Cells(6),
                                     .dash_units_per_s = Cells(16)};
constexpr FoeBehaviour kBossImmunities{
    .traits = static_cast<std::uint8_t>(kFoeKnockbackImmune | kFoeFreezeImmune)};

// LES LEVIERS DU LABO : ses salles enferment le héros avec la horde, et ses
// communs n'y gardent que cette part de leurs PV et de leurs dégâts (contact,
// tir et flaque, au moins un point) ; un mini-boss garde ses PV, mais pas plus
// de ses dégâts de contact. Ils tiennent la partie dans la cible du biome : une
// carte dure, de 40 à 50 % de victoires au robot.
constexpr std::int32_t kLabHpPct = 40;
constexpr std::int32_t kLabDamagePct = 35;
constexpr std::int32_t kWholePct = 100;

[[nodiscard]] constexpr std::int32_t Scaled(std::int32_t value, std::int32_t pct) noexcept {
  return value == 0 ? 0 : std::max(1, value * pct / kWholePct);
}

// Les tirs et les flaques d'un commun suivent la part de ses dégâts.
[[nodiscard]] constexpr FoeBehaviour ScaledBehaviour(FoeBehaviour behaviour) noexcept {
  behaviour.shot_damage = Scaled(behaviour.shot_damage, kLabDamagePct);
  behaviour.puddle_damage = Scaled(behaviour.puddle_damage, kLabDamagePct);
  return behaviour;
}

[[nodiscard]] constexpr std::uint32_t TouchMsOf(std::uint32_t attack_delay_ms) noexcept {
  return attack_delay_ms < kLeastTouchMs ? kLeastTouchMs : attack_delay_ms;
}

// Un monstre commun : niveau RO, taille, élément, PV, marche en ms par case,
// dégâts et délai d'attaque, XP, comportement.
[[nodiscard]] constexpr FoeRule Common(std::uint16_t foe_id, std::uint8_t ro_level, FoeSize size,
                                       SurvivorsElement element, std::int32_t hp,
                                       std::uint32_t walk_ms, std::int32_t touch_damage,
                                       std::uint32_t attack_delay_ms, std::uint32_t exp,
                                       const FoeBehaviour& behaviour = {}) noexcept {
  return FoeRule{.foe_id = foe_id,
                 .ro_level = ro_level,
                 .rank = FoeRank::kCommon,
                 .size = size,
                 .element = element,
                 .hp = Scaled(hp, kLabHpPct),
                 .ro_walk_ms_per_cell = walk_ms,
                 .touch_damage = Scaled(touch_damage, kLabDamagePct),
                 .touch_interval_ms = touch_damage == 0 ? kNoTouchMs : TouchMsOf(attack_delay_ms),
                 .exp = exp,
                 .behaviour = ScaledBehaviour(behaviour)};
}

// Un boss : comme un commun, à son rang, avec son armure.
[[nodiscard]] constexpr FoeRule Boss(FoeRank rank, std::uint16_t foe_id, std::uint8_t ro_level,
                                     FoeSize size, SurvivorsElement element, std::int32_t hp,
                                     std::uint32_t walk_ms, std::int32_t touch_damage,
                                     std::uint32_t attack_delay_ms, std::uint32_t exp,
                                     const FoeBehaviour& behaviour = {},
                                     std::int32_t armor_pct = 0) noexcept {
  FoeRule rule = Common(foe_id, ro_level, size, element, hp, walk_ms, touch_damage, attack_delay_ms,
                        exp, behaviour);
  rule.rank = rank;
  rule.hp = hp;
  if (rank != FoeRank::kMiniBoss) rule.touch_damage = touch_damage;
  rule.armor_pct = armor_pct;
  return rule;
}

// Common(Id, niveau, taille, élément, PV, ms par case, contact, délai d'attaque, XP[,
// comportement]).
constexpr std::array<FoeRule, kLabFoeCount> kLabFoes{{
    // Les champs de Lighthalzen.
    Common(kMetalingId, 26, kSmall, kNeutral, 16, 300, 4, 384, 1),
    Common(kCaramelId, 23, kSmall, kEarth, 18, 200, 4, 1604, 1),
    Common(kHornId, 18, kMedium, kEarth, 20, 200, 5, 1528, 1),
    Common(kAnophelesId, 23, kSmall, kWind, 10, 200, 3, 140, 1, kAnophelesSwarm),
    Common(kHoldenId, 36, kSmall, kEarth, 30, 300, 5, 140, 3, kBurrower),
    Common(kStemWormId, 40, kMedium, kWind, 40, 200, 5, 1500, 4, kSpitter),
    Common(kHillWindId, 45, kMedium, kWind, 35, 170, 6, 504, 4, kHillWindPack),
    // Le premier étage : le nettoyeur et les chercheurs.
    Common(kRemoverId, 55, kMedium, kUndead, 50, 250, 7, 1536, 6, kRemoverBucket),
    Common(kEgnigemCeniaId, 58, kMedium, kFire, 50, 145, 7, 576, 6),
    Common(kKavachIcarusId, 60, kMedium, kWind, 45, 150, 5, 576, 6, kArcher),
    Common(kErrendeEbeceeId, 59, kMedium, kHoly, 50, 130, 6, 576, 6),
    Common(kWickebineTresId, 62, kMedium, kPoison, 70, 120, 9, 576, 9, kAssassinBlink),
    Common(kArmeyerDinzeId, 66, kMedium, kEarth, 100, 120, 10, 576, 10, kKnockbackImmune),
    Common(kLaurellWeinderId, 61, kMedium, kGhost, 60, 150, 7, 576, 9, kCaster),
    // Le troisième étage : les gardiens.
    Common(kSeyrenWindsorId, 91, kMedium, kFire, 130, 170, 14, 76, 16, kKnightCharge),
    Common(kEremesGuileId, 87, kMedium, kPoison, 110, 180, 13, 76, 15, kCloaker),
    Common(kHowardAltEisenId, 83, kMedium, kWater, 160, 180, 16, 76, 18, kKnockbackImmune),
    Common(kCecilDamonId, 82, kMedium, kWind, 100, 180, 10, 76, 15, kArcher),
    Common(kKathryneKeyronId, 92, kMedium, kGhost, 110, 150, 11, 1152, 16, kCaster),
    // Les mini-boss et les MVP : PV fixes, marche de leur BossRule.
    Boss(FoeRank::kMiniBoss, kGeminiId, 72, kMedium, kWater, 900, 200, 12, 1872, 25),
    Boss(FoeRank::kMiniBoss, kEgnigemCeniaMvpId, 79, kMedium, kFire, 1000, 100, 12, 1008, 30),
    Boss(FoeRank::kMiniBoss, kWhitesmithHowardId, 99, kMedium, kWater, 1400, 100, 14, 76, 40),
    Boss(FoeRank::kMiniBoss, kHighPriestMargarethaId, 99, kMedium, kHoly, 1800, 125, 12, 1152, 50),
    Boss(FoeRank::kMiniBoss, kAssassinCrossEremesId, 99, kMedium, kPoison, 1200, 100, 10, 76, 40),
    Boss(FoeRank::kBoss, kLordKnightSeyrenId, 99, kMedium, kFire, 11000, 100, 35, 76, 0,
         kBossImmunities),
    Boss(FoeRank::kBoss, kHighWizardKathryneId, 99, kMedium, kGhost, 9500, 150, 30, 1152, 0,
         kBossImmunities),
}};

// ── Les vagues ─────────────────────────────────────────────────────────────
// Les planchers, les intervalles et les lots de la neige : seul le bestiaire
// change.

constexpr std::uint32_t kActBreatherMs = 20000;
constexpr std::uint32_t kSilenceMs = 14 * kMinuteMs + 50000;

constexpr std::array<std::uint16_t, 3> kWave0Foes{kMetalingId, kCaramelId, kHornId};
constexpr std::array<std::uint32_t, 3> kWave0Weights{40, 30, 30};
constexpr std::array<std::uint16_t, 4> kWave1Foes{kMetalingId, kCaramelId, kHornId, kAnophelesId};
constexpr std::array<std::uint32_t, 4> kWave1Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave2Foes{kMetalingId, kAnophelesId, kHornId, kHoldenId};
constexpr std::array<std::uint32_t, 4> kWave2Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave3Foes{kHoldenId, kMetalingId, kAnophelesId,
                                                  kStemWormId};
constexpr std::array<std::uint32_t, 4> kWave3Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave4Foes{kHoldenId, kStemWormId, kHillWindId, kMetalingId};
constexpr std::array<std::uint32_t, 4> kWave4Weights{25, 25, 25, 25};
constexpr std::array<std::uint16_t, 4> kWave5Foes{kHillWindId, kStemWormId, kHoldenId, kRemoverId};
constexpr std::array<std::uint32_t, 4> kWave5Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave6Foes{kRemoverId, kEgnigemCeniaId, kHillWindId,
                                                  kStemWormId};
constexpr std::array<std::uint32_t, 4> kWave6Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 3> kWave7Foes{kEgnigemCeniaId, kKavachIcarusId,
                                                  kErrendeEbeceeId};
constexpr std::array<std::uint32_t, 3> kWave7Weights{40, 30, 30};
constexpr std::array<std::uint16_t, 4> kWave8Foes{kKavachIcarusId, kErrendeEbeceeId, kRemoverId,
                                                  kEgnigemCeniaId};
constexpr std::array<std::uint32_t, 4> kWave8Weights{25, 25, 25, 25};
constexpr std::array<std::uint16_t, 4> kWave9Foes{kWickebineTresId, kKavachIcarusId,
                                                  kLaurellWeinderId, kErrendeEbeceeId};
constexpr std::array<std::uint32_t, 4> kWave9Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave10Foes{kWickebineTresId, kArmeyerDinzeId,
                                                   kLaurellWeinderId, kEgnigemCeniaId};
constexpr std::array<std::uint32_t, 4> kWave10Weights{30, 25, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave11Foes{kArmeyerDinzeId, kLaurellWeinderId,
                                                   kWickebineTresId, kSeyrenWindsorId};
constexpr std::array<std::uint32_t, 4> kWave11Weights{25, 30, 25, 20};
constexpr std::array<std::uint16_t, 4> kWave12Foes{kSeyrenWindsorId, kCecilDamonId, kArmeyerDinzeId,
                                                   kEremesGuileId};
constexpr std::array<std::uint32_t, 4> kWave12Weights{25, 30, 25, 20};
constexpr std::array<std::uint16_t, 5> kWave13Foes{
    kSeyrenWindsorId, kEremesGuileId, kHowardAltEisenId, kCecilDamonId, kKathryneKeyronId};
constexpr std::array<std::uint32_t, 5> kWave13Weights{25, 20, 20, 20, 15};
constexpr std::array<std::uint16_t, 4> kWave14Foes{kHowardAltEisenId, kSeyrenWindsorId,
                                                   kKathryneKeyronId, kEremesGuileId};
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
constexpr std::array<WaveRule, 18> kLabWaves{{
    Wave(0, kWave0Foes, kWave0Weights, 6, 1000, 2),
    Wave(1 * kMinuteMs, kWave1Foes, kWave1Weights, 12, 1000, 3),
    Wave(2 * kMinuteMs, kWave2Foes, kWave2Weights, 18, 900, 3),
    Wave(3 * kMinuteMs, kWave3Foes, kWave3Weights, 24, 900, 4),
    Wave(4 * kMinuteMs, kWave4Foes, kWave4Weights, 30, 800, 4),
    Wave(5 * kMinuteMs, kWave5Foes, kWave5Weights, 36, 800, 5),
    Wave(6 * kMinuteMs, kWave6Foes, kWave6Weights, 44, 700, 5),
    Wave(7 * kMinuteMs, kWave7Foes, kWave7Weights, 32, 700, 6),
    Wave(7 * kMinuteMs + kActBreatherMs, kWave7Foes, kWave7Weights, 52, 700, 6),
    Wave(8 * kMinuteMs, kWave8Foes, kWave8Weights, 60, 600, 6),
    Wave(9 * kMinuteMs, kWave9Foes, kWave9Weights, 68, 600, 7),
    Wave(10 * kMinuteMs, kWave10Foes, kWave10Weights, 76, 500, 7),
    Wave(11 * kMinuteMs, kWave11Foes, kWave11Weights, 64, 500, 8),
    Wave(11 * kMinuteMs + kActBreatherMs, kWave11Foes, kWave11Weights, 84, 500, 8),
    Wave(12 * kMinuteMs, kWave12Foes, kWave12Weights, 92, 450, 8),
    Wave(13 * kMinuteMs, kWave13Foes, kWave13Weights, 104, 400, 9),
    Wave(14 * kMinuteMs, kWave14Foes, kWave14Weights, 120, 350, 10),
    // La vague silencieuse : sa réserve ne sert qu'à garder des poids valides.
    Wave(kSilenceMs, kWave14Foes, kWave14Weights, 0, 1000, 0),
}};

// ── Les événements ─────────────────────────────────────────────────────────
// Deux fois, les cuves les plus proches du héros cèdent (EventKind::kTanks) :
// des chercheurs, puis des gardiens, en sortent dans une flaque d'acide.

constexpr std::array<EventRule, 7> kLabEvents{{
    {.at_ms = 90000, .kind = EventKind::kSwarm, .foe_id = kAnophelesId, .count = 24},
    {.at_ms = 150000,
     .kind = EventKind::kRing,
     .foe_id = kHoldenId,
     .count = 20,
     .radius_units = Cells(6)},
    {.at_ms = 270000, .kind = EventKind::kRaid, .foe_id = kMetalingId, .count = 24},
    {.at_ms = 390000,
     .kind = EventKind::kTanks,
     .foe_id = kEgnigemCeniaId,
     .count = 4,
     .radius_units = Cells(40),
     .piece_id = kLabTankPieceId},
    {.at_ms = 510000, .kind = EventKind::kRaid, .foe_id = kRemoverId, .count = 18},
    {.at_ms = 630000,
     .kind = EventKind::kTanks,
     .foe_id = kSeyrenWindsorId,
     .count = 6,
     .radius_units = Cells(40),
     .piece_id = kLabTankPieceId},
    {.at_ms = 810000,
     .kind = EventKind::kRing,
     .foe_id = kEremesGuileId,
     .count = 16,
     .radius_units = Cells(8)},
}};

// ── Les boss ───────────────────────────────────────────────────────────────

// Gemini-S58, le robot du premier étage, appelle des Metaling.
constexpr std::array<BossMoveRule, 1> kGeminiMoves{{
    {.kind = BossMoveKind::kSummon,
     .first_ms = 6000,
     .period_ms = 6000,
     .foe_id = kMetalingId,
     .count = 6},
}};

// Egnigem Cenia, devenue MVP, charge.
constexpr std::array<BossMoveRule, 1> kEgnigemMoves{{
    {.kind = BossMoveKind::kCharge,
     .first_ms = 7000,
     .period_ms = 7000,
     .telegraph_ms = 700,
     .effect_ms = 400,
     .reach_units = Cells(6)},
}};

// Howard, le forgeron : Cart Termination, une charge plus longue.
constexpr std::array<BossMoveRule, 1> kHowardMoves{{
    {.kind = BossMoveKind::kCharge,
     .first_ms = 6000,
     .period_ms = 6000,
     .telegraph_ms = 800,
     .effect_ms = 600,
     .reach_units = Cells(8)},
}};

// Margaretha, la prêtresse : elle presse les chercheurs, et Magnus Exorcismus
// tombe en cercles annoncés.
constexpr std::array<BossMoveRule, 2> kMargarethaMoves{{
    {.kind = BossMoveKind::kHowl,
     .first_ms = 10000,
     .period_ms = 10000,
     .effect_ms = 4000,
     .foe_id = kErrendeEbeceeId,
     .boost_pct = 30},
    {.kind = BossMoveKind::kQuake,
     .first_ms = 6000,
     .period_ms = 8000,
     .telegraph_ms = 1000,
     .count = 3,
     .reach_units = Cells(4),
     .radius_units = CellTenths(25),
     .damage = 15},
}};

// Eremes, l'assassin : il bondit, et s'éloigne quand on le touche.
constexpr std::array<BossMoveRule, 2> kEremesMoves{{
    {.kind = BossMoveKind::kLeap,
     .first_ms = 4000,
     .period_ms = 4000,
     .telegraph_ms = 300,
     .reach_units = Cells(5)},
    {.kind = BossMoveKind::kFlee, .period_ms = 3000, .effect_ms = 800},
}};

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

// Les MVP, par phase de PV restants : 100–60 %, 60–25 %, 25–0 %, comme le
// Pharaon et Hatii.
constexpr std::uint8_t kRagePct = 25;
constexpr std::uint8_t kSecondPhasePct = 60;
constexpr std::uint32_t kRageQuakeMs = 4000;
constexpr std::uint8_t kRageSummons = 6;

// Lord Knight Seyren : il charge, appelle ses chevaliers, et Bowling Bash
// tombe en cercles.
constexpr BossMoveRule kSeyrenCharge{.kind = BossMoveKind::kCharge,
                                     .first_ms = 7000,
                                     .period_ms = 7000,
                                     .telegraph_ms = 700,
                                     .effect_ms = 400,
                                     .reach_units = Cells(7)};
constexpr BossMoveRule kSeyrenSummon{.kind = BossMoveKind::kSummon,
                                     .first_ms = 12000,
                                     .period_ms = 12000,
                                     .foe_id = kSeyrenWindsorId,
                                     .count = 3};
constexpr BossMoveRule kBowlingBash{.kind = BossMoveKind::kQuake,
                                    .period_ms = 6000,
                                    .telegraph_ms = 900,
                                    .count = 3,
                                    .reach_units = Cells(3),
                                    .radius_units = CellTenths(25),
                                    .damage = 45};
constexpr std::array<BossMoveRule, 6> kLordKnightMoves{{
    Phased(kSeyrenCharge, kWholeHpPct, kSecondPhasePct),
    Phased(kSeyrenSummon, kWholeHpPct, kRagePct),
    Phased(kBowlingBash, kSecondPhasePct, kRagePct),
    Phased(kSeyrenCharge, kRagePct, 0),
    Phased(Pressed(kSeyrenSummon, kSeyrenSummon.period_ms, kRageSummons), kRagePct, 0),
    Phased(Pressed(kBowlingBash, kRageQuakeMs, kBowlingBash.count), kRagePct, 0),
}};

// High Wizard Kathryne : Storm Gust gèle (le gel de la neige), Meteor Storm
// tombe plus loin et plus fort, et elle appelle les sorcières du troisième
// étage.
constexpr std::int32_t kFrostSlowPct = 40;
constexpr std::uint32_t kFrostMs = 2000;
constexpr BossMoveRule kStormGust{.kind = BossMoveKind::kQuake,
                                  .first_ms = 6000,
                                  .period_ms = 7000,
                                  .telegraph_ms = 1000,
                                  .count = 3,
                                  .reach_units = Cells(4),
                                  .radius_units = CellTenths(25),
                                  .damage = 30,
                                  .slow_pct = kFrostSlowPct,
                                  .slow_ms = kFrostMs};
constexpr BossMoveRule kMeteorStorm{.kind = BossMoveKind::kQuake,
                                    .period_ms = 6000,
                                    .telegraph_ms = 1200,
                                    .count = 4,
                                    .reach_units = Cells(6),
                                    .radius_units = CellTenths(30),
                                    .damage = 50};
constexpr BossMoveRule kKathryneSummon{.kind = BossMoveKind::kSummon,
                                       .first_ms = 12000,
                                       .period_ms = 12000,
                                       .foe_id = kKathryneKeyronId,
                                       .count = 3};
constexpr std::array<BossMoveRule, 6> kHighWizardMoves{{
    Phased(kStormGust, kWholeHpPct, kSecondPhasePct),
    Phased(kKathryneSummon, kWholeHpPct, kRagePct),
    Phased(kMeteorStorm, kSecondPhasePct, kRagePct),
    Phased(Pressed(kStormGust, kRageQuakeMs, kStormGust.count), kRagePct, 0),
    Phased(Pressed(kKathryneSummon, kKathryneSummon.period_ms, kRageSummons), kRagePct, 0),
    Phased(Pressed(kMeteorStorm, kRageQuakeMs, kMeteorStorm.count), kRagePct, 0),
}};

constexpr std::uint8_t kMargarethaEscort = 4;

// Un mini-boss toutes les deux minutes de 3:00 à 11:00, puis le MVP à 15:00.
constexpr std::size_t kLabMiniBossCount = 5;
constexpr std::array<BossRule, kLabMiniBossCount> kLabMiniBosses{{
    {.at_ms = 3 * kMinuteMs, .foe_id = kGeminiId, .walk_ms_per_cell = 200, .moves = kGeminiMoves},
    {.at_ms = 5 * kMinuteMs,
     .foe_id = kEgnigemCeniaMvpId,
     .walk_ms_per_cell = 120,
     .moves = kEgnigemMoves},
    {.at_ms = 7 * kMinuteMs,
     .foe_id = kWhitesmithHowardId,
     .walk_ms_per_cell = 150,
     .moves = kHowardMoves},
    {.at_ms = 9 * kMinuteMs,
     .foe_id = kHighPriestMargarethaId,
     .walk_ms_per_cell = 150,
     .escort_foe_id = kErrendeEbeceeId,
     .escort_count = kMargarethaEscort,
     .moves = kMargarethaMoves},
    {.at_ms = 11 * kMinuteMs,
     .foe_id = kAssassinCrossEremesId,
     .walk_ms_per_cell = 110,
     .moves = kEremesMoves},
}};

// Les boss d'une partie : les cinq mini-boss, puis le MVP final tiré.
[[nodiscard]] constexpr std::array<BossRule, kLabMiniBossCount + 1> WithFinal(
    const BossRule& final_boss) noexcept {
  std::array<BossRule, kLabMiniBossCount + 1> bosses{};
  for (std::size_t index = 0; index < kLabMiniBossCount; ++index) {
    bosses[index] = kLabMiniBosses[index];
  }
  bosses[kLabMiniBossCount] = final_boss;
  return bosses;
}

constexpr std::array<BossRule, kLabMiniBossCount + 1> kLordKnightBosses =
    WithFinal({.at_ms = 15 * kMinuteMs,
               .foe_id = kLordKnightSeyrenId,
               .final = true,
               .walk_ms_per_cell = 100,
               .moves = kLordKnightMoves});
constexpr std::array<BossRule, kLabMiniBossCount + 1> kHighWizardBosses =
    WithFinal({.at_ms = 15 * kMinuteMs,
               .foe_id = kHighWizardKathryneId,
               .final = true,
               .walk_ms_per_cell = 150,
               .moves = kHighWizardMoves});

// LE TIRAGE DU MVP : comme au troisième étage du labo, la partie joue l'un
// de ces jeux de boss, que sa graine choisit.
constexpr std::array<std::span<const BossRule>, 2> kLabBossDraws{{
    kLordKnightBosses,
    kHighWizardBosses,
}};

}  // namespace

std::span<const FoeRule> LabFoes() { return kLabFoes; }

ArenaContent LabContent() {
  return ArenaContent{.waves = kLabWaves, .bosses = kLordKnightBosses, .events = kLabEvents};
}

std::span<const std::span<const BossRule>> LabBossDraws() { return kLabBossDraws; }

}  // namespace ro::sim::survivors

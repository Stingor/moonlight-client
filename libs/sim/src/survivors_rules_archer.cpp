#include <array>
#include <cstdint>
#include <span>

#include "ro/core/i18n.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "survivors_shipped.h"

namespace ro::sim::survivors {

namespace {

// ── L'ARCHER : sa classe, ses skills et ses évolutions ─────────────────────
// Il tient la horde au bout de ses flèches : il tire le plus loin, repousse ce
// qui approche, et sème des pièges que la horde qui le suit déclenche. Son
// faucon frappe pour lui. Archer au départ, il devient Hunter à sa première
// évolution, et Sniper à sa renaissance.

constexpr std::uint16_t kArcherClassId = 4;
constexpr std::uint16_t kArcherJobId = 3;     // l'apparence du départ
constexpr std::uint16_t kHunterJobId = 11;    // après une évolution
constexpr std::uint16_t kSniperJobId = 4012;  // rené
constexpr std::int32_t kArcherMaxHp = 350;
constexpr std::uint32_t kArcherWalkMsPerCell = 145;
constexpr std::int32_t kArcherMagnetTenths = 30;
constexpr std::int32_t kArcherDamagePctPerLevel = 1;
constexpr std::int32_t kArcherDamagePctCap = 25;
constexpr std::uint16_t kArcherRebornLevel = 50;
constexpr std::int32_t kArcherRebornDamagePct = 5;
constexpr std::int32_t kArcherFinalBossHpBonusPct = 90;

// Les flèches : la plus longue portée du mode.
constexpr std::int32_t kArrowReachCells = 10;
constexpr std::int32_t kArrowSpeedCellsPerSecond = 18;
constexpr std::int32_t kArrowHitRadiusTenths = 5;

// Arrow Shower : une pluie sur la foule la plus dense, qui l'écarte un peu.
constexpr std::int32_t kArrowShowerReachCells = 9;
constexpr std::uint32_t kArrowShowerIntervalMs = 150;
constexpr std::int32_t kArrowShowerSplashPct = 60;
constexpr std::int32_t kArrowShowerKnockbackCells = 1;

// Charge Arrow : une flèche lourde, qui renvoie loin ce qui approche.
constexpr std::int32_t kChargeArrowReachCells = 8;
constexpr std::int32_t kChargeArrowSpeedCellsPerSecond = 16;

// Blitz Beat : le faucon pique sur le plus proche, en plusieurs coups.
constexpr std::int32_t kBlitzBeatReachCells = 9;
constexpr std::uint32_t kBlitzBeatIntervalMs = 120;
constexpr std::int32_t kBlitzBeatRadiusTenths = 12;
constexpr std::int32_t kBlitzBeatSplashPct = 100;

// LES PIÈGES : un pilier d'un seul coup, posé à une case du héros, qui attend
// la horde qui le suit. Plusieurs pièges d'une même pose se répartissent
// autour de lui.
constexpr std::int32_t kTrapCircleCells = 1;
constexpr std::uint32_t kTrapLifeMs = 10000;
constexpr std::uint32_t kTrapRehitMs = 1000;
constexpr std::uint8_t kTrapHits = 1;

// Un niveau de tir droit : `amount` flèches de `damage`.
[[nodiscard]] constexpr SkillLevel Arrow(std::uint8_t amount, std::int32_t damage,
                                         std::uint32_t cooldown_ms) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .reach_units = Cells(kArrowReachCells),
                    .speed_units_per_s = Cells(kArrowSpeedCellsPerSecond),
                    .hit_radius_units = CellTenths(kArrowHitRadiusTenths)};
}

// Un niveau d'Arrow Shower : `amount` volées de `damage`, de rayon
// `radius_tenths` dixièmes de case.
[[nodiscard]] constexpr SkillLevel Shower(std::uint8_t amount, std::int32_t damage,
                                          std::uint32_t cooldown_ms,
                                          std::int32_t radius_tenths) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .interval_ms = kArrowShowerIntervalMs,
                    .reach_units = Cells(kArrowShowerReachCells),
                    .area_radius_units = CellTenths(radius_tenths),
                    .splash_pct = kArrowShowerSplashPct,
                    .knockback_units = Cells(kArrowShowerKnockbackCells)};
}

// Un niveau de Charge Arrow : `damage`, et un recul de `knockback_cells`.
[[nodiscard]] constexpr SkillLevel Charge(std::int32_t damage, std::uint32_t cooldown_ms,
                                          std::int32_t knockback_cells,
                                          std::uint8_t pierce) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .reach_units = Cells(kChargeArrowReachCells),
                    .speed_units_per_s = Cells(kChargeArrowSpeedCellsPerSecond),
                    .hit_radius_units = CellTenths(kArrowHitRadiusTenths),
                    .pierce = pierce,
                    .knockback_units = Cells(knockback_cells)};
}

// Un niveau de Blitz Beat : `strikes` piqués de `damage`.
[[nodiscard]] constexpr SkillLevel Beat(std::uint8_t strikes, std::int32_t damage,
                                        std::uint32_t cooldown_ms) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = strikes,
                    .interval_ms = kBlitzBeatIntervalMs,
                    .reach_units = Cells(kBlitzBeatReachCells),
                    .area_radius_units = CellTenths(kBlitzBeatRadiusTenths),
                    .splash_pct = kBlitzBeatSplashPct};
}

// Un niveau de piège : `damage` sur une aire de `radius_tenths` dixièmes de
// case, qui gèle `freeze_ms` et repousse de `knockback_tenths`.
[[nodiscard]] constexpr SkillLevel Trap(std::int32_t damage, std::uint32_t cooldown_ms,
                                        std::int32_t radius_tenths, std::uint32_t freeze_ms,
                                        std::int32_t knockback_tenths) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .reach_units = Cells(kTrapCircleCells),
                    .life_ms = kTrapLifeMs,
                    .rehit_ms = kTrapRehitMs,
                    .area_radius_units = CellTenths(radius_tenths),
                    .zone_hits = kTrapHits,
                    .knockback_units = CellTenths(knockback_tenths),
                    .freeze_ms = freeze_ms};
}

// Un niveau de posture : `bonus` à sa statistique pendant `life_ms`, toutes les
// `cooldown_ms`.
[[nodiscard]] constexpr SkillLevel Stance(std::uint32_t life_ms, std::uint32_t cooldown_ms,
                                          std::int32_t bonus) noexcept {
  return SkillLevel{.cooldown_ms = cooldown_ms, .life_ms = life_ms, .stance_bonus = bonus};
}

// Arrow(flèches, dégâts, recharge en ms) : le coup de base, le plus loin du mode.
constexpr std::array<SkillLevel, kMostSkillLevels> kDoubleStrafeLevels{{
    Arrow(2, 13, 1000),
    Arrow(2, 16, 1000),
    Arrow(2, 18, 950),
    Arrow(2, 22, 900),
    Arrow(3, 22, 900),
    Arrow(3, 26, 850),
    Arrow(3, 30, 800),
    Arrow(4, 34, 750),
}};

// Shower(volées, dégâts, recharge en ms, rayon en dixièmes).
constexpr std::array<SkillLevel, kMostSkillLevels> kArrowShowerLevels{{
    Shower(1, 18, 1800, 20),
    Shower(1, 22, 1800, 20),
    Shower(2, 22, 1700, 22),
    Shower(2, 26, 1700, 22),
    Shower(2, 30, 1600, 25),
    Shower(3, 30, 1500, 25),
    Shower(3, 35, 1400, 28),
    Shower(4, 41, 1300, 30),
}};

// Charge(dégâts, recharge en ms, recul en cases, perçage).
constexpr std::array<SkillLevel, kMostSkillLevels> kChargeArrowLevels{{
    Charge(33, 1600, 3, 1),
    Charge(39, 1550, 3, 1),
    Charge(39, 1500, 4, 1),
    Charge(48, 1400, 4, 1),
    Charge(48, 1300, 4, 2),
    Charge(58, 1250, 5, 2),
    Charge(67, 1150, 5, 2),
    Charge(77, 1000, 5, 3),
}};

// Improve Concentration(durée en ms, recharge en ms, recharge en %) : ses tirs
// se pressent, par moments.
constexpr std::array<SkillLevel, kMostSkillLevels> kImproveConcentrationLevels{{
    Stance(5000, 15000, 10),
    Stance(5500, 14500, 12),
    Stance(6000, 14000, 14),
    Stance(6500, 13500, 16),
    Stance(7000, 13000, 18),
    Stance(7500, 12500, 20),
    Stance(8000, 12000, 22),
    Stance(8000, 11000, 25),
}};

// Beat(piqués, dégâts, recharge en ms).
constexpr std::array<SkillLevel, kMostSkillLevels> kBlitzBeatLevels{{
    Beat(2, 15, 2200),
    Beat(2, 18, 2200),
    Beat(3, 18, 2100),
    Beat(3, 22, 2000),
    Beat(4, 22, 1900),
    Beat(4, 27, 1800),
    Beat(5, 27, 1700),
    Beat(5, 34, 1500),
}};

// Trap(dégâts, recharge en ms, rayon en dixièmes, gel en ms, recul en
// dixièmes) : Ankle Snare fige le premier qui passe.
constexpr std::array<SkillLevel, kMostSkillLevels> kAnkleSnareLevels{{
    Trap(6, 2600, 12, 2000, 0),
    Trap(8, 2500, 12, 2300, 0),
    Trap(8, 2400, 13, 2600, 0),
    Trap(11, 2300, 13, 2900, 0),
    Trap(11, 2200, 14, 3200, 0),
    Trap(14, 2100, 14, 3500, 0),
    Trap(16, 2000, 15, 3800, 0),
    Trap(19, 1800, 16, 4000, 0),
}};

// Land Mine : elle saute sous la horde, fort, et la secoue à peine.
constexpr std::array<SkillLevel, kMostSkillLevels> kLandMineLevels{{
    Trap(38, 2000, 15, 300, 0),
    Trap(46, 2000, 15, 300, 0),
    Trap(46, 1900, 16, 300, 0),
    Trap(55, 1800, 16, 300, 0),
    Trap(55, 1700, 17, 300, 0),
    Trap(66, 1600, 18, 300, 0),
    Trap(79, 1500, 19, 300, 0),
    Trap(96, 1400, 20, 300, 0),
}};

// Skid Trap : il renvoie la horde d'où elle vient.
constexpr std::array<SkillLevel, kMostSkillLevels> kSkidTrapLevels{{
    Trap(10, 2400, 15, 0, 30),
    Trap(11, 2300, 15, 0, 30),
    Trap(11, 2200, 15, 0, 35),
    Trap(14, 2100, 16, 0, 35),
    Trap(14, 2000, 16, 0, 40),
    Trap(18, 1900, 17, 0, 40),
    Trap(21, 1800, 17, 0, 45),
    Trap(26, 1600, 18, 0, 50),
}};

// Freezing Trap : il gèle une foule.
constexpr std::array<SkillLevel, kMostSkillLevels> kFreezingTrapLevels{{
    Trap(19, 3000, 20, 1500, 0),
    Trap(23, 2900, 20, 1700, 0),
    Trap(23, 2800, 21, 1900, 0),
    Trap(28, 2700, 21, 2100, 0),
    Trap(28, 2600, 22, 2300, 0),
    Trap(34, 2400, 23, 2500, 0),
    Trap(41, 2200, 24, 2700, 0),
    Trap(51, 2000, 25, 3000, 0),
}};

// ── Les évolués : un niveau chacun, que seul un coffre donne ──────────────

// Beast Strafing : Double Strafe, bien plus fort.
constexpr std::array<SkillLevel, 1> kBeastStrafingLevels{{
    {.damage = 96,
     .cooldown_ms = 800,
     .amount = 4,
     .reach_units = Cells(kArrowReachCells),
     .speed_units_per_s = Cells(20),
     .hit_radius_units = CellTenths(kArrowHitRadiusTenths)},
}};

// Sharp Shooting : un trait qui perce la file de part en part.
constexpr std::array<SkillLevel, 1> kSharpShootingLevels{{
    {.damage = 128,
     .cooldown_ms = 1300,
     .amount = 2,
     .interval_ms = 150,
     .reach_units = Cells(12),
     .speed_units_per_s = Cells(22),
     .hit_radius_units = CellTenths(8),
     .pierce = 3},
}};

// Falcon Assault : le faucon fond sur le plus fort, d'un seul coup.
constexpr std::array<SkillLevel, 1> kFalconAssaultLevels{{
    {.damage = 520, .cooldown_ms = 2000, .reach_units = Cells(10)},
}};

// True Sight(durée en ms, recharge en ms, dégâts en %) : nettement plus fort
// qu'Improve Concentration.
constexpr std::array<SkillLevel, 1> kTrueSightLevels{{
    Stance(8000, 15000, 50),
}};

// Claymore Trap : un piège de feu, à grande aire, qui repousse.
constexpr std::array<SkillLevel, 1> kClaymoreTrapLevels{{
    Trap(141, 1800, 30, 0, 20),
}};

// Blast Mine : un piège de vent, plus fort que Land Mine.
constexpr std::array<SkillLevel, 1> kBlastMineLevels{{
    Trap(205, 1400, 25, 300, 0),
}};

// Phantasmic Arrow : l'union de Charge Arrow et de Skid Trap, une flèche qui
// traverse la file et la repousse.
constexpr std::array<SkillLevel, 1> kPhantasmicArrowLevels{{
    {.damage = 192,
     .cooldown_ms = 1200,
     .reach_units = Cells(11),
     .speed_units_per_s = Cells(20),
     .hit_radius_units = CellTenths(7),
     .pierce = 3,
     .knockback_units = Cells(3)},
}};

constexpr std::array<SurvivorsSkillRule, kArcherPoolSkillCount + kArcherEvolvedSkillCount>
    kArcherSkills{{
        {.skill_id = kDoubleStrafeId,
         .key = "double_strafe",
         .description = core::TrText("Des flèches filent sur le monstre le plus proche."),
         .archetype = SkillArchetype::kLinear,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kDoubleStrafeLevels},
        {.skill_id = kArrowShowerId,
         .key = "arrow_shower",
         .description = core::TrText("Une pluie de flèches tombe là où la horde se presse."),
         .archetype = SkillArchetype::kClusterBlast,
         .target = TargetRule::kDensest,
         .element = SurvivorsElement::kNeutral,
         .levels = kArrowShowerLevels},
        {.skill_id = kChargeArrowId,
         .key = "charge_arrow",
         .description = core::TrText("Une flèche lourde renvoie au loin ce qui approche."),
         .archetype = SkillArchetype::kLinear,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kChargeArrowLevels},
        {.skill_id = kImproveConcentrationId,
         .key = "improve_concentration",
         .description = core::TrText("Par moments, vos tirs se pressent : vos skills reviennent "
                                     "plus vite."),
         .archetype = SkillArchetype::kStance,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kImproveConcentrationLevels,
         .stance_stat = SurvivorsStat::kCooldownPct},
        {.skill_id = kBlitzBeatId,
         .key = "blitz_beat",
         .description = core::TrText("Votre faucon pique, coup après coup, sur le plus proche."),
         .archetype = SkillArchetype::kClusterBlast,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kBlitzBeatLevels},
        {.skill_id = kAnkleSnareId,
         .key = "ankle_snare",
         .description = core::TrText("Un piège posé près de vous fige ce qui marche dessus."),
         .archetype = SkillArchetype::kRing,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kAnkleSnareLevels},
        {.skill_id = kLandMineId,
         .key = "land_mine",
         .description = core::TrText("Une mine posée près de vous saute sous la horde."),
         .archetype = SkillArchetype::kRing,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kEarth,
         .levels = kLandMineLevels},
        {.skill_id = kSkidTrapId,
         .key = "skid_trap",
         .description =
             core::TrText("Un piège posé près de vous renvoie la horde d'où elle vient."),
         .archetype = SkillArchetype::kRing,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kSkidTrapLevels},
        {.skill_id = kFreezingTrapId,
         .key = "freezing_trap",
         .description = core::TrText("Un piège posé près de vous gèle ce qui l'entoure."),
         .archetype = SkillArchetype::kRing,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kWater,
         .levels = kFreezingTrapLevels},
        {.skill_id = kBeastStrafingId,
         .key = "beast_strafing",
         .description = core::TrText("Des flèches puissantes filent sur le plus proche."),
         .archetype = SkillArchetype::kLinear,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kBeastStrafingLevels},
        {.skill_id = kSharpShootingId,
         .key = "sharp_shooting",
         .description = core::TrText("Un trait perce la file des monstres de part en part."),
         .archetype = SkillArchetype::kLinear,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kSharpShootingLevels},
        {.skill_id = kFalconAssaultId,
         .key = "falcon_assault",
         .description = core::TrText("Votre faucon fond sur le plus fort, d'un seul coup."),
         .archetype = SkillArchetype::kTargetStrike,
         .target = TargetRule::kStrongest,
         .element = SurvivorsElement::kNeutral,
         .levels = kFalconAssaultLevels},
        {.skill_id = kTrueSightId,
         .key = "true_sight",
         .description = core::TrText("Par moments, votre œil s'aiguise : vos dégâts explosent."),
         .archetype = SkillArchetype::kStance,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kTrueSightLevels,
         .stance_stat = SurvivorsStat::kDamagePct},
        {.skill_id = kClaymoreTrapId,
         .key = "claymore_trap",
         .description = core::TrText("Un piège de feu, posé près de vous, souffle la horde."),
         .archetype = SkillArchetype::kRing,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kFire,
         .levels = kClaymoreTrapLevels},
        {.skill_id = kBlastMineId,
         .key = "blast_mine",
         .description = core::TrText("Une mine de vent, posée près de vous, éclate sous la horde."),
         .archetype = SkillArchetype::kRing,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kWind,
         .levels = kBlastMineLevels},
        {.skill_id = kPhantasmicArrowId,
         .key = "phantasmic_arrow",
         .description = core::TrText("Une flèche spectrale traverse la file et la repousse."),
         .archetype = SkillArchetype::kLinear,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kPhantasmicArrowLevels},
    }};

constexpr std::array<EvolutionRule, kArcherEvolvedSkillCount> kArcherEvolutions{{
    {.skill_id = kDoubleStrafeId,
     .card_item_id = kArcherSkeletonCardId,
     .evolved_skill_id = kBeastStrafingId},
    {.skill_id = kArrowShowerId, .card_item_id = kYoyoCardId, .evolved_skill_id = kSharpShootingId},
    {.skill_id = kBlitzBeatId,
     .card_item_id = kHunterFlyCardId,
     .evolved_skill_id = kFalconAssaultId},
    {.skill_id = kImproveConcentrationId,
     .card_item_id = kRockerCardId,
     .evolved_skill_id = kTrueSightId},
    {.skill_id = kAnkleSnareId, .card_item_id = kWillowCardId, .evolved_skill_id = kClaymoreTrapId},
    {.skill_id = kLandMineId, .card_item_id = kPhreeoniCardId, .evolved_skill_id = kBlastMineId},
    // L'UNION : Charge Arrow et Skid Trap, les deux reculs, au plus haut niveau.
    {.skill_id = kChargeArrowId,
     .card_item_id = 0,
     .evolved_skill_id = kPhantasmicArrowId,
     .partner_skill_id = kSkidTrapId},
}};

constexpr std::array<std::uint16_t, 3> kArcherStarterSkills{kDoubleStrafeId, kArrowShowerId,
                                                            kChargeArrowId};
constexpr std::array<std::uint16_t, kArcherPoolSkillCount> kArcherSkillPool{
    kDoubleStrafeId, kArrowShowerId, kChargeArrowId, kImproveConcentrationId, kBlitzBeatId,
    kAnkleSnareId,   kLandMineId,    kSkidTrapId,    kFreezingTrapId};

// Archer Skeleton : ses coups à distance portent plus.
constexpr std::array<std::int32_t, kMostCardLevels> kArcherSkeletonLevels{8, 16, 24, 32, 40};
// Hunter Fly : une part de ses coups lui revient en PV.
constexpr std::array<std::int32_t, kMostCardLevels> kHunterFlyLevels{3, 6, 9, 12, 15};
// Phreeoni : abattu, il se relève.
constexpr std::array<std::int32_t, kMostCardLevels> kPhreeoniLevels{1, 1, 1, 1, 2};
// Willow : le Black Mushroom pousse, de plus en plus souvent.
constexpr std::array<std::int32_t, kMostCardLevels> kWillowLevels{100, 150, 200, 250, 300};
// Rocker : ses skills reviennent plus vite, comme Eclipse.
constexpr std::array<std::int32_t, kMostCardLevels> kRockerLevels{4, 8, 12, 16, 20};
// Yoyo : il marche plus vite, comme Chonchon.
constexpr std::array<std::int32_t, kMostCardLevels> kYoyoLevels{8, 16, 24, 32, 40};
constexpr std::array<CardRule, kArcherCardCount> kArcherCards{{
    {.card_item_id = kArcherSkeletonCardId,
     .key = "archer_skeleton_card",
     .description = core::TrText("Dégâts à distance : vos tirs et vos éclats portent plus."),
     .stat = SurvivorsStat::kRangedDamagePct,
     .per_level = kArcherSkeletonLevels},
    {.card_item_id = kHunterFlyCardId,
     .key = "hunter_fly_card",
     .description = core::TrText("Vol de vie : une part de vos dégâts vous revient en PV."),
     .stat = SurvivorsStat::kLifeStealPct,
     .per_level = kHunterFlyLevels},
    {.card_item_id = kPhreeoniCardId,
     .key = "phreeoni_card",
     .description = core::TrText("Une vie de plus : abattu, vous vous relevez."),
     .stat = SurvivorsStat::kExtraLives,
     .per_level = kPhreeoniLevels},
    {.card_item_id = kWillowCardId,
     .key = "willow_card",
     .description = core::TrText("Le Black Mushroom pousse : sa Yggdrasil Berry rend tous les PV."),
     .stat = SurvivorsStat::kSproutPct,
     .per_level = kWillowLevels},
    {.card_item_id = kRockerCardId,
     .key = "rocker_card",
     .description = core::TrText("Recharge réduite : vos skills reviennent plus vite."),
     .stat = SurvivorsStat::kCooldownPct,
     .per_level = kRockerLevels},
    {.card_item_id = kYoyoCardId,
     .key = "yoyo_card",
     .description = core::TrText("Vitesse de marche augmentée."),
     .stat = SurvivorsStat::kWalkPct,
     .per_level = kYoyoLevels},
}};

// Les cartes du Magicien qui n'ouvrent rien chez lui, puis les siennes, qu'il
// prête à ses coéquipiers.
constexpr std::array<std::uint16_t, 16> kArcherCardPool{
    kArcherSkeletonCardId, kPupaCardId,     kYoyoCardId,        kPoringCardId,
    kHunterFlyCardId,      kThiefBugCardId, kGolemCardId,       kMasteringCardId,
    kRockerCardId,         kPhreeoniCardId, kWillowCardId,      kBaphometCardId,
    kSmokieCardId,         kMantisCardId,   kOrcSkeletonCardId, kNightmareCardId};
constexpr std::array<std::uint16_t, kArcherCardCount> kArcherLentCards{
    kArcherSkeletonCardId, kHunterFlyCardId, kPhreeoniCardId,
    kWillowCardId,         kRockerCardId,    kYoyoCardId};

constexpr ClassRule kArcher{.class_id = kArcherClassId,
                            .key = "archer",
                            .job_id = kArcherJobId,
                            .weapon_item_id = kBowItemId,
                            .max_hp = kArcherMaxHp,
                            .walk_ms_per_cell = kArcherWalkMsPerCell,
                            .magnet_units = CellTenths(kArcherMagnetTenths),
                            .damage_pct_per_level = kArcherDamagePctPerLevel,
                            .damage_pct_cap = kArcherDamagePctCap,
                            .weapon_skill_id = kBowShotSkillId,
                            .starter_skills = kArcherStarterSkills,
                            .skill_pool = kArcherSkillPool,
                            .card_pool = kArcherCardPool,
                            .evolved_job_id = kHunterJobId,
                            .evolved_weapon_item_id = kHunterBowItemId,
                            .attack_gesture = true,
                            .reborn_level = kArcherRebornLevel,
                            .reborn_job_id = kSniperJobId,
                            .reborn_damage_pct = kArcherRebornDamagePct,
                            .final_boss_hp_bonus_pct = kArcherFinalBossHpBonusPct,
                            .lent_cards = kArcherLentCards};

}  // namespace

const ClassRule& ArcherClass() { return kArcher; }

std::span<const SurvivorsSkillRule> ArcherSkills() { return kArcherSkills; }

std::span<const CardRule> ArcherCards() { return kArcherCards; }

std::span<const EvolutionRule> ArcherEvolutions() { return kArcherEvolutions; }

}  // namespace ro::sim::survivors

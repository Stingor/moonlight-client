#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "ro/core/i18n.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "survivors_shipped.h"

namespace ro::sim::survivors {

namespace {

// ── LE MAGE : sa classe, ses huit skills et ses huit cartes ─────────────────
// Les Id sont ceux du jeu : compétence RO pour un skill, objet RO pour une
// carte, métier RO pour l'apparence.

constexpr std::uint16_t kMageClassId = 1;
constexpr std::uint16_t kMageJobId = 2;
constexpr std::uint16_t kWizardJobId = 9;         // l'apparence après une évolution
constexpr std::uint16_t kHighWizardJobId = 4010;  // rené
constexpr std::uint16_t kMageRebornLevel = 50;
constexpr std::int32_t kMageRebornDamagePct = 5;
constexpr std::uint16_t kMageWeaponItemId = 1601;  // Rod
constexpr std::int32_t kMageMaxHp = 340;
constexpr std::uint32_t kMageWalkMsPerCell = 150;
constexpr std::int32_t kMageMagnetCells = 3;
constexpr std::int32_t kMageDamagePctPerLevel = 1;
constexpr std::int32_t kMageDamagePctCap = 25;

// Les trois éclairs frappent à la même portée, une frappe toutes les 120 ms.
constexpr std::int32_t kBoltReachCells = 9;
constexpr std::uint32_t kBoltIntervalMs = 120;

// Lightning Bolt rebondit sur le plus proche non touché, à 60 % de la frappe.
constexpr std::int32_t kBounceReachCells = 3;
constexpr std::int32_t kBouncePct = 60;

// Soul Strike : des esprits qui poursuivent, à 9 cases par seconde.
constexpr std::int32_t kSoulReachCells = 10;
constexpr std::int32_t kSoulSpeedCellsPerSecond = 9;
constexpr std::int32_t kSoulHitRadiusTenths = 4;
constexpr std::uint32_t kSoulLifeMs = 2000;

// Sight : des orbes qui font un tour en 1,6 s.
constexpr std::uint32_t kSightPeriodMs = 1600;
constexpr std::int32_t kSightHitRadiusTenths = 6;
constexpr std::uint32_t kSightRehitMs = 500;
constexpr std::int32_t kSightNearOrbitTenths = 20;
constexpr std::int32_t kSightWideOrbitTenths = 25;
constexpr std::uint32_t kSightShortActiveMs = 3000;
constexpr std::uint32_t kSightLongActiveMs = 4000;
constexpr std::uint32_t kSightRestMs = 3000;
constexpr std::uint32_t kSightPermanentRestMs = 0;

// Napalm Beat : des éclats sur le plus dense, 60 % autour du centre.
constexpr std::int32_t kNapalmReachCells = 8;
constexpr std::uint32_t kNapalmIntervalMs = 150;
constexpr std::int32_t kNapalmSplashPct = 60;

// Fire Wall : un mur à deux cases devant le héros, qui repousse de deux cases.
constexpr std::int32_t kWallAheadCells = 2;
constexpr std::int32_t kWallKnockbackCells = 2;
constexpr std::uint32_t kWallRehitMs = 400;
constexpr std::uint32_t kWallLifeMs = 4000;

// Frost Diver : un projectile droit à 12 cases par seconde, qui gèle.
constexpr std::int32_t kDiverReachCells = 9;
constexpr std::int32_t kDiverSpeedCellsPerSecond = 12;
constexpr std::int32_t kDiverHitRadiusTenths = 4;

// Thunderstorm : un orage posé sur le monstre le plus proche, à 9 cases, qui
// frappe tout son disque toutes les 300 ms.
constexpr std::int32_t kThunderstormReachCells = 9;
constexpr std::uint32_t kThunderstormWaveMs = 300;

// Fire Ball : un cône de feu devant le héros, qui écarte d'une demi-case.
constexpr std::int32_t kFireBallKnockbackTenths = 5;

// Ice Wall : des blocs d'une demi-case de rayon, posés à trois cases du héros
// face à la foule la plus dense qu'il cherche à dix cases, et qui tiennent
// six secondes.
constexpr std::int32_t kIceWallSearchCells = 10;
constexpr std::int32_t kIceWallDensityCells = 2;
constexpr std::int32_t kIceWallAheadCells = 3;
constexpr std::int32_t kIceWallBlockTenths = 5;
constexpr std::uint32_t kIceWallLifeMs = 6000;

// Un niveau d'éclair : `amount` frappes de `damage`, une salve par
// `cooldown_ms`, et un glaçage de `chill_ms` (0 : aucun).
[[nodiscard]] constexpr SkillLevel Bolt(std::uint8_t amount, std::int32_t damage,
                                        std::uint32_t cooldown_ms,
                                        std::uint32_t chill_ms = 0) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .interval_ms = kBoltIntervalMs,
                    .reach_units = Cells(kBoltReachCells),
                    .chill_ms = chill_ms};
}

// Un niveau d'éclair qui rebondit `bounces` fois.
[[nodiscard]] constexpr SkillLevel BouncingBolt(std::uint8_t amount, std::int32_t damage,
                                                std::uint32_t cooldown_ms,
                                                std::uint8_t bounces) noexcept {
  SkillLevel level = Bolt(amount, damage, cooldown_ms);
  level.bounces = bounces;
  level.bounce_pct = kBouncePct;
  level.bounce_reach_units = Cells(kBounceReachCells);
  return level;
}

// Un niveau de Soul Strike : `amount` esprits de `damage`, qui traversent
// `pierce` monstres de plus que le premier.
[[nodiscard]] constexpr SkillLevel Soul(std::uint8_t amount, std::int32_t damage,
                                        std::uint32_t cooldown_ms, std::uint8_t pierce) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .reach_units = Cells(kSoulReachCells),
                    .speed_units_per_s = Cells(kSoulSpeedCellsPerSecond),
                    .hit_radius_units = CellTenths(kSoulHitRadiusTenths),
                    .life_ms = kSoulLifeMs,
                    .pierce = pierce};
}

// Un niveau de Sight : `amount` orbes de `damage` sur une orbite de
// `orbit_tenths` dixièmes de case, allumées `active_ms` puis éteintes
// `rest_ms` (0 : allumées en permanence).
[[nodiscard]] constexpr SkillLevel Orbit(std::uint8_t amount, std::int32_t damage,
                                         std::int32_t orbit_tenths, std::uint32_t active_ms,
                                         std::uint32_t rest_ms) noexcept {
  return SkillLevel{.damage = damage,
                    .amount = amount,
                    .reach_units = CellTenths(orbit_tenths),
                    .hit_radius_units = CellTenths(kSightHitRadiusTenths),
                    .life_ms = active_ms,
                    .rest_ms = rest_ms,
                    .period_ms = kSightPeriodMs,
                    .rehit_ms = kSightRehitMs};
}

// Un niveau de Napalm Beat : `amount` éclats de `damage` au centre, de rayon
// `radius_tenths` dixièmes de case.
[[nodiscard]] constexpr SkillLevel Blast(std::uint8_t amount, std::int32_t damage,
                                         std::uint32_t cooldown_ms,
                                         std::int32_t radius_tenths) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .interval_ms = kNapalmIntervalMs,
                    .reach_units = Cells(kNapalmReachCells),
                    .area_radius_units = CellTenths(radius_tenths),
                    .splash_pct = kNapalmSplashPct};
}

// Un niveau de Fire Wall : `amount` murs de `length_cells` cases, qui frappent
// chacun `hits` fois `damage`.
[[nodiscard]] constexpr SkillLevel Wall(std::uint8_t amount, std::int32_t damage, std::uint8_t hits,
                                        std::int32_t length_cells,
                                        std::uint32_t cooldown_ms) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .reach_units = Cells(kWallAheadCells),
                    .life_ms = kWallLifeMs,
                    .rehit_ms = kWallRehitMs,
                    .length_units = Cells(length_cells),
                    .zone_hits = hits,
                    .knockback_units = Cells(kWallKnockbackCells)};
}

// Un niveau de Frost Diver : `amount` traits de `damage`, qui gèlent
// `freeze_ms` et traversent `pierce` monstres de plus que le premier.
[[nodiscard]] constexpr SkillLevel Diver(std::uint8_t amount, std::int32_t damage,
                                         std::uint32_t cooldown_ms, std::uint32_t freeze_ms,
                                         std::uint8_t pierce) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .reach_units = Cells(kDiverReachCells),
                    .speed_units_per_s = Cells(kDiverSpeedCellsPerSecond),
                    .hit_radius_units = CellTenths(kDiverHitRadiusTenths),
                    .pierce = pierce,
                    .freeze_ms = freeze_ms};
}

// Un niveau de Thunderstorm : `waves` vagues de `damage`, sur un disque de
// `radius_tenths` dixièmes de case.
[[nodiscard]] constexpr SkillLevel Storm(std::uint8_t waves, std::int32_t damage,
                                         std::uint32_t cooldown_ms,
                                         std::int32_t radius_tenths) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = waves,
                    .interval_ms = kThunderstormWaveMs,
                    .reach_units = Cells(kThunderstormReachCells),
                    .area_radius_units = CellTenths(radius_tenths)};
}

// Un niveau de Fire Ball : un cône de `damage`, long de `length_tenths`
// dixièmes de case et ouvert de `half_steps` caps de chaque côté.
[[nodiscard]] constexpr SkillLevel Cone(std::int32_t damage, std::uint32_t cooldown_ms,
                                        std::int32_t length_tenths,
                                        std::uint8_t half_steps) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .reach_units = CellTenths(length_tenths),
                    .knockback_units = CellTenths(kFireBallKnockbackTenths),
                    .cone_half_steps = half_steps};
}

// Un niveau d'Ice Wall : `blocks` blocs de `block_hp` PV, qui gèlent
// `freeze_ms` le monstre qui les frappe.
[[nodiscard]] constexpr SkillLevel IceWall(std::uint8_t blocks, std::int32_t block_hp,
                                           std::uint32_t freeze_ms,
                                           std::uint32_t cooldown_ms) noexcept {
  return SkillLevel{.cooldown_ms = cooldown_ms,
                    .amount = blocks,
                    .reach_units = Cells(kIceWallSearchCells),
                    .hit_radius_units = CellTenths(kIceWallBlockTenths),
                    .life_ms = kIceWallLifeMs,
                    .area_radius_units = Cells(kIceWallDensityCells),
                    .length_units = Cells(kIceWallAheadCells),
                    .freeze_ms = freeze_ms,
                    .block_hp = block_hp};
}

// Bolt(frappes, dégâts, recharge en ms).
constexpr std::array<SkillLevel, kMostSkillLevels> kFireBoltLevels{{
    Bolt(2, 12, 1100),
    Bolt(3, 12, 1100),
    Bolt(3, 14, 1100),
    Bolt(4, 14, 1100),
    Bolt(4, 14, 950),
    Bolt(5, 14, 950),
    Bolt(5, 20, 950),
    Bolt(7, 20, 950),
}};

// Bolt(frappes, dégâts, recharge en ms, glaçage en ms).
constexpr std::array<SkillLevel, kMostSkillLevels> kColdBoltLevels{{
    Bolt(2, 11, 1300, 1500),
    Bolt(3, 11, 1300, 1500),
    Bolt(3, 12, 1300, 1500),
    Bolt(3, 12, 1300, 2500),
    Bolt(4, 12, 1300, 2500),
    Bolt(4, 12, 1100, 2500),
    Bolt(5, 12, 1100, 2500),
    Bolt(5, 16, 1100, 3000),
}};

// Bolt(frappes, dégâts, recharge en ms), puis BouncingBolt(…, rebonds).
constexpr std::array<SkillLevel, kMostSkillLevels> kLightningBoltLevels{{
    Bolt(2, 16, 1500),
    Bolt(3, 16, 1500),
    Bolt(3, 19, 1500),
    BouncingBolt(4, 19, 1500, 1),
    BouncingBolt(4, 19, 1250, 1),
    BouncingBolt(4, 24, 1250, 1),
    BouncingBolt(5, 24, 1250, 1),
    BouncingBolt(7, 24, 1250, 2),
}};

// Soul(esprits, dégâts, recharge en ms, perçage).
constexpr std::array<SkillLevel, kMostSkillLevels> kSoulStrikeLevels{{
    Soul(2, 8, 1000, 0),
    Soul(3, 8, 1000, 0),
    Soul(3, 11, 1000, 0),
    Soul(3, 11, 1000, 1),
    Soul(4, 11, 1000, 1),
    Soul(4, 11, 850, 1),
    Soul(6, 11, 850, 1),
    Soul(6, 16, 850, 2),
}};

// Orbit(orbes, dégâts, rayon d'orbite en dixièmes de case, allumée, éteinte).
constexpr std::array<SkillLevel, kMostSkillLevels> kSightLevels{{
    Orbit(1, 10, kSightNearOrbitTenths, kSightShortActiveMs, kSightRestMs),
    Orbit(2, 10, kSightNearOrbitTenths, kSightShortActiveMs, kSightRestMs),
    Orbit(2, 14, kSightNearOrbitTenths, kSightShortActiveMs, kSightRestMs),
    Orbit(2, 14, kSightWideOrbitTenths, kSightShortActiveMs, kSightRestMs),
    Orbit(3, 14, kSightWideOrbitTenths, kSightLongActiveMs, kSightRestMs),
    Orbit(3, 20, kSightWideOrbitTenths, kSightLongActiveMs, kSightRestMs),
    Orbit(4, 20, kSightWideOrbitTenths, kSightLongActiveMs, kSightRestMs),
    Orbit(4, 20, kSightWideOrbitTenths, kSightLongActiveMs, kSightPermanentRestMs),
}};

// Blast(éclats, dégâts, recharge en ms, rayon d'éclat en dixièmes de case).
constexpr std::array<SkillLevel, kMostSkillLevels> kNapalmBeatLevels{{
    Blast(1, 24, 1400, 15),
    Blast(1, 30, 1400, 15),
    Blast(2, 30, 1400, 15),
    Blast(2, 30, 1200, 18),
    Blast(2, 38, 1200, 18),
    Blast(3, 38, 1200, 18),
    Blast(3, 38, 1000, 20),
    Blast(3, 48, 1000, 22),
}};

// Wall(murs, dégâts, coups, longueur en cases, recharge en ms).
constexpr std::array<SkillLevel, kMostSkillLevels> kFireWallLevels{{
    Wall(1, 12, 5, 3, 4500),
    Wall(1, 12, 7, 3, 4500),
    Wall(1, 15, 7, 4, 4500),
    Wall(2, 15, 7, 4, 4000),
    Wall(2, 15, 9, 5, 4000),
    Wall(2, 19, 9, 5, 4000),
    Wall(3, 19, 9, 5, 3500),
    Wall(4, 24, 10, 5, 3500),
}};

// Diver(tirs, dégâts, recharge en ms, gel en ms, perçage).
constexpr std::array<SkillLevel, kMostSkillLevels> kFrostDiverLevels{{
    Diver(1, 18, 1600, 1500, 0),
    Diver(1, 24, 1600, 1500, 0),
    Diver(2, 24, 1600, 1500, 0),
    Diver(2, 24, 1600, 1500, 1),
    Diver(2, 30, 1600, 2000, 1),
    Diver(3, 30, 1400, 2000, 1),
    Diver(3, 36, 1400, 2000, 2),
    Diver(4, 40, 1400, 2500, 3),
}};
// Bolt(frappes, dégâts, recharge en ms) : les pointes d'Earth Spike.
constexpr std::array<SkillLevel, kMostSkillLevels> kEarthSpikeLevels{{
    Bolt(2, 22, 1400),
    Bolt(3, 22, 1400),
    Bolt(3, 27, 1400),
    Bolt(4, 27, 1400),
    Bolt(4, 27, 1200),
    Bolt(5, 31, 1200),
    Bolt(6, 31, 1200),
    Bolt(8, 36, 1200),
}};
// Storm(vagues, dégâts, recharge en ms, rayon en dixièmes de case).
constexpr std::array<SkillLevel, kMostSkillLevels> kThunderstormLevels{{
    Storm(4, 18, 3200, 20),
    Storm(4, 22, 3200, 20),
    Storm(5, 22, 3200, 20),
    Storm(5, 22, 2800, 22),
    Storm(5, 28, 2800, 22),
    Storm(6, 28, 2800, 25),
    Storm(6, 28, 2400, 25),
    Storm(7, 36, 2400, 25),
}};

// Cone(dégâts, recharge en ms, longueur en dixièmes de case, demi-ouverture
// en caps).
constexpr std::array<SkillLevel, kMostSkillLevels> kFireBallLevels{{
    Cone(33, 1500, 35, 5),
    Cone(39, 1500, 35, 5),
    Cone(39, 1400, 40, 6),
    Cone(47, 1400, 40, 6),
    Cone(47, 1300, 45, 7),
    Cone(56, 1300, 45, 7),
    Cone(56, 1200, 50, 8),
    Cone(67, 1100, 50, 8),
}};
// Bolt(frappes, dégâts, recharge en ms) : la rafale de Water Ball.
constexpr std::array<SkillLevel, kMostSkillLevels> kWaterBallLevels{{
    Bolt(3, 28, 2600),
    Bolt(3, 33, 2600),
    Bolt(4, 33, 2600),
    Bolt(4, 39, 2400),
    Bolt(5, 39, 2400),
    Bolt(5, 44, 2200),
    Bolt(6, 44, 2200),
    Bolt(7, 50, 2000),
}};
// IceWall(blocs, PV d'un bloc, gel en ms, recharge en ms).
constexpr std::array<SkillLevel, kMostSkillLevels> kIceWallLevels{{
    IceWall(5, 80, 800, 9000),
    IceWall(5, 100, 800, 9000),
    IceWall(6, 100, 1000, 8500),
    IceWall(6, 130, 1000, 8500),
    IceWall(7, 130, 1200, 8000),
    IceWall(7, 160, 1200, 7500),
    IceWall(8, 160, 1500, 7500),
    IceWall(9, 200, 1500, 7000),
}};

constexpr std::array<SurvivorsSkillRule, 13> kPoolSkills{{
    {.skill_id = kFireBoltId,
     .key = "fire_bolt",
     .description = core::TrText("Des éclairs de feu frappent le monstre le plus proche."),
     .archetype = SkillArchetype::kTargetStrike,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kFire,
     .levels = kFireBoltLevels},
    {.skill_id = kColdBoltId,
     .key = "cold_bolt",
     .description = core::TrText("Des éclairs de glace frappent au hasard et ralentissent."),
     .archetype = SkillArchetype::kTargetStrike,
     .target = TargetRule::kRandom,
     .element = SurvivorsElement::kWater,
     .levels = kColdBoltLevels},
    {.skill_id = kLightningBoltId,
     .key = "lightning_bolt",
     .description = core::TrText("La foudre frappe le monstre le plus robuste."),
     .archetype = SkillArchetype::kTargetStrike,
     .target = TargetRule::kStrongest,
     .element = SurvivorsElement::kWind,
     .levels = kLightningBoltLevels},
    {.skill_id = kSoulStrikeId,
     .key = "soul_strike",
     .description = core::TrText("Des esprits poursuivent les monstres et les transpercent."),
     .archetype = SkillArchetype::kHoming,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kGhost,
     .levels = kSoulStrikeLevels},
    {.skill_id = kSightId,
     .key = "sight",
     .description = core::TrText("Des boules de feu tournent autour de vous."),
     .archetype = SkillArchetype::kOrbit,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kFire,
     .levels = kSightLevels},
    {.skill_id = kNapalmBeatId,
     .key = "napalm_beat",
     .description = core::TrText("Des éclats d'esprit frappent là où les monstres se pressent."),
     .archetype = SkillArchetype::kClusterBlast,
     .target = TargetRule::kDensest,
     .element = SurvivorsElement::kGhost,
     .levels = kNapalmBeatLevels},
    {.skill_id = kFireWallId,
     .key = "fire_wall",
     .description = core::TrText("Un mur de feu devant vous brûle et repousse les monstres."),
     .archetype = SkillArchetype::kWall,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kFire,
     .levels = kFireWallLevels},
    {.skill_id = kFrostDiverId,
     .key = "frost_diver",
     .description = core::TrText("Un trait de glace file droit, transperce et gèle."),
     .archetype = SkillArchetype::kLinear,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kWater,
     .levels = kFrostDiverLevels},
    {.skill_id = kEarthSpikeId,
     .key = "earth_spike",
     .description = core::TrText("Des pointes de terre jaillissent sous des monstres au hasard."),
     .archetype = SkillArchetype::kTargetStrike,
     .target = TargetRule::kRandom,
     .element = SurvivorsElement::kEarth,
     .levels = kEarthSpikeLevels},
    {.skill_id = kThunderstormId,
     .key = "thunderstorm",
     .description =
         core::TrText("Un orage s'abat sur le monstre le plus proche et frappe en rafale."),
     .archetype = SkillArchetype::kStorm,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kWind,
     .levels = kThunderstormLevels},
    {.skill_id = kFireBallId,
     .key = "fire_ball",
     .description = core::TrText("Un cône de feu jaillit devant vous, là où vous marchez."),
     .archetype = SkillArchetype::kCone,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kFire,
     .levels = kFireBallLevels},
    {.skill_id = kWaterBallId,
     .key = "water_ball",
     .description =
         core::TrText("Une rafale de boules d'eau s'abat sur le monstre le plus robuste."),
     .archetype = SkillArchetype::kTargetStrike,
     .target = TargetRule::kStrongest,
     .element = SurvivorsElement::kWater,
     .levels = kWaterBallLevels},
    {.skill_id = kIceWallId,
     .key = "ice_wall",
     .description =
         core::TrText("Des blocs de glace barrent la route à la horde ; qui les frappe gèle."),
     .archetype = SkillArchetype::kBarrier,
     .target = TargetRule::kDensest,
     .element = SurvivorsElement::kWater,
     .levels = kIceWallLevels},
}};

// Le gain de chaque carte, du niveau 1 au niveau 5.
constexpr std::array<std::int32_t, kMostCardLevels> kHornetLevels{6, 12, 18, 24, 30};
constexpr std::array<std::int32_t, kMostCardLevels> kPupaLevels{20, 40, 60, 80, 100};
constexpr std::array<std::int32_t, kMostCardLevels> kChonchonLevels{8, 16, 24, 32, 40};
constexpr std::array<std::int32_t, kMostCardLevels> kPoringLevels{10, 20, 30, 40, 50};
constexpr std::array<std::int32_t, kMostCardLevels> kSporeLevels{18, 36, 54, 72, 90};
constexpr std::array<std::int32_t, kMostCardLevels> kThiefBugLevels{40, 80, 120, 160, 200};
constexpr std::array<std::int32_t, kMostCardLevels> kGolemLevels{1, 2, 3, 4, 5};
// Mastering Card n'a que deux niveaux : un projectile de plus par niveau.
constexpr std::array<std::int32_t, 2> kMasteringLevels{1, 2};
// Les cartes des mini-boss et de Baphomet : recharge, aire, durée, expérience.
constexpr std::array<std::int32_t, kMostCardLevels> kEclipseLevels{4, 8, 12, 16, 20};
constexpr std::array<std::int32_t, kMostCardLevels> kDragonFlyLevels{8, 16, 24, 32, 40};
constexpr std::array<std::int32_t, kMostCardLevels> kVagabondWolfLevels{10, 20, 30, 40, 50};
constexpr std::array<std::int32_t, kMostCardLevels> kBaphometLevels{8, 16, 24, 32, 40};

constexpr std::array<CardRule, 12> kCards{{
    {.card_item_id = kHornetCardId,
     .key = "hornet_card",
     .description = core::TrText("Dégâts de tous les skills augmentés."),
     .stat = SurvivorsStat::kDamagePct,
     .per_level = kHornetLevels},
    {.card_item_id = kPupaCardId,
     .key = "pupa_card",
     .description = core::TrText("PV maximum augmentés."),
     .stat = SurvivorsStat::kMaxHp,
     .per_level = kPupaLevels},
    {.card_item_id = kChonchonCardId,
     .key = "chonchon_card",
     .description = core::TrText("Vitesse de marche augmentée."),
     .stat = SurvivorsStat::kWalkPct,
     .per_level = kChonchonLevels},
    {.card_item_id = kPoringCardId,
     .key = "poring_card",
     .description = core::TrText("Chance d'une quatrième offre à la montée de niveau."),
     .stat = SurvivorsStat::kLuckPct,
     .per_level = kPoringLevels},
    {.card_item_id = kSporeCardId,
     .key = "spore_card",
     .description = core::TrText("Récupération de PV au fil du temps."),
     .stat = SurvivorsStat::kRegenPerMinute,
     .per_level = kSporeLevels},
    {.card_item_id = kThiefBugCardId,
     .key = "thief_bug_card",
     .description = core::TrText("Portée de ramassage des gemmes augmentée."),
     .stat = SurvivorsStat::kMagnetPct,
     .per_level = kThiefBugLevels},
    {.card_item_id = kGolemCardId,
     .key = "golem_card",
     .description = core::TrText("Armure : chaque coup reçu est réduit."),
     .stat = SurvivorsStat::kArmor,
     .per_level = kGolemLevels},
    {.card_item_id = kMasteringCardId,
     .key = "mastering_card",
     .description = core::TrText("Un projectile de plus pour chaque skill."),
     .stat = SurvivorsStat::kAmount,
     .per_level = kMasteringLevels},
    {.card_item_id = kEclipseCardId,
     .key = "eclipse_card",
     .description = core::TrText("Recharge de tous les skills raccourcie."),
     .stat = SurvivorsStat::kCooldownPct,
     .per_level = kEclipseLevels},
    {.card_item_id = kDragonFlyCardId,
     .key = "dragon_fly_card",
     .description = core::TrText("Aire de tous les skills élargie."),
     .stat = SurvivorsStat::kAreaPct,
     .per_level = kDragonFlyLevels},
    {.card_item_id = kVagabondWolfCardId,
     .key = "vagabond_wolf_card",
     .description = core::TrText("Durée des zones, des gels et des statuts allongée."),
     .stat = SurvivorsStat::kDurationPct,
     .per_level = kVagabondWolfLevels},
    {.card_item_id = kBaphometCardId,
     .key = "baphomet_card",
     .description = core::TrText("Expérience gagnée augmentée."),
     .stat = SurvivorsStat::kExpPct,
     .per_level = kBaphometLevels},
}};

constexpr std::array<std::uint16_t, 3> kMageStarterSkills{kFireBoltId, kColdBoltId,
                                                          kLightningBoltId};
constexpr std::array<std::uint16_t, 13> kMageSkillPool{
    kFireBoltId,   kColdBoltId,  kLightningBoltId, kSoulStrikeId, kSightId,
    kNapalmBeatId, kFireWallId,  kFrostDiverId,    kEarthSpikeId, kThunderstormId,
    kFireBallId,   kWaterBallId, kIceWallId};
// Les douze cartes, celles des boss comprises : la réserve livrée est
// entière, celle du départ neutre ; une partie libre en retire par ses Id
// verrouillés ce que le carnet n'a pas débloqué.
constexpr std::array<std::uint16_t, 12> kMageCardPool{
    kHornetCardId,  kPupaCardId,      kChonchonCardId,     kPoringCardId,
    kSporeCardId,   kThiefBugCardId,  kGolemCardId,        kMasteringCardId,
    kEclipseCardId, kDragonFlyCardId, kVagabondWolfCardId, kBaphometCardId};

constexpr ClassRule kMage{.class_id = kMageClassId,
                          .key = "mage",
                          .job_id = kMageJobId,
                          .weapon_item_id = kMageWeaponItemId,
                          .max_hp = kMageMaxHp,
                          .walk_ms_per_cell = kMageWalkMsPerCell,
                          .magnet_units = Cells(kMageMagnetCells),
                          .damage_pct_per_level = kMageDamagePctPerLevel,
                          .damage_pct_cap = kMageDamagePctCap,
                          .starter_skills = kMageStarterSkills,
                          .skill_pool = kMageSkillPool,
                          .card_pool = kMageCardPool,
                          .evolved_job_id = kWizardJobId,
                          .reborn_level = kMageRebornLevel,
                          .reborn_job_id = kHighWizardJobId,
                          .reborn_damage_pct = kMageRebornDamagePct};

constexpr std::size_t kKnightSkillCount = kKnightPoolSkillCount + kKnightEvolvedSkillCount;
constexpr std::size_t kThiefSkillCount = kThiefPoolSkillCount + kThiefEvolvedSkillCount;
constexpr std::size_t kSkillCount =
    kPoolSkills.size() + kShippedEvolvedSkillCount + kKnightSkillCount + kThiefSkillCount;

}  // namespace

std::span<const ClassRule> ShippedClasses() {
  // Le Magicien en tête : la partie classée et le défi prennent la première
  // classe.
  static const std::array<ClassRule, 3> kClasses{kMage, KnightClass(), ThiefClass()};
  return kClasses;
}

std::span<const SurvivorsSkillRule> ShippedSkills() {
  // Une table IMMUABLE, assemblée au premier appel : la réserve du Magicien,
  // ses évolués, puis les skills du Chevalier et ceux du Voleur, dans cet ordre.
  static const std::array<SurvivorsSkillRule, kSkillCount> kSkills = [] {
    std::array<SurvivorsSkillRule, kSkillCount> skills{};
    const std::span<const SurvivorsSkillRule> evolved = ShippedEvolvedSkills();
    const std::span<const SurvivorsSkillRule> knight = KnightSkills();
    const std::span<const SurvivorsSkillRule> thief = ThiefSkills();
    auto out = std::copy(kPoolSkills.begin(), kPoolSkills.end(), skills.begin());
    out = std::copy_n(evolved.begin(), std::min(evolved.size(), kShippedEvolvedSkillCount), out);
    out = std::copy_n(knight.begin(), std::min(knight.size(), kKnightSkillCount), out);
    std::copy_n(thief.begin(), std::min(thief.size(), kThiefSkillCount), out);
    return skills;
  }();
  return kSkills;
}

std::span<const CardRule> ShippedCards() {
  // Une table IMMUABLE, assemblée au premier appel : les cartes du Magicien,
  // puis celles du Chevalier et celles du Voleur.
  static const std::array<CardRule, kCards.size() + kKnightCardCount + kThiefCardCount> kAllCards =
      [] {
        std::array<CardRule, kCards.size() + kKnightCardCount + kThiefCardCount> cards{};
        const std::span<const CardRule> knight = KnightCards();
        const std::span<const CardRule> thief = ThiefCards();
        auto out = std::copy(kCards.begin(), kCards.end(), cards.begin());
        out = std::copy_n(knight.begin(), std::min(knight.size(), kKnightCardCount), out);
        std::copy_n(thief.begin(), std::min(thief.size(), kThiefCardCount), out);
        return cards;
      }();
  return kAllCards;
}

}  // namespace ro::sim::survivors

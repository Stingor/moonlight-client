#include <array>
#include <cstdint>
#include <span>

#include "ro/core/i18n.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "survivors_shipped.h"

namespace ro::sim::survivors {

namespace {

// ── LE CHEVALIER : sa classe, ses skills et ses évolutions ─────────────────
// Il tient la ligne : plus de PV que le Magicien, une marche plus lente, un
// aimant plus court, et des coups courts qui repoussent. Ses évolutions sont
// les skills de Lord Knight. Épéiste au départ, il devient Chevalier à sa
// première évolution, et Lord Knight à sa renaissance.

constexpr std::uint16_t kKnightClassId = 2;
constexpr std::uint16_t kSwordmanJobId = 1;           // l'apparence du départ
constexpr std::uint16_t kKnightJobId = 7;             // après une évolution
constexpr std::uint16_t kLordKnightJobId = 4008;      // rené
constexpr std::uint16_t kKnightPecoJobId = 13;        // monté sur son Peco Peco
constexpr std::uint16_t kLordKnightPecoJobId = 4014;  // monté, rené
constexpr std::int32_t kKnightMaxHp = 900;
constexpr std::uint32_t kKnightWalkMsPerCell = 165;
constexpr std::int32_t kKnightMagnetTenths = 24;
constexpr std::int32_t kKnightDamagePctPerLevel = 1;
constexpr std::int32_t kKnightDamagePctCap = 25;
constexpr std::uint16_t kKnightRebornLevel = 50;
constexpr std::int32_t kKnightRebornDamagePct = 5;
// Il combat le boss final au corps à corps, à portée de tous ses coups : ces PV
// font durer le combat de 20 à 60 s, et ses propres PV le lui font tenir.
constexpr std::int32_t kKnightFinalBossHpBonusPct = 50;

// Bash : le coup de lance, sur ce qui est à portée de bras.
constexpr std::int32_t kBashReachTenths = 32;
constexpr std::uint32_t kBashIntervalMs = 100;
constexpr std::int32_t kBashKnockbackTenths = 5;

// Magnum Break : une onde de feu autour du héros, qui repousse.
constexpr std::int32_t kMagnumBreakKnockbackCells = 2;

// Pierce : un coup droit et court, qui traverse la file.
constexpr std::int32_t kPierceReachCells = 4;
constexpr std::int32_t kPierceSpeedCellsPerSecond = 16;
constexpr std::int32_t kPierceHitRadiusTenths = 5;

// Spear Boomerang : la lance lancée loin, seul coup à distance.
constexpr std::int32_t kBoomerangReachCells = 9;
constexpr std::int32_t kBoomerangSpeedCellsPerSecond = 13;
constexpr std::int32_t kBoomerangHitRadiusTenths = 5;

// Bowling Bash : un coup sur la foule la plus dense à portée, qui la disperse.
constexpr std::int32_t kBowlingBashReachCells = 3;
constexpr std::uint32_t kBowlingBashIntervalMs = 150;
constexpr std::int32_t kBowlingBashSplashPct = 70;
constexpr std::int32_t kBowlingBashKnockbackCells = 2;

// Un niveau de posture : `bonus` à sa statistique pendant `life_ms`, toutes
// les `cooldown_ms`.
[[nodiscard]] constexpr SkillLevel Stance(std::uint32_t life_ms, std::uint32_t cooldown_ms,
                                          std::int32_t bonus) noexcept {
  return SkillLevel{.cooldown_ms = cooldown_ms, .life_ms = life_ms, .stance_bonus = bonus};
}

// Un niveau de Bash : `amount` coups de `damage`.
[[nodiscard]] constexpr SkillLevel Bash(std::uint8_t amount, std::int32_t damage,
                                        std::uint32_t cooldown_ms) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .interval_ms = kBashIntervalMs,
                    .reach_units = CellTenths(kBashReachTenths),
                    .knockback_units = CellTenths(kBashKnockbackTenths)};
}

// Un niveau de Magnum Break : une onde de `damage`, de rayon `radius_tenths`
// dixièmes de case.
[[nodiscard]] constexpr SkillLevel Magnum(std::int32_t damage, std::uint32_t cooldown_ms,
                                          std::int32_t radius_tenths) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .area_radius_units = CellTenths(radius_tenths),
                    .knockback_units = Cells(kMagnumBreakKnockbackCells)};
}

// Un niveau de Pierce : `amount` coups de `damage`, qui traversent `pierce`
// monstres de plus que le premier.
[[nodiscard]] constexpr SkillLevel Pierce(std::uint8_t amount, std::int32_t damage,
                                          std::uint32_t cooldown_ms, std::uint8_t pierce) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .reach_units = Cells(kPierceReachCells),
                    .speed_units_per_s = Cells(kPierceSpeedCellsPerSecond),
                    .hit_radius_units = CellTenths(kPierceHitRadiusTenths),
                    .pierce = pierce};
}

// Un niveau de Spear Boomerang : `amount` lances de `damage`.
[[nodiscard]] constexpr SkillLevel Boomerang(std::uint8_t amount, std::int32_t damage,
                                             std::uint32_t cooldown_ms,
                                             std::uint8_t pierce) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .reach_units = Cells(kBoomerangReachCells),
                    .speed_units_per_s = Cells(kBoomerangSpeedCellsPerSecond),
                    .hit_radius_units = CellTenths(kBoomerangHitRadiusTenths),
                    .pierce = pierce};
}

// Un niveau de cône : `damage`, long de `length_tenths` dixièmes de case,
// ouvert de `half_steps` caps de chaque côté, qui repousse de
// `knockback_tenths` dixièmes de case.
[[nodiscard]] constexpr SkillLevel Sweep(std::int32_t damage, std::uint32_t cooldown_ms,
                                         std::int32_t length_tenths, std::uint8_t half_steps,
                                         std::int32_t knockback_tenths) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .reach_units = CellTenths(length_tenths),
                    .knockback_units = CellTenths(knockback_tenths),
                    .cone_half_steps = half_steps};
}

// Un niveau de Bowling Bash : `amount` coups de `damage`, de rayon
// `radius_tenths` dixièmes de case.
[[nodiscard]] constexpr SkillLevel Bowling(std::uint8_t amount, std::int32_t damage,
                                           std::uint32_t cooldown_ms,
                                           std::int32_t radius_tenths) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .interval_ms = kBowlingBashIntervalMs,
                    .reach_units = Cells(kBowlingBashReachCells),
                    .area_radius_units = CellTenths(radius_tenths),
                    .splash_pct = kBowlingBashSplashPct,
                    .knockback_units = Cells(kBowlingBashKnockbackCells)};
}

// Bash(coups, dégâts, recharge en ms).
constexpr std::array<SkillLevel, kMostSkillLevels> kBashLevels{{
    Bash(1, 24, 900),
    Bash(2, 24, 900),
    Bash(2, 29, 900),
    Bash(3, 29, 850),
    Bash(3, 34, 850),
    Bash(4, 34, 800),
    Bash(4, 40, 800),
    Bash(5, 46, 750),
}};

// Magnum(dégâts, recharge en ms, rayon en dixièmes de case).
constexpr std::array<SkillLevel, kMostSkillLevels> kMagnumBreakLevels{{
    Magnum(26, 2000, 30),
    Magnum(32, 2000, 30),
    Magnum(32, 1900, 33),
    Magnum(38, 1900, 33),
    Magnum(38, 1800, 36),
    Magnum(46, 1800, 36),
    Magnum(46, 1700, 40),
    Magnum(56, 1500, 42),
}};

// Pierce(coups, dégâts, recharge en ms, perçage).
constexpr std::array<SkillLevel, kMostSkillLevels> kPierceLevels{{
    Pierce(1, 24, 1200, 1),
    Pierce(1, 30, 1200, 1),
    Pierce(2, 30, 1200, 1),
    Pierce(2, 30, 1100, 2),
    Pierce(2, 36, 1100, 2),
    Pierce(3, 36, 1000, 2),
    Pierce(3, 42, 1000, 3),
    Pierce(4, 48, 900, 3),
}};

// Sweep(dégâts, recharge en ms, longueur en dixièmes, demi-ouverture,
// recul en dixièmes) : la pique étroite et longue, qui repousse loin.
constexpr std::array<SkillLevel, kMostSkillLevels> kSpearStabLevels{{
    Sweep(28, 1800, 40, 2, 15),
    Sweep(34, 1800, 40, 2, 15),
    Sweep(34, 1700, 45, 2, 20),
    Sweep(41, 1700, 45, 3, 20),
    Sweep(41, 1600, 50, 3, 20),
    Sweep(49, 1600, 50, 3, 25),
    Sweep(49, 1500, 55, 3, 25),
    Sweep(60, 1400, 60, 4, 30),
}};

// Sweep(…) : le balayage large et court de Brandish Spear.
constexpr std::array<SkillLevel, kMostSkillLevels> kBrandishSpearLevels{{
    Sweep(30, 1500, 28, 8, 5),
    Sweep(36, 1500, 28, 8, 5),
    Sweep(36, 1400, 30, 9, 5),
    Sweep(43, 1400, 30, 9, 5),
    Sweep(43, 1300, 32, 10, 8),
    Sweep(52, 1300, 32, 10, 8),
    Sweep(52, 1200, 35, 11, 8),
    Sweep(64, 1100, 35, 12, 10),
}};

// Boomerang(lances, dégâts, recharge en ms, perçage).
constexpr std::array<SkillLevel, kMostSkillLevels> kSpearBoomerangLevels{{
    Boomerang(1, 26, 1600, 0),
    Boomerang(1, 32, 1600, 0),
    Boomerang(2, 32, 1600, 0),
    Boomerang(2, 32, 1400, 1),
    Boomerang(2, 38, 1400, 1),
    Boomerang(3, 38, 1300, 1),
    Boomerang(3, 44, 1300, 1),
    Boomerang(4, 50, 1200, 2),
}};

// Bowling(coups, dégâts, recharge en ms, rayon en dixièmes de case).
constexpr std::array<SkillLevel, kMostSkillLevels> kBowlingBashLevels{{
    Bowling(1, 29, 2000, 12),
    Bowling(1, 34, 2000, 12),
    Bowling(2, 34, 2000, 12),
    Bowling(2, 34, 1800, 15),
    Bowling(2, 41, 1800, 15),
    Bowling(3, 41, 1800, 15),
    Bowling(3, 41, 1600, 18),
    Bowling(4, 49, 1500, 18),
}};

// Endure(durée en ms, recharge en ms, armure) : le Chevalier encaisse.
constexpr std::array<SkillLevel, kMostSkillLevels> kEndureLevels{{
    Stance(5000, 16000, 2),
    Stance(5500, 15500, 2),
    Stance(6000, 15000, 3),
    Stance(6500, 14500, 3),
    Stance(7000, 14000, 4),
    Stance(7500, 13500, 4),
    Stance(8000, 13000, 5),
    Stance(8000, 12000, 6),
}};

// Two-Hand Quicken(durée en ms, recharge en ms, recharges raccourcies en %) :
// tous ses coups reviennent plus vite.
constexpr std::array<SkillLevel, kMostSkillLevels> kTwoHandQuickenLevels{{
    Stance(5000, 15000, 15),
    Stance(5500, 14500, 18),
    Stance(6000, 14000, 20),
    Stance(6500, 13500, 23),
    Stance(7000, 13000, 25),
    Stance(7500, 12500, 28),
    Stance(8000, 12000, 30),
    Stance(8000, 11000, 35),
}};

// ── Les évolués : un niveau chacun, que seul un coffre donne ──────────────

// Head Crush : trois coups écrasants sur ce qui est à portée de bras.
constexpr std::array<SkillLevel, 1> kHeadCrushLevels{{
    {.damage = 55,
     .cooldown_ms = 800,
     .amount = 3,
     .interval_ms = 120,
     .reach_units = CellTenths(28),
     .knockback_units = Cells(1)},
}};

// Spiral Pierce : quatre lances qui traversent toute la file.
constexpr std::array<SkillLevel, 1> kSpiralPierceLevels{{
    {.damage = 50,
     .cooldown_ms = 900,
     .amount = 4,
     .reach_units = Cells(8),
     .speed_units_per_s = Cells(18),
     .hit_radius_units = CellTenths(7),
     .pierce = 3},
}};

// Joint Beat : un large coup devant le héros, qui brise les jambes et fige.
constexpr std::array<SkillLevel, 1> kJointBeatLevels{{
    {.damage = 55,
     .cooldown_ms = 1300,
     .reach_units = CellTenths(45),
     .knockback_units = CellTenths(5),
     .freeze_ms = 1500,
     .cone_half_steps = 12},
}};

// Aura Blade : l'onde de Magnum Break, qu'il remplace, plus forte et plus large.
constexpr std::array<SkillLevel, 1> kAuraBladeLevels{{
    {.damage = 80, .cooldown_ms = 1300, .area_radius_units = Cells(5), .knockback_units = Cells(2)},
}};

// Concentration(durée en ms, recharge en ms, dégâts en %) : ses coups portent
// mieux, par moments.
constexpr std::array<SkillLevel, 1> kConcentrationLevels{{
    Stance(8000, 15000, 25),
}};

// Frenzy(durée en ms, recharge en ms, dégâts en %) : plus du double du gain de
// Concentration, moins longtemps.
constexpr std::array<SkillLevel, 1> kFrenzyLevels{{
    Stance(7000, 14000, 60),
}};

// Ignition Break : l'union de Bowling Bash et de Brandish Spear, trois
// explosions autour du héros, qui repoussent.
constexpr std::array<SkillLevel, 1> kIgnitionBreakLevels{{
    {.damage = 65,
     .cooldown_ms = 2500,
     .amount = 3,
     .interval_ms = 350,
     .area_radius_units = Cells(6),
     .knockback_units = Cells(2)},
}};

constexpr std::array<SurvivorsSkillRule, kKnightPoolSkillCount + kKnightEvolvedSkillCount>
    kKnightSkills{{
        {.skill_id = kBashId,
         .key = "bash",
         .description = core::TrText("Des coups de lance frappent le monstre à portée de bras."),
         .archetype = SkillArchetype::kTargetStrike,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kBashLevels},
        {.skill_id = kMagnumBreakId,
         .key = "magnum_break",
         .description = core::TrText("Une onde de feu jaillit de vous et repousse la horde."),
         .archetype = SkillArchetype::kNova,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kFire,
         .levels = kMagnumBreakLevels},
        {.skill_id = kPierceId,
         .key = "pierce",
         .description = core::TrText("Un coup de lance droit traverse la file des monstres."),
         .archetype = SkillArchetype::kLinear,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kPierceLevels},
        {.skill_id = kSpearStabId,
         .key = "spear_stab",
         .description = core::TrText("Une longue pique devant vous repousse les monstres au loin."),
         .archetype = SkillArchetype::kCone,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kSpearStabLevels},
        {.skill_id = kBrandishSpearId,
         .key = "brandish_spear",
         .description = core::TrText("La lance balaie largement devant vous, là où vous marchez."),
         .archetype = SkillArchetype::kCone,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kBrandishSpearLevels},
        {.skill_id = kSpearBoomerangId,
         .key = "spear_boomerang",
         .description = core::TrText("Une lance lancée au loin frappe le monstre le plus proche."),
         .archetype = SkillArchetype::kLinear,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kSpearBoomerangLevels},
        {.skill_id = kBowlingBashId,
         .key = "bowling_bash",
         .description = core::TrText("Un coup furieux disperse la foule la plus dense à portée."),
         .archetype = SkillArchetype::kClusterBlast,
         .target = TargetRule::kDensest,
         .element = SurvivorsElement::kNeutral,
         .levels = kBowlingBashLevels},
        {.skill_id = kEndureId,
         .key = "endure",
         .description = core::TrText("Par moments, vous encaissez : votre armure augmente."),
         .archetype = SkillArchetype::kStance,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kEndureLevels,
         .stance_stat = SurvivorsStat::kArmor},
        {.skill_id = kTwoHandQuickenId,
         .key = "two_hand_quicken",
         .description = core::TrText(
             "Par moments, vos coups s'enchaînent : toutes les recharges raccourcissent."),
         .archetype = SkillArchetype::kStance,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kTwoHandQuickenLevels,
         .stance_stat = SurvivorsStat::kCooldownPct},
        {.skill_id = kHeadCrushId,
         .key = "head_crush",
         .description = core::TrText("Des coups écrasants s'abattent sur ce qui vous touche."),
         .archetype = SkillArchetype::kTargetStrike,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kHeadCrushLevels},
        {.skill_id = kSpiralPierceId,
         .key = "spiral_pierce",
         .description = core::TrText("Des lances en spirale transpercent toute la file."),
         .archetype = SkillArchetype::kLinear,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kSpiralPierceLevels},
        {.skill_id = kJointBeatId,
         .key = "joint_beat",
         .description = core::TrText("Un coup large brise les jambes des monstres et les fige."),
         .archetype = SkillArchetype::kCone,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kJointBeatLevels},
        {.skill_id = kAuraBladeId,
         .key = "aura_blade",
         .description = core::TrText("Une aura tranchante jaillit de vous et balaie la horde."),
         .archetype = SkillArchetype::kNova,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kAuraBladeLevels},
        {.skill_id = kConcentrationId,
         .key = "concentration",
         .description = core::TrText("Par moments, vous vous concentrez : vos dégâts augmentent."),
         .archetype = SkillArchetype::kStance,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kConcentrationLevels,
         .stance_stat = SurvivorsStat::kDamagePct},
        {.skill_id = kFrenzyId,
         .key = "frenzy",
         .description = core::TrText("Par moments, la furie vous prend : vos dégâts explosent."),
         .archetype = SkillArchetype::kStance,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kFrenzyLevels,
         .stance_stat = SurvivorsStat::kDamagePct},
        {.skill_id = kIgnitionBreakId,
         .key = "ignition_break",
         .description = core::TrText("Le sol explose autour de vous, trois fois de suite."),
         .archetype = SkillArchetype::kStorm,
         .target = TargetRule::kHero,
         .element = SurvivorsElement::kFire,
         .levels = kIgnitionBreakLevels},
    }};

constexpr std::array<EvolutionRule, kKnightEvolvedSkillCount> kKnightEvolutions{{
    {.skill_id = kBashId, .card_item_id = kGolemCardId, .evolved_skill_id = kHeadCrushId},
    {.skill_id = kPierceId, .card_item_id = kHornetCardId, .evolved_skill_id = kSpiralPierceId},
    {.skill_id = kSpearStabId, .card_item_id = kEclipseCardId, .evolved_skill_id = kJointBeatId},
    {.skill_id = kMagnumBreakId,
     .card_item_id = kDragonFlyCardId,
     .evolved_skill_id = kAuraBladeId},
    {.skill_id = kEndureId, .card_item_id = kPupaCardId, .evolved_skill_id = kConcentrationId},
    {.skill_id = kTwoHandQuickenId,
     .card_item_id = kVagabondWolfCardId,
     .evolved_skill_id = kFrenzyId},
    // L'UNION : Bowling Bash et Brandish Spear, tous deux au plus haut niveau.
    {.skill_id = kBowlingBashId,
     .card_item_id = 0,
     .evolved_skill_id = kIgnitionBreakId,
     .partner_skill_id = kBrandishSpearId},
}};

constexpr std::array<std::uint16_t, 3> kKnightStarterSkills{kBashId, kPierceId, kMagnumBreakId};
constexpr std::array<std::uint16_t, kKnightPoolSkillCount> kKnightSkillPool{
    kBashId,           kMagnumBreakId, kPierceId, kSpearStabId,     kBrandishSpearId,
    kSpearBoomerangId, kBowlingBashId, kEndureId, kTwoHandQuickenId};

// La carte Peco Peco : le Chevalier monte son oiseau, et sa marche bondit.
constexpr std::array<std::int32_t, kMostCardLevels> kPecoPecoLevels{10, 20, 30, 40, 50};
constexpr std::array<CardRule, kKnightCardCount> kKnightCards{{
    {.card_item_id = kPecoPecoCardId,
     .key = "peco_peco_card",
     .description = core::TrText("Vous montez votre Peco Peco : vitesse de marche augmentée."),
     .stat = SurvivorsStat::kWalkPct,
     .per_level = kPecoPecoLevels},
}};

// Les cartes sont les passifs du Magicien, sauf la marche : le Peco Peco
// tient la place de Chonchon.
constexpr std::array<std::uint16_t, 16> kKnightCardPool{
    kHornetCardId,  kPupaCardId,      kPecoPecoCardId,     kPoringCardId,
    kSporeCardId,   kThiefBugCardId,  kGolemCardId,        kMasteringCardId,
    kEclipseCardId, kDragonFlyCardId, kVagabondWolfCardId, kBaphometCardId,
    kSmokieCardId,  kMantisCardId,    kOrcSkeletonCardId,  kNightmareCardId};

constexpr ClassRule kKnight{.class_id = kKnightClassId,
                            .key = "knight",
                            .job_id = kSwordmanJobId,
                            .weapon_item_id = kSwordItemId,
                            .max_hp = kKnightMaxHp,
                            .walk_ms_per_cell = kKnightWalkMsPerCell,
                            .magnet_units = CellTenths(kKnightMagnetTenths),
                            .damage_pct_per_level = kKnightDamagePctPerLevel,
                            .damage_pct_cap = kKnightDamagePctCap,
                            .weapon_skill_id = kSwordSlashSkillId,
                            .starter_skills = kKnightStarterSkills,
                            .skill_pool = kKnightSkillPool,
                            .card_pool = kKnightCardPool,
                            .evolved_job_id = kKnightJobId,
                            .evolved_weapon_item_id = kJavelinItemId,
                            .attack_gesture = true,
                            .mount_card_item_id = kPecoPecoCardId,
                            .mounted_job_id = kKnightPecoJobId,
                            .reborn_level = kKnightRebornLevel,
                            .reborn_job_id = kLordKnightJobId,
                            .reborn_mounted_job_id = kLordKnightPecoJobId,
                            .reborn_damage_pct = kKnightRebornDamagePct,
                            .final_boss_hp_bonus_pct = kKnightFinalBossHpBonusPct};

}  // namespace

const ClassRule& KnightClass() { return kKnight; }

std::span<const SurvivorsSkillRule> KnightSkills() { return kKnightSkills; }

std::span<const CardRule> KnightCards() { return kKnightCards; }

std::span<const EvolutionRule> KnightEvolutions() { return kKnightEvolutions; }

}  // namespace ro::sim::survivors

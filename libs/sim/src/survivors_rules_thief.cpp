#include <array>
#include <cstdint>
#include <span>

#include "ro/core/i18n.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "survivors_shipped.h"

namespace ro::sim::survivors {

namespace {

// ── LE VOLEUR : sa classe, ses skills et ses évolutions ────────────────────
// Il entre et il sort : le plus rapide et le plus fragile, il frappe au contact
// et laisse le poison finir le travail. Thief au départ, il devient Assassin à
// sa première évolution, et Assassin Cross à sa renaissance.

constexpr std::uint16_t kThiefClassId = 3;
constexpr std::uint16_t kThiefJobId = 6;               // l'apparence du départ
constexpr std::uint16_t kAssassinJobId = 12;           // après une évolution
constexpr std::uint16_t kAssassinCrossJobId = 4013;    // rené
constexpr std::uint16_t kThiefWeaponItemId = 1201;     // Knife
constexpr std::uint16_t kAssassinWeaponItemId = 1250;  // Jur, un katar
constexpr std::int32_t kThiefMaxHp = 560;
constexpr std::uint32_t kThiefWalkMsPerCell = 135;
constexpr std::int32_t kThiefMagnetTenths = 27;
constexpr std::int32_t kThiefDamagePctPerLevel = 1;
constexpr std::int32_t kThiefDamagePctCap = 25;
constexpr std::uint16_t kThiefRebornLevel = 50;
constexpr std::int32_t kThiefRebornDamagePct = 5;
// Ses coups portent fort sur le boss final : ces PV font durer le combat de 20 à
// 60 s, et les siens le lui font tenir.
constexpr std::int32_t kThiefFinalBossHpBonusPct = 30;

// Double Attack et Envenom : les coups de dague, à portée de bras.
constexpr std::int32_t kDaggerReachTenths = 42;
constexpr std::uint32_t kDoubleAttackIntervalMs = 80;

// Throw Stone : le seul coup à distance du début.
constexpr std::int32_t kThrowStoneReachCells = 7;
constexpr std::int32_t kThrowStoneSpeedCellsPerSecond = 13;
constexpr std::int32_t kThrowStoneHitRadiusTenths = 5;

// Sprinkle Sand : un cône court et large, devant lui.
constexpr std::uint8_t kSprinkleSandHalfSteps = 9;

// Back Slide : un cône derrière lui, qui repousse ce qui le talonne.
constexpr std::uint8_t kBackSlideHalfSteps = 10;
constexpr std::int32_t kBackSlideKnockbackCells = 3;

// Grimtooth : un coup droit et court, qui perce la file.
constexpr std::int32_t kGrimtoothReachCells = 5;
constexpr std::int32_t kGrimtoothSpeedCellsPerSecond = 16;
constexpr std::int32_t kGrimtoothHitRadiusTenths = 6;

// Venom Dust : un nuage posé sur le plus proche, par vagues.
constexpr std::int32_t kVenomDustReachCells = 6;
constexpr std::uint32_t kVenomDustIntervalMs = 500;

// Le réflexe de survie : il part à un coup reçu sous ce seuil de PV. Hiding le
// dérobe et lui retire ses coups, pas sa marche ; Cloaking dure plus longtemps,
// plus souvent, et lui laisse ses coups.
constexpr std::int32_t kReflexBelowHpPct = 50;
constexpr std::uint32_t kHidingLifeMs = 2000;
constexpr std::uint32_t kHidingCooldownMs = 30000;
constexpr std::uint32_t kHidingCooldownCutPerLevelMs = 1000;
constexpr std::uint32_t kCloakingLifeMs = 4000;
constexpr std::uint32_t kCloakingCooldownMs = 20000;

// Un niveau de Double Attack : `amount` coups de `damage`.
[[nodiscard]] constexpr SkillLevel Dagger(std::uint8_t amount, std::int32_t damage,
                                          std::uint32_t cooldown_ms) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .interval_ms = kDoubleAttackIntervalMs,
                    .reach_units = CellTenths(kDaggerReachTenths)};
}

// Un niveau d'Envenom : un coup de `damage`, et le poison de `tick_damage` à
// chaque tic pendant `poison_ms`.
[[nodiscard]] constexpr SkillLevel Venom(std::int32_t damage, std::uint32_t cooldown_ms,
                                         std::uint32_t poison_ms,
                                         std::int32_t tick_damage) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .reach_units = CellTenths(kDaggerReachTenths),
                    .poison_ms = poison_ms,
                    .poison_tick_damage = tick_damage};
}

// Un niveau de Throw Stone : `amount` pierres de `damage`.
[[nodiscard]] constexpr SkillLevel Stone(std::uint8_t amount, std::int32_t damage,
                                         std::uint32_t cooldown_ms) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .reach_units = Cells(kThrowStoneReachCells),
                    .speed_units_per_s = Cells(kThrowStoneSpeedCellsPerSecond),
                    .hit_radius_units = CellTenths(kThrowStoneHitRadiusTenths)};
}

// Un niveau de Sprinkle Sand : `damage`, long de `length_tenths` dixièmes de
// case, qui aveugle `blind_ms`.
[[nodiscard]] constexpr SkillLevel Sand(std::int32_t damage, std::uint32_t cooldown_ms,
                                        std::int32_t length_tenths,
                                        std::uint32_t blind_ms) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .reach_units = CellTenths(length_tenths),
                    .blind_ms = blind_ms,
                    .cone_half_steps = kSprinkleSandHalfSteps};
}

// Un niveau de Back Slide : `damage`, long de `length_tenths` dixièmes de case.
[[nodiscard]] constexpr SkillLevel Slide(std::int32_t damage, std::uint32_t cooldown_ms,
                                         std::int32_t length_tenths) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .reach_units = CellTenths(length_tenths),
                    .knockback_units = Cells(kBackSlideKnockbackCells),
                    .cone_half_steps = kBackSlideHalfSteps};
}

// Un niveau de Grimtooth : `amount` coups de `damage`, qui traversent `pierce`
// monstres de plus que le premier.
[[nodiscard]] constexpr SkillLevel Tooth(std::uint8_t amount, std::int32_t damage,
                                         std::uint32_t cooldown_ms, std::uint8_t pierce) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .reach_units = Cells(kGrimtoothReachCells),
                    .speed_units_per_s = Cells(kGrimtoothSpeedCellsPerSecond),
                    .hit_radius_units = CellTenths(kGrimtoothHitRadiusTenths),
                    .pierce = pierce};
}

// Un niveau de Venom Dust : `waves` vagues de `damage`, de rayon `radius_tenths`
// dixièmes de case, et le poison de `tick_damage` à chaque tic pendant
// `poison_ms`.
[[nodiscard]] constexpr SkillLevel Dust(std::uint8_t waves, std::int32_t damage,
                                        std::uint32_t cooldown_ms, std::int32_t radius_tenths,
                                        std::uint32_t poison_ms,
                                        std::int32_t tick_damage) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = waves,
                    .interval_ms = kVenomDustIntervalMs,
                    .reach_units = Cells(kVenomDustReachCells),
                    .area_radius_units = CellTenths(radius_tenths),
                    .poison_ms = poison_ms,
                    .poison_tick_damage = tick_damage};
}

// Un niveau de posture : `bonus` à sa statistique pendant `life_ms`, toutes les
// `cooldown_ms`.
[[nodiscard]] constexpr SkillLevel Stance(std::uint32_t life_ms, std::uint32_t cooldown_ms,
                                          std::int32_t bonus) noexcept {
  return SkillLevel{.cooldown_ms = cooldown_ms, .life_ms = life_ms, .stance_bonus = bonus};
}

// Le niveau `level` de Hiding : la recharge raccourcit, la durée reste.
[[nodiscard]] constexpr SkillLevel Hide(std::uint32_t level) noexcept {
  return SkillLevel{.cooldown_ms = kHidingCooldownMs - (level - 1) * kHidingCooldownCutPerLevelMs,
                    .life_ms = kHidingLifeMs,
                    .reflex_below_hp_pct = kReflexBelowHpPct,
                    .reflex_sheathes = true};
}

// Dagger(coups, dégâts, recharge en ms).
constexpr std::array<SkillLevel, kMostSkillLevels> kDoubleAttackLevels{{
    Dagger(2, 25, 800),
    Dagger(2, 30, 800),
    Dagger(2, 36, 800),
    Dagger(3, 36, 750),
    Dagger(3, 43, 750),
    Dagger(4, 43, 700),
    Dagger(4, 50, 700),
    Dagger(5, 58, 650),
}};

// Venom(dégâts, recharge en ms, poison en ms, dégâts d'un tic).
constexpr std::array<SkillLevel, kMostSkillLevels> kEnvenomLevels{{
    Venom(39, 1400, 3000, 8),
    Venom(46, 1400, 3000, 11),
    Venom(46, 1300, 4000, 11),
    Venom(55, 1300, 4000, 14),
    Venom(55, 1200, 4000, 17),
    Venom(64, 1200, 5000, 17),
    Venom(64, 1100, 5000, 23),
    Venom(80, 1000, 5000, 30),
}};

// Stone(pierres, dégâts, recharge en ms).
constexpr std::array<SkillLevel, kMostSkillLevels> kThrowStoneLevels{{
    Stone(1, 40, 1000),
    Stone(1, 50, 1000),
    Stone(2, 50, 1000),
    Stone(2, 50, 900),
    Stone(2, 62, 900),
    Stone(3, 62, 800),
    Stone(3, 75, 800),
    Stone(4, 85, 700),
}};

// Sand(dégâts, recharge en ms, longueur en dixièmes, aveuglement en ms).
constexpr std::array<SkillLevel, kMostSkillLevels> kSprinkleSandLevels{{
    Sand(20, 2400, 30, 1500),
    Sand(24, 2400, 30, 1500),
    Sand(24, 2200, 33, 1800),
    Sand(30, 2200, 33, 1800),
    Sand(30, 2000, 36, 2100),
    Sand(37, 2000, 36, 2100),
    Sand(37, 1800, 40, 2400),
    Sand(46, 1600, 42, 3000),
}};

// Slide(dégâts, recharge en ms, longueur en dixièmes) : la recharge d'un geste
// qu'on ne répète pas sans arrêt.
constexpr std::array<SkillLevel, kMostSkillLevels> kBackSlideLevels{{
    Slide(35, 4000, 30),
    Slide(41, 4000, 30),
    Slide(41, 3600, 33),
    Slide(52, 3600, 33),
    Slide(52, 3200, 36),
    Slide(63, 3000, 36),
    Slide(63, 2700, 40),
    Slide(80, 2400, 42),
}};

// Tooth(coups, dégâts, recharge en ms, perçage).
constexpr std::array<SkillLevel, kMostSkillLevels> kGrimtoothLevels{{
    Tooth(1, 28, 1300, 1),
    Tooth(1, 34, 1300, 1),
    Tooth(2, 34, 1300, 1),
    Tooth(2, 34, 1200, 2),
    Tooth(2, 41, 1200, 2),
    Tooth(3, 41, 1100, 2),
    Tooth(3, 48, 1100, 3),
    Tooth(4, 55, 1000, 3),
}};

// Dust(vagues, dégâts, recharge en ms, rayon en dixièmes, poison en ms, dégâts
// d'un tic).
constexpr std::array<SkillLevel, kMostSkillLevels> kVenomDustLevels{{
    Dust(4, 23, 3200, 20, 2000, 8),
    Dust(4, 29, 3200, 20, 2000, 11),
    Dust(5, 29, 3200, 20, 3000, 11),
    Dust(5, 29, 2800, 22, 3000, 15),
    Dust(5, 37, 2800, 22, 3000, 15),
    Dust(6, 37, 2800, 25, 4000, 19),
    Dust(6, 37, 2400, 25, 4000, 23),
    Dust(7, 48, 2400, 25, 4000, 28),
}};

// Enchant Poison(durée en ms, recharge en ms, dégâts en %) : ses coups mordent
// plus, par moments.
constexpr std::array<SkillLevel, kMostSkillLevels> kEnchantPoisonLevels{{
    Stance(5000, 15000, 10),
    Stance(5500, 14500, 12),
    Stance(6000, 14000, 14),
    Stance(6500, 13500, 16),
    Stance(7000, 13000, 18),
    Stance(7500, 12500, 20),
    Stance(8000, 12000, 22),
    Stance(8000, 11000, 25),
}};

// Improve Dodge(durée en ms, recharge en ms, marche en %) : une fuite périodique.
constexpr std::array<SkillLevel, kMostSkillLevels> kImproveDodgeLevels{{
    Stance(3000, 15000, 15),
    Stance(3000, 14500, 18),
    Stance(3500, 14000, 20),
    Stance(3500, 13500, 23),
    Stance(4000, 13000, 25),
    Stance(4000, 12500, 28),
    Stance(4500, 12000, 30),
    Stance(5000, 11000, 35),
}};

constexpr std::array<SkillLevel, kMostSkillLevels> kHidingLevels{{
    Hide(1),
    Hide(2),
    Hide(3),
    Hide(4),
    Hide(5),
    Hide(6),
    Hide(7),
    Hide(8),
}};

// ── Les évolués : un niveau chacun, que seul un coffre donne ──────────────

// Sonic Blow : huit coups qui tombent presque ensemble.
constexpr std::array<SkillLevel, 1> kSonicBlowLevels{{
    {.damage = 65,
     .cooldown_ms = 1300,
     .amount = 8,
     .interval_ms = 40,
     .reach_units = CellTenths(kDaggerReachTenths)},
}};

// Soul Destroyer : un seul projectile, très fort, qui traverse un monstre.
constexpr std::array<SkillLevel, 1> kSoulDestroyerLevels{{
    {.damage = 340,
     .cooldown_ms = 1600,
     .reach_units = Cells(10),
     .speed_units_per_s = Cells(16),
     .hit_radius_units = CellTenths(7),
     .pierce = 1},
}};

// Venom Splasher : des éclats sur la foule la plus dense.
constexpr std::array<SkillLevel, 1> kVenomSplasherLevels{{
    {.damage = 150,
     .cooldown_ms = 1800,
     .amount = 3,
     .interval_ms = 200,
     .reach_units = Cells(8),
     .area_radius_units = CellTenths(30),
     .splash_pct = 70},
}};

// Enchant Deadly Poison(durée en ms, recharge en ms, dégâts en %) : nettement
// plus fort qu'Enchant Poison.
constexpr std::array<SkillLevel, 1> kEnchantDeadlyPoisonLevels{{
    Stance(8000, 15000, 60),
}};

// Cloaking : le réflexe de Hiding, qui lui laisse ses coups et sa marche.
constexpr std::array<SkillLevel, 1> kCloakingLevels{{
    {.cooldown_ms = kCloakingCooldownMs,
     .life_ms = kCloakingLifeMs,
     .reflex_below_hp_pct = kReflexBelowHpPct},
}};

// Throw Venom Knife : des couteaux droits, qui traversent et empoisonnent.
constexpr std::array<SkillLevel, 1> kThrowVenomKnifeLevels{{
    {.damage = 90,
     .cooldown_ms = 800,
     .amount = 3,
     .reach_units = Cells(9),
     .speed_units_per_s = Cells(16),
     .hit_radius_units = CellTenths(5),
     .pierce = 1,
     .poison_ms = 4000,
     .poison_tick_damage = 30},
}};

// Meteor Assault : l'union d'Envenom et de Sprinkle Sand, une onde autour du
// héros, qui repousse.
constexpr std::array<SkillLevel, 1> kMeteorAssaultLevels{{
    {.damage = 170,
     .cooldown_ms = 1600,
     .area_radius_units = Cells(5),
     .knockback_units = Cells(1)},
}};

constexpr std::array<SurvivorsSkillRule, kThiefPoolSkillCount + kThiefEvolvedSkillCount>
    kThiefSkills{{
        {.skill_id = kDoubleAttackId,
         .key = "double_attack",
         .description = core::TrText("Des coups de dague frappent le monstre à portée de bras."),
         .archetype = SkillArchetype::kTargetStrike,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kDoubleAttackLevels},
        {.skill_id = kEnvenomId,
         .key = "envenom",
         .description = core::TrText("Un coup empoisonné : le monstre perd ses PV peu à peu."),
         .archetype = SkillArchetype::kTargetStrike,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kPoison,
         .levels = kEnvenomLevels},
        {.skill_id = kThrowStoneId,
         .key = "throw_stone",
         .description = core::TrText("Une pierre lancée frappe le monstre le plus proche."),
         .archetype = SkillArchetype::kLinear,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kThrowStoneLevels},
        {.skill_id = kSprinkleSandId,
         .key = "sprinkle_sand",
         .description = core::TrText("Du sable jeté devant vous aveugle la horde."),
         .archetype = SkillArchetype::kCone,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kSprinkleSandLevels},
        {.skill_id = kBackSlideId,
         .key = "back_slide",
         .description = core::TrText("Un bond en arrière blesse et repousse ce qui vous talonne."),
         .archetype = SkillArchetype::kCone,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kBackSlideLevels,
         .cone_behind = true},
        {.skill_id = kGrimtoothId,
         .key = "grimtooth",
         .description = core::TrText("Un coup droit et court transperce la file des monstres."),
         .archetype = SkillArchetype::kLinear,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kGrimtoothLevels},
        {.skill_id = kVenomDustId,
         .key = "venom_dust",
         .description = core::TrText("Un nuage de poison blesse et empoisonne, vague après vague."),
         .archetype = SkillArchetype::kStorm,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kPoison,
         .levels = kVenomDustLevels},
        {.skill_id = kEnchantPoisonId,
         .key = "enchant_poison",
         .description = core::TrText("Par moments, vos lames s'enduisent : vos dégâts augmentent."),
         .archetype = SkillArchetype::kStance,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kPoison,
         .levels = kEnchantPoisonLevels,
         .stance_stat = SurvivorsStat::kDamagePct},
        {.skill_id = kImproveDodgeId,
         .key = "improve_dodge",
         .description = core::TrText("Par moments, vous filez : votre vitesse de marche augmente."),
         .archetype = SkillArchetype::kStance,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kImproveDodgeLevels,
         .stance_stat = SurvivorsStat::kWalkPct},
        {.skill_id = kHidingId,
         .key = "hiding",
         .description = core::TrText("Blessé, vous vous terrez un instant : rien ne vous touche, "
                                     "mais vous ne frappez plus."),
         .archetype = SkillArchetype::kReflex,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kHidingLevels},
        {.skill_id = kSonicBlowId,
         .key = "sonic_blow",
         .description = core::TrText("Huit coups s'abattent presque ensemble sur le plus proche."),
         .archetype = SkillArchetype::kTargetStrike,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kSonicBlowLevels},
        {.skill_id = kSoulDestroyerId,
         .key = "soul_destroyer",
         .description =
             core::TrText("Une lame d'énergie, très forte, part droit sur le plus proche."),
         .archetype = SkillArchetype::kLinear,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kSoulDestroyerLevels},
        {.skill_id = kVenomSplasherId,
         .key = "venom_splasher",
         .description = core::TrText("Le poison éclate là où les monstres se pressent."),
         .archetype = SkillArchetype::kClusterBlast,
         .target = TargetRule::kDensest,
         .element = SurvivorsElement::kPoison,
         .levels = kVenomSplasherLevels},
        {.skill_id = kEnchantDeadlyPoisonId,
         .key = "enchant_deadly_poison",
         .description = core::TrText("Par moments, un venin mortel : vos dégâts explosent."),
         .archetype = SkillArchetype::kStance,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kPoison,
         .levels = kEnchantDeadlyPoisonLevels,
         .stance_stat = SurvivorsStat::kDamagePct},
        {.skill_id = kCloakingId,
         .key = "cloaking",
         .description = core::TrText(
             "Blessé, vous disparaissez sans cesser de frapper, et votre premier coup en sortant "
             "est un critique."),
         .archetype = SkillArchetype::kReflex,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kCloakingLevels},
        {.skill_id = kThrowVenomKnifeId,
         .key = "throw_venom_knife",
         .description = core::TrText("Des couteaux droits traversent la file et l'empoisonnent."),
         .archetype = SkillArchetype::kLinear,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kPoison,
         .levels = kThrowVenomKnifeLevels},
        {.skill_id = kMeteorAssaultId,
         .key = "meteor_assault",
         .description = core::TrText("Une onde sombre jaillit de vous et balaie la horde."),
         .archetype = SkillArchetype::kNova,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kMeteorAssaultLevels},
    }};

constexpr std::array<EvolutionRule, kThiefEvolvedSkillCount> kThiefEvolutions{{
    {.skill_id = kDoubleAttackId, .card_item_id = kHornetCardId, .evolved_skill_id = kSonicBlowId},
    {.skill_id = kGrimtoothId,
     .card_item_id = kDragonFlyCardId,
     .evolved_skill_id = kSoulDestroyerId},
    {.skill_id = kVenomDustId, .card_item_id = kSporeCardId, .evolved_skill_id = kVenomSplasherId},
    {.skill_id = kEnchantPoisonId,
     .card_item_id = kVagabondWolfCardId,
     .evolved_skill_id = kEnchantDeadlyPoisonId},
    {.skill_id = kHidingId, .card_item_id = kEclipseCardId, .evolved_skill_id = kCloakingId},
    {.skill_id = kThrowStoneId,
     .card_item_id = kThiefBugCardId,
     .evolved_skill_id = kThrowVenomKnifeId},
    // L'UNION : Envenom et Sprinkle Sand, tous deux au plus haut niveau.
    {.skill_id = kEnvenomId,
     .card_item_id = 0,
     .evolved_skill_id = kMeteorAssaultId,
     .partner_skill_id = kSprinkleSandId},
}};

constexpr std::array<std::uint16_t, 3> kThiefStarterSkills{kDoubleAttackId, kEnvenomId,
                                                           kThrowStoneId};
constexpr std::array<std::uint16_t, kThiefPoolSkillCount> kThiefSkillPool{
    kDoubleAttackId, kEnvenomId,   kThrowStoneId,    kSprinkleSandId, kBackSlideId,
    kGrimtoothId,    kVenomDustId, kEnchantPoisonId, kImproveDodgeId, kHidingId};

// La carte Smokie : ses coups ont une chance d'être critiques, et comptent double.
constexpr std::array<std::int32_t, kMostCardLevels> kSmokieLevels{6, 12, 18, 24, 30};
// La carte Whisper : une part des coups le manque.
constexpr std::array<std::int32_t, kMostCardLevels> kWhisperLevels{7, 14, 21, 28, 35};
constexpr std::array<CardRule, kThiefCardCount> kThiefCards{{
    {.card_item_id = kSmokieCardId,
     .key = "smokie_card",
     .description = core::TrText("Chance de critique : certains coups comptent double."),
     .stat = SurvivorsStat::kCritPct,
     .per_level = kSmokieLevels},
    {.card_item_id = kWhisperCardId,
     .key = "whisper_card",
     .description = core::TrText("Chance d'esquive : certains coups le manquent."),
     .stat = SurvivorsStat::kDodgePct,
     .per_level = kWhisperLevels},
}};

// Les cartes sont celles du Magicien, Chonchon comprise, plus les siennes.
constexpr std::array<std::uint16_t, 14> kThiefCardPool{
    kHornetCardId,       kPupaCardId,     kChonchonCardId,  kPoringCardId,  kSporeCardId,
    kThiefBugCardId,     kGolemCardId,    kMasteringCardId, kEclipseCardId, kDragonFlyCardId,
    kVagabondWolfCardId, kBaphometCardId, kSmokieCardId,    kWhisperCardId};

constexpr ClassRule kThief{.class_id = kThiefClassId,
                           .key = "thief",
                           .job_id = kThiefJobId,
                           .weapon_item_id = kThiefWeaponItemId,
                           .max_hp = kThiefMaxHp,
                           .walk_ms_per_cell = kThiefWalkMsPerCell,
                           .magnet_units = CellTenths(kThiefMagnetTenths),
                           .damage_pct_per_level = kThiefDamagePctPerLevel,
                           .damage_pct_cap = kThiefDamagePctCap,
                           .starter_skills = kThiefStarterSkills,
                           .skill_pool = kThiefSkillPool,
                           .card_pool = kThiefCardPool,
                           .evolved_job_id = kAssassinJobId,
                           .evolved_weapon_item_id = kAssassinWeaponItemId,
                           .melee = true,
                           .reborn_level = kThiefRebornLevel,
                           .reborn_job_id = kAssassinCrossJobId,
                           .reborn_damage_pct = kThiefRebornDamagePct,
                           .final_boss_hp_bonus_pct = kThiefFinalBossHpBonusPct};

}  // namespace

const ClassRule& ThiefClass() { return kThief; }

std::span<const SurvivorsSkillRule> ThiefSkills() { return kThiefSkills; }

std::span<const CardRule> ThiefCards() { return kThiefCards; }

std::span<const EvolutionRule> ThiefEvolutions() { return kThiefEvolutions; }

}  // namespace ro::sim::survivors

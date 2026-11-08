#include <array>
#include <cstdint>
#include <span>

#include "ro/core/i18n.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "survivors_shipped.h"

namespace ro::sim::survivors {

namespace {

// ── LE MARCHAND : sa classe, ses skills et ses évolutions ──────────────────
// Il ne se bat pas seul : il frappe de près, à la hache et au chariot, et ses
// invocations frappent pour lui — des plantes qui mordent, des sphères qui
// sautent, un homoncule à ses côtés. Lui seul soigne l'équipe. Marchand au
// départ, il devient Alchimiste à sa première évolution, et Créateur à sa
// renaissance.

constexpr std::uint16_t kMerchantClassId = 5;
constexpr std::uint16_t kMerchantJobId = 5;             // l'apparence du départ
constexpr std::uint16_t kAlchemistJobId = 18;           // après une évolution
constexpr std::uint16_t kCreatorJobId = 4019;           // rené
constexpr std::uint16_t kMerchantWeaponItemId = 1301;   // Axe
constexpr std::uint16_t kAlchemistWeaponItemId = 1305;  // Cleaver
constexpr std::int32_t kMerchantMaxHp = 650;
constexpr std::uint32_t kMerchantWalkMsPerCell = 155;
// C'est un marchand : il ramasse le plus loin du mode.
constexpr std::int32_t kMerchantMagnetTenths = 35;
constexpr std::int32_t kMerchantDamagePctPerLevel = 1;
constexpr std::int32_t kMerchantDamagePctCap = 25;
constexpr std::uint16_t kMerchantRebornLevel = 50;
constexpr std::int32_t kMerchantRebornDamagePct = 5;
constexpr std::int32_t kMerchantFinalBossHpBonusPct = 50;

// Mammonite : un coup de hache, à portée de bras.
constexpr std::int32_t kAxeReachTenths = 32;
constexpr std::uint32_t kAxeIntervalMs = 150;

// LE CHARIOT QUI TOURNE : un seul orbe, le chariot, qui fait un tour et
// repousse ce qu'il frappe.
constexpr std::int32_t kCartHitRadiusTenths = 9;
constexpr std::uint32_t kCartRevolutionTurnMs = 500;
constexpr std::int32_t kCartRevolutionRadiusTenths = 20;

// Acid Terror : la fiole lancée droit, qui fait saigner.
constexpr std::int32_t kAcidReachCells = 8;
constexpr std::int32_t kAcidSpeedCellsPerSecond = 14;
constexpr std::int32_t kAcidHitRadiusTenths = 5;
constexpr std::uint32_t kBleedMs = 3000;

// Demonstration : la bombe incendiaire, qui brûle par vagues.
constexpr std::int32_t kBombReachCells = 6;
constexpr std::uint32_t kBombIntervalMs = 500;

// Sphere Mine : une Marine Sphere posée près du héros, qui saute quand la horde
// la touche.
constexpr std::int32_t kSphereCircleCells = 1;
constexpr std::uint32_t kSphereLifeMs = 10000;
constexpr std::uint32_t kSphereRehitMs = 1000;
constexpr std::uint8_t kSphereHits = 1;

// Bio Cannibalize : des plantes posées autour du héros, qui mordent ce qui
// passe à leur portée.
constexpr std::uint16_t kMandragoraClass = 1020;
constexpr std::uint16_t kHydraClass = 1068;
constexpr std::uint16_t kFloraClass = 1118;
constexpr std::uint16_t kParasiteClass = 1155;
constexpr std::uint16_t kGeographerClass = 1368;
constexpr std::uint32_t kPlantLifeMs = 15000;
constexpr std::uint32_t kPlantCooldownMs = 2500;
constexpr std::int32_t kPlantReachTenths = 25;
constexpr std::int32_t kParasiteReachCells = 6;

// Potion Pitcher : la potion lancée au plus blessé.
constexpr std::int32_t kPotionReachCells = 6;

// Un niveau de Mammonite : `amount` coups de `damage`.
[[nodiscard]] constexpr SkillLevel Axe(std::uint8_t amount, std::int32_t damage,
                                       std::uint32_t cooldown_ms) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .interval_ms = kAxeIntervalMs,
                    .reach_units = CellTenths(kAxeReachTenths)};
}

// Un niveau du chariot qui tourne : `damage` à chaque monstre frappé, sur un
// cercle de `radius_tenths` dixièmes de case, un tour en `turn_ms`, allumé
// `active_ms` puis éteint `rest_ms`, qui repousse de `knockback_tenths`.
[[nodiscard]] constexpr SkillLevel Cart(std::int32_t damage, std::int32_t radius_tenths,
                                        std::uint32_t turn_ms, std::uint32_t active_ms,
                                        std::uint32_t rest_ms,
                                        std::int32_t knockback_tenths) noexcept {
  return SkillLevel{.damage = damage,
                    .amount = 1,
                    .reach_units = CellTenths(radius_tenths),
                    .hit_radius_units = CellTenths(kCartHitRadiusTenths),
                    .life_ms = active_ms,
                    .rest_ms = rest_ms,
                    .period_ms = turn_ms,
                    .rehit_ms = turn_ms,
                    .knockback_units = CellTenths(knockback_tenths)};
}

// Un niveau d'Acid Terror : `amount` fioles de `damage`, qui font saigner de
// `bleed_damage` par tic.
[[nodiscard]] constexpr SkillLevel Acid(std::uint8_t amount, std::int32_t damage,
                                        std::uint32_t cooldown_ms,
                                        std::int32_t bleed_damage) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .reach_units = Cells(kAcidReachCells),
                    .speed_units_per_s = Cells(kAcidSpeedCellsPerSecond),
                    .hit_radius_units = CellTenths(kAcidHitRadiusTenths),
                    .poison_ms = kBleedMs,
                    .poison_tick_damage = bleed_damage};
}

// Un niveau de Demonstration : `waves` vagues de `damage`, de rayon
// `radius_tenths` dixièmes de case.
[[nodiscard]] constexpr SkillLevel Bomb(std::uint8_t waves, std::int32_t damage,
                                        std::uint32_t cooldown_ms,
                                        std::int32_t radius_tenths) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = waves,
                    .interval_ms = kBombIntervalMs,
                    .reach_units = Cells(kBombReachCells),
                    .area_radius_units = CellTenths(radius_tenths)};
}

// Un niveau de Sphere Mine : `damage` sur une aire de `radius_tenths` dixièmes
// de case, qui repousse de `knockback_tenths`.
[[nodiscard]] constexpr SkillLevel Sphere(std::int32_t damage, std::uint32_t cooldown_ms,
                                          std::int32_t radius_tenths,
                                          std::int32_t knockback_tenths) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .reach_units = Cells(kSphereCircleCells),
                    .life_ms = kSphereLifeMs,
                    .rehit_ms = kSphereRehitMs,
                    .area_radius_units = CellTenths(radius_tenths),
                    .zone_hits = kSphereHits,
                    .knockback_units = CellTenths(knockback_tenths)};
}

// Un niveau de Bio Cannibalize : `amount` plantes `plant` à la fois, qui
// mordent de `damage` toutes les `bite_ms` à `reach_units`, et soignent de
// `heal_pct` %.
[[nodiscard]] constexpr SkillLevel Plant(std::uint16_t plant, std::uint8_t amount,
                                         std::int32_t damage, std::uint32_t bite_ms,
                                         std::int32_t reach_units, std::int32_t heal_pct) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = kPlantCooldownMs,
                    .amount = amount,
                    .interval_ms = bite_ms,
                    .reach_units = reach_units,
                    .life_ms = kPlantLifeMs,
                    .summon_class = plant,
                    .heal_pct = heal_pct};
}

// Un niveau de Potion Pitcher : `heal_pct` % des PV max du plus blessé.
[[nodiscard]] constexpr SkillLevel Potion(std::int32_t heal_pct,
                                          std::uint32_t cooldown_ms) noexcept {
  return SkillLevel{
      .cooldown_ms = cooldown_ms, .reach_units = Cells(kPotionReachCells), .heal_pct = heal_pct};
}

// Un niveau de posture : `bonus` à sa statistique pendant `life_ms`, toutes les
// `cooldown_ms`.
[[nodiscard]] constexpr SkillLevel Stance(std::uint32_t life_ms, std::uint32_t cooldown_ms,
                                          std::int32_t bonus) noexcept {
  return SkillLevel{.cooldown_ms = cooldown_ms, .life_ms = life_ms, .stance_bonus = bonus};
}

// Axe(coups, dégâts, recharge en ms) : le coup de hache de la cupidité.
constexpr std::array<SkillLevel, kMostSkillLevels> kMammoniteLevels{{
    Axe(1, 40, 1100),
    Axe(1, 48, 1100),
    Axe(1, 56, 1050),
    Axe(1, 64, 1000),
    Axe(2, 56, 1000),
    Axe(2, 64, 950),
    Axe(2, 74, 900),
    Axe(2, 86, 850),
}};

// Cart(dégâts, rayon en dixièmes, tour en ms, allumé, éteint, recul en
// dixièmes) : un seul tour vif, qui repousse.
constexpr std::array<SkillLevel, kMostSkillLevels> kCartRevolutionLevels{{
    Cart(30, kCartRevolutionRadiusTenths, kCartRevolutionTurnMs, 500, 2200, 15),
    Cart(36, kCartRevolutionRadiusTenths, kCartRevolutionTurnMs, 500, 2100, 15),
    Cart(36, kCartRevolutionRadiusTenths, kCartRevolutionTurnMs, 500, 2000, 18),
    Cart(43, kCartRevolutionRadiusTenths, kCartRevolutionTurnMs, 500, 1900, 18),
    Cart(43, 22, kCartRevolutionTurnMs, 500, 1800, 20),
    Cart(52, 22, kCartRevolutionTurnMs, 500, 1700, 20),
    Cart(60, 22, kCartRevolutionTurnMs, 500, 1600, 22),
    Cart(70, 24, kCartRevolutionTurnMs, 500, 1500, 25),
}};

// Acid(fioles, dégâts, recharge en ms, saignement par tic).
constexpr std::array<SkillLevel, kMostSkillLevels> kAcidTerrorLevels{{
    Acid(1, 28, 1300, 6),
    Acid(1, 34, 1250, 7),
    Acid(1, 40, 1200, 8),
    Acid(2, 34, 1150, 9),
    Acid(2, 40, 1100, 11),
    Acid(2, 47, 1000, 12),
    Acid(3, 47, 950, 14),
    Acid(3, 56, 900, 16),
}};

// Crazy Uproar(durée en ms, recharge en ms, dégâts en %) : le cri du marchand.
constexpr std::array<SkillLevel, kMostSkillLevels> kCrazyUproarLevels{{
    Stance(5000, 16000, 15),
    Stance(5500, 15500, 18),
    Stance(6000, 15000, 20),
    Stance(6500, 14500, 23),
    Stance(7000, 14000, 26),
    Stance(7500, 13500, 29),
    Stance(8000, 13000, 32),
    Stance(8000, 12000, 35),
}};

// Bomb(vagues, dégâts, recharge en ms, rayon en dixièmes).
constexpr std::array<SkillLevel, kMostSkillLevels> kDemonstrationLevels{{
    Bomb(3, 14, 3600, 18),
    Bomb(3, 17, 3500, 18),
    Bomb(4, 17, 3400, 20),
    Bomb(4, 21, 3200, 20),
    Bomb(5, 21, 3100, 22),
    Bomb(5, 25, 3000, 22),
    Bomb(6, 25, 2800, 24),
    Bomb(6, 31, 2600, 25),
}};

// Sphere(dégâts, recharge en ms, rayon en dixièmes, recul en dixièmes).
constexpr std::array<SkillLevel, kMostSkillLevels> kSphereMineLevels{{
    Sphere(48, 2600, 20, 10),
    Sphere(58, 2500, 20, 10),
    Sphere(58, 2400, 22, 12),
    Sphere(70, 2300, 22, 12),
    Sphere(70, 2200, 24, 15),
    Sphere(84, 2000, 25, 15),
    Sphere(100, 1800, 26, 18),
    Sphere(120, 1600, 28, 20),
}};

// Plant(espèce, à la fois, dégâts, morsure en ms, portée, soin en %) : la
// Mandragora d'abord, puis l'Hydra, la Flora, la Parasite qui tire de loin, et
// la Geographer qui soigne.
constexpr std::array<SkillLevel, kMostSkillLevels> kBioCannibalizeLevels{{
    Plant(kMandragoraClass, 1, 12, 900, CellTenths(kPlantReachTenths), 0),
    Plant(kMandragoraClass, 2, 12, 900, CellTenths(kPlantReachTenths), 0),
    Plant(kHydraClass, 2, 16, 850, CellTenths(kPlantReachTenths), 0),
    Plant(kHydraClass, 2, 20, 800, CellTenths(kPlantReachTenths), 0),
    Plant(kFloraClass, 3, 20, 800, CellTenths(kPlantReachTenths), 0),
    Plant(kFloraClass, 3, 25, 750, CellTenths(kPlantReachTenths), 0),
    Plant(kParasiteClass, 3, 25, 800, Cells(kParasiteReachCells), 0),
    Plant(kGeographerClass, 3, 30, 700, Cells(kParasiteReachCells), 3),
}};

// Potion(soin en %, recharge en ms).
constexpr std::array<SkillLevel, kMostSkillLevels> kPotionPitcherLevels{{
    Potion(6, 6000),
    Potion(7, 5800),
    Potion(8, 5600),
    Potion(9, 5400),
    Potion(10, 5200),
    Potion(11, 5000),
    Potion(13, 4600),
    Potion(15, 4000),
}};

// Chemical Protection Armor(durée en ms, recharge en ms, armure) : comme
// Endure, un peu plus.
constexpr std::array<SkillLevel, kMostSkillLevels> kChemicalProtectionArmorLevels{{
    Stance(6000, 16000, 2),
    Stance(6500, 15500, 3),
    Stance(7000, 15000, 3),
    Stance(7500, 14500, 4),
    Stance(8000, 14000, 4),
    Stance(8500, 13500, 5),
    Stance(9000, 13000, 6),
    Stance(9000, 12000, 7),
}};

// ── Les évolués : un niveau chacun, que seul un coffre donne ──────────────

// Acid Demonstration : l'acide et la bombe, d'un seul coup sur le plus fort.
constexpr std::array<SkillLevel, 1> kAcidDemonstrationLevels{{
    {.damage = 360,
     .cooldown_ms = 1600,
     .amount = 2,
     .interval_ms = 250,
     .reach_units = Cells(8),
     .poison_ms = 4000,
     .poison_tick_damage = 30},
}};

// Slim Potion Pitcher : toute l'équipe à portée reprend des PV.
constexpr std::array<SkillLevel, 1> kSlimPotionPitcherLevels{{
    Potion(20, 3500),
}};

// Full Chemical Protection(durée en ms, recharge en ms, armure).
constexpr std::array<SkillLevel, 1> kFullChemicalProtectionLevels{{
    Stance(10000, 13000, 12),
}};

// Plant Cultivation : un jardin de Geographer, qui mordent et soignent.
constexpr std::array<SkillLevel, 1> kPlantCultivationLevels{{
    Plant(kGeographerClass, 5, 48, 600, Cells(kParasiteReachCells), 4),
}};

// Cart Cannon : le boulet tiré du chariot sur la foule la plus dense.
constexpr std::array<SkillLevel, 1> kCartCannonLevels{{
    {.damage = 150,
     .cooldown_ms = 1200,
     .amount = 2,
     .interval_ms = 200,
     .reach_units = Cells(9),
     .area_radius_units = CellTenths(25),
     .splash_pct = 70,
     .knockback_units = Cells(1)},
}};

// Cart Tornado : le chariot tourne trois fois, plus large, plus vite.
constexpr std::array<SkillLevel, 1> kCartTornadoLevels{{
    Cart(110, 35, 600, 1800, 1400, 30),
}};

constexpr std::array<SurvivorsSkillRule,
                     kMerchantPoolSkillCount + kMerchantEvolvedSkillCount - kHomunculusSkillCount>
    kMerchantSkills{{
        {.skill_id = kMammoniteId,
         .key = "mammonite",
         .description = core::TrText("Un coup de hache puissant frappe le monstre le plus proche."),
         .archetype = SkillArchetype::kTargetStrike,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kMammoniteLevels},
        {.skill_id = kCartRevolutionId,
         .key = "cart_revolution",
         .description =
             core::TrText("Votre chariot tourne autour de vous et repousse ce qu'il frappe."),
         .archetype = SkillArchetype::kOrbit,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kCartRevolutionLevels},
        {.skill_id = kAcidTerrorId,
         .key = "acid_terror",
         .description =
             core::TrText("Une fiole d'acide file sur le plus proche et le fait saigner."),
         .archetype = SkillArchetype::kLinear,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kAcidTerrorLevels},
        {.skill_id = kCrazyUproarId,
         .key = "crazy_uproar",
         .description = core::TrText("Par moments, votre cri vous galvanise : vos dégâts montent."),
         .archetype = SkillArchetype::kStance,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kCrazyUproarLevels,
         .stance_stat = SurvivorsStat::kDamagePct},
        {.skill_id = kDemonstrationId,
         .key = "demonstration",
         .description = core::TrText("Une bombe incendiaire brûle le sol, vague après vague."),
         .archetype = SkillArchetype::kStorm,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kFire,
         .levels = kDemonstrationLevels},
        {.skill_id = kSphereMineId,
         .key = "sphere_mine",
         .description = core::TrText("Une Marine Sphere posée près de vous saute sous la horde."),
         .archetype = SkillArchetype::kRing,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kSphereMineLevels},
        {.skill_id = kBioCannibalizeId,
         .key = "bio_cannibalize",
         .description =
             core::TrText("Des plantes poussent autour de vous et mordent ce qui passe."),
         .archetype = SkillArchetype::kSummon,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kBioCannibalizeLevels},
        {.skill_id = kPotionPitcherId,
         .key = "potion_pitcher",
         .description = core::TrText("Une potion vole vers le héros le plus blessé et le soigne."),
         .archetype = SkillArchetype::kMend,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kPotionPitcherLevels},
        {.skill_id = kChemicalProtectionArmorId,
         .key = "chemical_protection_armor",
         .description = core::TrText("Par moments, votre armure enduite encaisse mieux les coups."),
         .archetype = SkillArchetype::kStance,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kChemicalProtectionArmorLevels,
         .stance_stat = SurvivorsStat::kArmor},
        {.skill_id = kAcidDemonstrationId,
         .key = "acid_demonstration",
         .description = core::TrText("L'acide et la bombe, ensemble, écrasent le plus fort."),
         .archetype = SkillArchetype::kTargetStrike,
         .target = TargetRule::kStrongest,
         .element = SurvivorsElement::kNeutral,
         .levels = kAcidDemonstrationLevels},
        {.skill_id = kSlimPotionPitcherId,
         .key = "slim_potion_pitcher",
         .description = core::TrText("Une potion condensée soigne largement le plus blessé."),
         .archetype = SkillArchetype::kMend,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kSlimPotionPitcherLevels},
        {.skill_id = kFullChemicalProtectionId,
         .key = "full_chemical_protection",
         .description = core::TrText("Par moments, votre armure devient presque impénétrable."),
         .archetype = SkillArchetype::kStance,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kFullChemicalProtectionLevels,
         .stance_stat = SurvivorsStat::kArmor},
        {.skill_id = kPlantCultivationId,
         .key = "plant_cultivation",
         .description = core::TrText("Un jardin de Geographer mord la horde et vous soigne."),
         .archetype = SkillArchetype::kSummon,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kPlantCultivationLevels},
        {.skill_id = kCartCannonId,
         .key = "cart_cannon",
         .description =
             core::TrText("Un boulet tiré du chariot éclate sur la foule la plus dense."),
         .archetype = SkillArchetype::kClusterBlast,
         .target = TargetRule::kDensest,
         .element = SurvivorsElement::kNeutral,
         .levels = kCartCannonLevels},
        {.skill_id = kCartTornadoId,
         .key = "cart_tornado",
         .description = core::TrText("Votre chariot tourne en tornade et balaie la horde au loin."),
         .archetype = SkillArchetype::kOrbit,
         .target = TargetRule::kNearest,
         .element = SurvivorsElement::kNeutral,
         .levels = kCartTornadoLevels},
    }};

constexpr std::array<EvolutionRule, kMerchantEvolvedSkillCount> kMerchantEvolutions{{
    // L'UNION : la fiole d'Acid Terror lancée avec la bombe de Demonstration.
    {.skill_id = kAcidTerrorId,
     .card_item_id = 0,
     .evolved_skill_id = kAcidDemonstrationId,
     .partner_skill_id = kDemonstrationId},
    {.skill_id = kPotionPitcherId,
     .card_item_id = kMukaCardId,
     .evolved_skill_id = kSlimPotionPitcherId},
    {.skill_id = kChemicalProtectionArmorId,
     .card_item_id = kIceTitanCardId,
     .evolved_skill_id = kFullChemicalProtectionId},
    {.skill_id = kBioCannibalizeId,
     .card_item_id = kCookieCardId,
     .evolved_skill_id = kPlantCultivationId},
    {.skill_id = kMammoniteId, .card_item_id = kMarinCardId, .evolved_skill_id = kCartCannonId},
    {.skill_id = kCartRevolutionId,
     .card_item_id = kAndreCardId,
     .evolved_skill_id = kCartTornadoId},
    // UN SEUL HOMONCULE VIT : une seule carte les fait tous évoluer.
    {.skill_id = kMoonlightId, .card_item_id = kSasquatchCardId, .evolved_skill_id = kSbr44Id},
    {.skill_id = kCapriceId, .card_item_id = kSasquatchCardId, .evolved_skill_id = kBioExplosionId},
    {.skill_id = kTouchOfHealId,
     .card_item_id = kSasquatchCardId,
     .evolved_skill_id = kMentalChangeId},
    {.skill_id = kCastlingId, .card_item_id = kSasquatchCardId, .evolved_skill_id = kBloodLustId},
}};

constexpr std::array<std::uint16_t, 3> kMerchantStarterSkills{kMammoniteId, kCartRevolutionId,
                                                              kAcidTerrorId};
constexpr std::array<std::uint16_t, kMerchantPoolSkillCount> kMerchantSkillPool{
    kMammoniteId,      kCartRevolutionId, kAcidTerrorId,
    kCrazyUproarId,    kDemonstrationId,  kSphereMineId,
    kBioCannibalizeId, kPotionPitcherId,  kChemicalProtectionArmorId,
    kMoonlightId,      kCapriceId,        kTouchOfHealId,
    kCastlingId};

// Andre : ses coups portent plus, comme Hornet.
constexpr std::array<std::int32_t, kMostCardLevels> kAndreLevels{6, 12, 18, 24, 30};
// Muka : il récupère des PV, comme Spore.
constexpr std::array<std::int32_t, kMostCardLevels> kMukaLevels{30, 60, 90, 120, 150};
// Ice Titan : il encaisse mieux.
constexpr std::array<std::int32_t, kMostCardLevels> kIceTitanLevels{1, 2, 3, 4, 5};
// Marin : chaque Gold ramassé vaut plus.
constexpr std::array<std::int32_t, kMostCardLevels> kMarinLevels{20, 40, 60, 80, 100};
// Sasquatch : ce qui le touche gèle, par chance.
constexpr std::array<std::int32_t, kMostCardLevels> kSasquatchLevels{5, 8, 11, 14, 18};
// Cookie : ses invocations frappent plus.
constexpr std::array<std::int32_t, kMostCardLevels> kCookieLevels{10, 20, 30, 40, 50};
constexpr std::array<CardRule, kMerchantCardCount> kMerchantCards{{
    {.card_item_id = kAndreCardId,
     .key = "andre_card",
     .description = core::TrText("Dégâts augmentés."),
     .stat = SurvivorsStat::kDamagePct,
     .per_level = kAndreLevels},
    {.card_item_id = kMukaCardId,
     .key = "muka_card",
     .description = core::TrText("Récupération : vous regagnez des PV au fil du temps."),
     .stat = SurvivorsStat::kRegenPerMinute,
     .per_level = kMukaLevels},
    {.card_item_id = kIceTitanCardId,
     .key = "ice_titan_card",
     .description = core::TrText("Armure augmentée : chaque coup reçu blesse moins."),
     .stat = SurvivorsStat::kArmor,
     .per_level = kIceTitanLevels},
    {.card_item_id = kMarinCardId,
     .key = "marin_card",
     .description = core::TrText("Plus de zeny : chaque Gold ramassé vaut davantage."),
     .stat = SurvivorsStat::kZenyPct,
     .per_level = kMarinLevels},
    {.card_item_id = kSasquatchCardId,
     .key = "sasquatch_card",
     .description = core::TrText("Riposte de gel : un monstre qui vous touche peut geler."),
     .stat = SurvivorsStat::kFreezeRiposteChancePct,
     .per_level = kSasquatchLevels},
    {.card_item_id = kCookieCardId,
     .key = "cookie_card",
     .description =
         core::TrText("Invocations : vos plantes et votre homoncule frappent plus fort."),
     .stat = SurvivorsStat::kSummonDamagePct,
     .per_level = kCookieLevels},
}};

// Les cartes du Magicien qui n'ouvrent rien chez lui, puis les siennes, qu'il
// prête à ses coéquipiers.
constexpr std::array<std::uint16_t, 12> kMerchantCardPool{
    kAndreCardId,     kPupaCardId,     kMukaCardId,   kPoringCardId,
    kIceTitanCardId,  kThiefBugCardId, kMarinCardId,  kMasteringCardId,
    kSasquatchCardId, kGolemCardId,    kCookieCardId, kBaphometCardId};
constexpr std::array<std::uint16_t, kMerchantCardCount> kMerchantLentCards{
    kAndreCardId, kMukaCardId, kIceTitanCardId, kMarinCardId, kSasquatchCardId, kCookieCardId};

constexpr ClassRule kMerchant{.class_id = kMerchantClassId,
                              .key = "merchant",
                              .job_id = kMerchantJobId,
                              .weapon_item_id = kMerchantWeaponItemId,
                              .max_hp = kMerchantMaxHp,
                              .walk_ms_per_cell = kMerchantWalkMsPerCell,
                              .magnet_units = CellTenths(kMerchantMagnetTenths),
                              .damage_pct_per_level = kMerchantDamagePctPerLevel,
                              .damage_pct_cap = kMerchantDamagePctCap,
                              .starter_skills = kMerchantStarterSkills,
                              .skill_pool = kMerchantSkillPool,
                              .card_pool = kMerchantCardPool,
                              .evolved_job_id = kAlchemistJobId,
                              .evolved_weapon_item_id = kAlchemistWeaponItemId,
                              .attack_gesture = true,
                              .reborn_level = kMerchantRebornLevel,
                              .reborn_job_id = kCreatorJobId,
                              .reborn_damage_pct = kMerchantRebornDamagePct,
                              .final_boss_hp_bonus_pct = kMerchantFinalBossHpBonusPct,
                              .lent_cards = kMerchantLentCards,
                              // Ses invocations frappent hors de sa main : le
                              // serveur ne les relit pas encore.
                              .free_play_only = true};

}  // namespace

const ClassRule& MerchantClass() { return kMerchant; }

std::span<const SurvivorsSkillRule> MerchantSkills() { return kMerchantSkills; }

std::span<const CardRule> MerchantCards() { return kMerchantCards; }

std::span<const EvolutionRule> MerchantEvolutions() { return kMerchantEvolutions; }

}  // namespace ro::sim::survivors

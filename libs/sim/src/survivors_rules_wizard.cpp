#include <array>
#include <cstdint>
#include <span>

#include "ro/core/i18n.h"
#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "survivors_shipped.h"

namespace ro::sim::survivors {

namespace {

// ── LES ÉVOLUTIONS DU MAGE ──────────────────────────────────────────────────
// Un skill au niveau 8 et sa carte possédée, à l'ouverture d'un coffre après
// la sixième minute, laissent place à l'évolué. Un évolué n'a qu'un niveau, et
// n'entre dans aucune réserve : seul un coffre le donne.

constexpr std::uint16_t kMeteorStormId = 83;
constexpr std::uint16_t kSightrasherId = 81;
constexpr std::uint16_t kFirePillarId = 80;
constexpr std::uint16_t kFrostNovaId = 88;
constexpr std::uint16_t kStormGustId = 89;
constexpr std::uint16_t kJupitelThunderId = 84;
constexpr std::uint16_t kLordOfVermilionId = 85;
constexpr std::uint16_t kHeavensDriveId = 91;
constexpr std::uint16_t kNapalmVulcanId = 400;

// Meteor Storm : cinq météores sur les plus denses, pleins dans tout l'éclat.
constexpr std::array<SkillLevel, 1> kMeteorStormLevels{{
    {.damage = 32,
     .cooldown_ms = 2500,
     .amount = 5,
     .interval_ms = 200,
     .reach_units = Cells(10),
     .area_radius_units = CellTenths(25),
     .splash_pct = 100},
}};

// Sightrasher : six orbes allumées en permanence, qui repoussent. Une orbite
// sans repos garde une phase active non nulle, que le contrôle exige.
constexpr std::array<SkillLevel, 1> kSightrasherLevels{{
    {.damage = 22,
     .amount = 6,
     .reach_units = Cells(3),
     .hit_radius_units = CellTenths(6),
     .life_ms = 4000,
     .rest_ms = 0,
     .period_ms = 1600,
     .rehit_ms = 400,
     .knockback_units = Cells(2)},
}};

// Fire Pillar : huit piliers en cercle, chacun réarmé une seconde après son
// coup, jusqu'à la fin de sa vie.
constexpr std::array<SkillLevel, 1> kFirePillarLevels{{
    {.damage = 25,
     .cooldown_ms = 6000,
     .amount = 8,
     .reach_units = Cells(3),
     .life_ms = 6000,
     .rehit_ms = 1000,
     .area_radius_units = Cells(1)},
}};

// Frost Nova : une aire autour du héros, qui gèle.
constexpr std::array<SkillLevel, 1> kFrostNovaLevels{{
    {.damage = 30,
     .cooldown_ms = 3000,
     .amount = 1,
     .area_radius_units = Cells(4),
     .freeze_ms = 2500},
}};

// Storm Gust : un blizzard sur la foule la plus dense, huit vagues, qui gèlent
// à partir de la troisième.
constexpr std::array<SkillLevel, 1> kStormGustLevels{{
    {.damage = 30,
     .cooldown_ms = 4000,
     .amount = 8,
     .interval_ms = 400,
     .reach_units = Cells(9),
     .area_radius_units = Cells(3),
     .freeze_ms = 2000,
     .freeze_from_wave = 3},
}};

// Jupitel Thunder : quatre boules de foudre droites, qui traversent et
// repoussent loin.
constexpr std::array<SkillLevel, 1> kJupitelThunderLevels{{
    {.damage = 60,
     .cooldown_ms = 1200,
     .amount = 4,
     .reach_units = Cells(10),
     .speed_units_per_s = Cells(14),
     .hit_radius_units = CellTenths(8),
     .pierce = 3,
     .knockback_units = Cells(3)},
}};

// Lord of Vermilion : la foudre sur tout ce qui est à 9 cases du héros, cinq
// vagues, qui aveuglent.
constexpr std::array<SkillLevel, 1> kLordOfVermilionLevels{{
    {.damage = 45,
     .cooldown_ms = 5000,
     .amount = 5,
     .interval_ms = 500,
     .area_radius_units = Cells(9),
     .blind_ms = 3000},
}};

// Heaven's Drive : trois salves de pointes de terre autour du héros.
constexpr std::array<SkillLevel, 1> kHeavensDriveLevels{{
    {.damage = 60,
     .cooldown_ms = 3000,
     .amount = 3,
     .interval_ms = 250,
     .area_radius_units = Cells(5)},
}};

// Napalm Vulcan : six esprits explosifs qui poursuivent, traversent un
// monstre de plus, et maudissent.
constexpr std::array<SkillLevel, 1> kNapalmVulcanLevels{{
    {.damage = 36,
     .cooldown_ms = 1000,
     .amount = 6,
     .reach_units = Cells(10),
     .speed_units_per_s = Cells(9),
     .hit_radius_units = CellTenths(5),
     .life_ms = 2500,
     .pierce = 1,
     .curse_ms = 3000},
}};

constexpr std::array<SurvivorsSkillRule, kShippedEvolvedSkillCount> kEvolvedSkills{{
    {.skill_id = kMeteorStormId,
     .key = "meteor_storm",
     .description = core::TrText("Des météores s'abattent là où les monstres se pressent."),
     .archetype = SkillArchetype::kClusterBlast,
     .target = TargetRule::kDensest,
     .element = SurvivorsElement::kFire,
     .levels = kMeteorStormLevels},
    {.skill_id = kSightrasherId,
     .key = "sightrasher",
     .description = core::TrText("Six boules de feu tournent sans cesse et repoussent."),
     .archetype = SkillArchetype::kOrbit,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kFire,
     .levels = kSightrasherLevels},
    {.skill_id = kFirePillarId,
     .key = "fire_pillar",
     .description = core::TrText("Un cercle de piliers de feu brûle qui s'en approche."),
     .archetype = SkillArchetype::kRing,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kFire,
     .levels = kFirePillarLevels},
    {.skill_id = kFrostNovaId,
     .key = "frost_nova",
     .description = core::TrText("Une onde de glace gèle les monstres autour de vous."),
     .archetype = SkillArchetype::kNova,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kWater,
     .levels = kFrostNovaLevels},
    {.skill_id = kStormGustId,
     .key = "storm_gust",
     .description = core::TrText("Un blizzard tourbillonne sur la foule et finit par la geler."),
     .archetype = SkillArchetype::kStorm,
     .target = TargetRule::kDensest,
     .element = SurvivorsElement::kWater,
     .levels = kStormGustLevels},
    {.skill_id = kJupitelThunderId,
     .key = "jupitel_thunder",
     .description = core::TrText("Des boules de foudre filent droit et repoussent tout loin."),
     .archetype = SkillArchetype::kLinear,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kWind,
     .levels = kJupitelThunderLevels},
    {.skill_id = kLordOfVermilionId,
     .key = "lord_of_vermilion",
     .description = core::TrText("La foudre s'abat autour de vous en vagues et aveugle."),
     .archetype = SkillArchetype::kStorm,
     .target = TargetRule::kHero,
     .element = SurvivorsElement::kWind,
     .levels = kLordOfVermilionLevels},
    {.skill_id = kHeavensDriveId,
     .key = "heavens_drive",
     .description = core::TrText("Des pointes de terre jaillissent tout autour de vous."),
     .archetype = SkillArchetype::kNova,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kEarth,
     .levels = kHeavensDriveLevels},
    {.skill_id = kNapalmVulcanId,
     .key = "napalm_vulcan",
     .description =
         core::TrText("Des esprits explosifs poursuivent les monstres et les maudissent."),
     .archetype = SkillArchetype::kHoming,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kGhost,
     .levels = kNapalmVulcanLevels},
}};

constexpr std::array<EvolutionRule, kShippedEvolvedSkillCount> kEvolutions{{
    {.skill_id = kFireBoltId, .card_item_id = kHornetCardId, .evolved_skill_id = kMeteorStormId},
    {.skill_id = kSightId, .card_item_id = kChonchonCardId, .evolved_skill_id = kSightrasherId},
    {.skill_id = kFireWallId, .card_item_id = kGolemCardId, .evolved_skill_id = kFirePillarId},
    {.skill_id = kFrostDiverId, .card_item_id = kMasteringCardId, .evolved_skill_id = kFrostNovaId},
    {.skill_id = kColdBoltId, .card_item_id = kEclipseCardId, .evolved_skill_id = kStormGustId},
    {.skill_id = kLightningBoltId,
     .card_item_id = kDragonFlyCardId,
     .evolved_skill_id = kJupitelThunderId},
    {.skill_id = kThunderstormId,
     .card_item_id = kBaphometCardId,
     .evolved_skill_id = kLordOfVermilionId},
    {.skill_id = kEarthSpikeId, .card_item_id = kPupaCardId, .evolved_skill_id = kHeavensDriveId},
    // L'UNION : Soul Strike et Napalm Beat, tous deux au plus haut niveau.
    {.skill_id = kSoulStrikeId,
     .card_item_id = 0,
     .evolved_skill_id = kNapalmVulcanId,
     .partner_skill_id = kNapalmBeatId},
}};

}  // namespace

std::span<const SurvivorsSkillRule> ShippedEvolvedSkills() { return kEvolvedSkills; }

std::span<const EvolutionRule> ShippedEvolutions() { return kEvolutions; }

}  // namespace ro::sim::survivors

#include <array>
#include <cstdint>
#include <span>

#include "ro/core/i18n.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "survivors_shipped.h"

namespace ro::sim::survivors {

namespace {

// ── LES HOMONCULES DU MARCHAND : quatre espèces, et leurs formes évoluées ──
// Un seul vit à côté du héros. Ils le suivent, fondent sur ce qui entre dans
// sa laisse, et ne meurent pas : Filir frappe vite, Vanilmirth frappe de loin
// au hasard, Lif soigne, Amistr repousse et prend la place du héros en danger.

// LE GROUPE DES HOMONCULES : un seul vit à côté du héros.
constexpr std::uint8_t kHomunculusGroup = 1;

// LES HOMONCULES : ils suivent le héros, fondent sur ce qui entre dans sa
// laisse, et ne meurent pas.
constexpr std::uint16_t kLifClass = 6001;
constexpr std::uint16_t kAmistrClass = 6002;
constexpr std::uint16_t kFilirClass = 6003;
constexpr std::uint16_t kVanilmirthClass = 6004;
constexpr std::uint16_t kEvolvedLifClass = 6005;
constexpr std::uint16_t kEvolvedAmistrClass = 6006;
constexpr std::uint16_t kEvolvedFilirClass = 6007;
constexpr std::uint16_t kEvolvedVanilmirthClass = 6008;
constexpr std::uint32_t kHomunculusCooldownMs = 1000;
constexpr std::int32_t kHomunculusLeashCells = 6;
constexpr std::int32_t kHomunculusWalkCellsPerSecond = 6;
constexpr std::int32_t kFilirFlightCellsPerSecond = 9;
constexpr std::int32_t kHomunculusReachTenths = 13;
constexpr std::int32_t kCapriceReachCells = 6;
constexpr std::int32_t kLifReachCells = 4;
constexpr std::int32_t kAmistrKnockbackTenths = 10;
// Amistr prend la place du héros frappé sous ce seuil.
constexpr std::int32_t kCastlingBelowHpPct = 30;

// Un homoncule `homunculus` : il frappe de `damage` toutes les `strike_ms` à
// `reach_units`, marche à `walk_cells` cases par seconde, et soigne de
// `heal_pct` %.
struct Homunculus {
  std::uint16_t homunculus = 0;
  std::int32_t damage = 0;
  std::uint32_t strike_ms = 0;
  std::int32_t reach_units = 0;
  std::int32_t walk_cells = kHomunculusWalkCellsPerSecond;
  std::int32_t heal_pct = 0;
  std::int32_t knockback_units = 0;
  std::int32_t area_radius_units = 0;
  std::int32_t splash_pct = 0;
  std::int32_t swap_below_hp_pct = 0;
  std::uint32_t swap_cooldown_ms = kHomunculusCooldownMs;
};
[[nodiscard]] constexpr SkillLevel Homun(const Homunculus& homunculus) noexcept {
  return SkillLevel{.damage = homunculus.damage,
                    .cooldown_ms = homunculus.swap_cooldown_ms,
                    .amount = 1,
                    .interval_ms = homunculus.strike_ms,
                    .reach_units = homunculus.reach_units,
                    .speed_units_per_s = Cells(homunculus.walk_cells),
                    .area_radius_units = homunculus.area_radius_units,
                    .splash_pct = homunculus.splash_pct,
                    .knockback_units = homunculus.knockback_units,
                    .reflex_below_hp_pct = homunculus.swap_below_hp_pct,
                    .summon_class = homunculus.homunculus,
                    .leash_units = Cells(kHomunculusLeashCells),
                    .heal_pct = homunculus.heal_pct};
}

// Filir : il frappe vite le plus proche, en volant.
[[nodiscard]] constexpr SkillLevel Filir(std::int32_t damage, std::uint32_t strike_ms) noexcept {
  return Homun({.homunculus = kFilirClass,
                .damage = damage,
                .strike_ms = strike_ms,
                .reach_units = CellTenths(kHomunculusReachTenths),
                .walk_cells = kFilirFlightCellsPerSecond});
}
// Vanilmirth : il frappe de loin un monstre au hasard.
[[nodiscard]] constexpr SkillLevel Vanilmirth(std::int32_t damage,
                                              std::uint32_t strike_ms) noexcept {
  return Homun({.homunculus = kVanilmirthClass,
                .damage = damage,
                .strike_ms = strike_ms,
                .reach_units = Cells(kCapriceReachCells)});
}
// Lif : il soigne le plus blessé, et frappe quand personne ne l'est.
[[nodiscard]] constexpr SkillLevel Lif(std::int32_t damage, std::uint32_t strike_ms,
                                       std::int32_t heal_pct) noexcept {
  return Homun({.homunculus = kLifClass,
                .damage = damage,
                .strike_ms = strike_ms,
                .reach_units = Cells(kLifReachCells),
                .heal_pct = heal_pct});
}
// Amistr : il repousse ce qu'il frappe, et prend la place du héros en danger.
[[nodiscard]] constexpr SkillLevel Amistr(std::int32_t damage, std::uint32_t strike_ms,
                                          std::uint32_t swap_cooldown_ms) noexcept {
  return Homun({.homunculus = kAmistrClass,
                .damage = damage,
                .strike_ms = strike_ms,
                .reach_units = CellTenths(kHomunculusReachTenths),
                .knockback_units = CellTenths(kAmistrKnockbackTenths),
                .swap_below_hp_pct = kCastlingBelowHpPct,
                .swap_cooldown_ms = swap_cooldown_ms});
}

// Filir(dégâts, coup en ms).
constexpr std::array<SkillLevel, kMostSkillLevels> kMoonlightLevels{{
    Filir(14, 600),
    Filir(17, 580),
    Filir(20, 560),
    Filir(24, 540),
    Filir(28, 500),
    Filir(32, 470),
    Filir(36, 440),
    Filir(40, 400),
}};

// Vanilmirth(dégâts, coup en ms).
constexpr std::array<SkillLevel, kMostSkillLevels> kCapriceLevels{{
    Vanilmirth(18, 900),
    Vanilmirth(22, 880),
    Vanilmirth(26, 850),
    Vanilmirth(30, 800),
    Vanilmirth(35, 760),
    Vanilmirth(40, 720),
    Vanilmirth(45, 660),
    Vanilmirth(50, 600),
}};

// Lif(dégâts, coup en ms, soin en %).
constexpr std::array<SkillLevel, kMostSkillLevels> kTouchOfHealLevels{{
    Lif(8, 4000, 3),
    Lif(10, 3800, 3),
    Lif(11, 3600, 4),
    Lif(13, 3400, 4),
    Lif(14, 3200, 5),
    Lif(16, 3000, 6),
    Lif(18, 2800, 7),
    Lif(20, 2500, 8),
}};

// Amistr(dégâts, coup en ms, recharge de l'échange en ms).
constexpr std::array<SkillLevel, kMostSkillLevels> kCastlingLevels{{
    Amistr(16, 700, 12000),
    Amistr(20, 680, 11500),
    Amistr(23, 660, 11000),
    Amistr(27, 640, 10500),
    Amistr(31, 600, 10000),
    Amistr(35, 570, 9500),
    Amistr(40, 540, 9000),
    Amistr(44, 500, 8000),
}};

// Les homoncules évolués : le même, au corps évolué, bien plus fort.
constexpr std::array<SkillLevel, 1> kSbr44Levels{{
    Homun({.homunculus = kEvolvedFilirClass,
           .damage = 110,
           .strike_ms = 400,
           .reach_units = CellTenths(kHomunculusReachTenths),
           .walk_cells = kFilirFlightCellsPerSecond}),
}};
constexpr std::array<SkillLevel, 1> kBioExplosionLevels{{
    Homun({.homunculus = kEvolvedVanilmirthClass,
           .damage = 90,
           .strike_ms = 600,
           .reach_units = Cells(kCapriceReachCells),
           .area_radius_units = CellTenths(25),
           .splash_pct = 70}),
}};
constexpr std::array<SkillLevel, 1> kMentalChangeLevels{{
    Homun({.homunculus = kEvolvedLifClass,
           .damage = 45,
           .strike_ms = 2000,
           .reach_units = Cells(kLifReachCells),
           .heal_pct = 10}),
}};
constexpr std::array<SkillLevel, 1> kBloodLustLevels{{
    Homun({.homunculus = kEvolvedAmistrClass,
           .damage = 100,
           .strike_ms = 450,
           .reach_units = CellTenths(kHomunculusReachTenths),
           .knockback_units = CellTenths(15),
           .swap_below_hp_pct = kCastlingBelowHpPct,
           .swap_cooldown_ms = 7000}),
}};

constexpr std::array<SurvivorsSkillRule, kHomunculusSkillCount> kHomunculusSkills{{
    {.skill_id = kMoonlightId,
     .key = "moonlight",
     .description = core::TrText("Filir vole à vos côtés et frappe vite ce qui approche."),
     .archetype = SkillArchetype::kSummon,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kNeutral,
     .levels = kMoonlightLevels,
     .exclusive_group = kHomunculusGroup},
    {.skill_id = kCapriceId,
     .key = "caprice",
     .description = core::TrText("Vanilmirth vous suit et frappe de loin un monstre au hasard."),
     .archetype = SkillArchetype::kSummon,
     .target = TargetRule::kRandom,
     .element = SurvivorsElement::kNeutral,
     .levels = kCapriceLevels,
     .exclusive_group = kHomunculusGroup},
    {.skill_id = kTouchOfHealId,
     .key = "touch_of_heal",
     .description = core::TrText("Lif vous suit et soigne le héros le plus blessé."),
     .archetype = SkillArchetype::kSummon,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kNeutral,
     .levels = kTouchOfHealLevels,
     .exclusive_group = kHomunculusGroup},
    {.skill_id = kCastlingId,
     .key = "castling",
     .description = core::TrText("Amistr repousse ce qu'il frappe, et prend votre place quand vous "
                                 "faiblissez."),
     .archetype = SkillArchetype::kSummon,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kNeutral,
     .levels = kCastlingLevels,
     .exclusive_group = kHomunculusGroup},
    {.skill_id = kSbr44Id,
     .key = "sbr44",
     .description = core::TrText("Filir, évolué, frappe d'une force terrible."),
     .archetype = SkillArchetype::kSummon,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kNeutral,
     .levels = kSbr44Levels,
     .exclusive_group = kHomunculusGroup},
    {.skill_id = kBioExplosionId,
     .key = "bio_explosion",
     .description = core::TrText("Vanilmirth, évolué, fait éclater la horde autour de sa cible."),
     .archetype = SkillArchetype::kSummon,
     .target = TargetRule::kRandom,
     .element = SurvivorsElement::kNeutral,
     .levels = kBioExplosionLevels,
     .exclusive_group = kHomunculusGroup},
    {.skill_id = kMentalChangeId,
     .key = "mental_change",
     .description = core::TrText("Lif, évolué, soigne bien plus et frappe de loin."),
     .archetype = SkillArchetype::kSummon,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kNeutral,
     .levels = kMentalChangeLevels,
     .exclusive_group = kHomunculusGroup},
    {.skill_id = kBloodLustId,
     .key = "blood_lust",
     .description = core::TrText("Amistr, évolué, broie et renvoie la horde au loin."),
     .archetype = SkillArchetype::kSummon,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kNeutral,
     .levels = kBloodLustLevels,
     .exclusive_group = kHomunculusGroup},
}};

}  // namespace

std::span<const SurvivorsSkillRule> HomunculusSkills() { return kHomunculusSkills; }

}  // namespace ro::sim::survivors

#include <array>
#include <cstdint>
#include <span>

#include "ro/core/i18n.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "survivors_shipped.h"

namespace ro::sim::survivors {

namespace {

// ── LE COUP DE L'ARME ──────────────────────────────────────────────────────
// Toute classe frappe de son arme, sans emplacement ni offre : un socle de
// dégâts, la moitié environ d'un skill de départ, que les cartes de l'arme font
// grandir. Niveau 1 : l'arme du départ ; niveau 2 : celle de l'évolution.

constexpr std::size_t kWeaponLevelCount = 2;

// Le trait de baguette du Magicien : un projectile droit, comme la flèche.
constexpr std::int32_t kRodSpeedCellsPerSecond = 10;
constexpr std::int32_t kRodHitRadiusTenths = 4;
constexpr std::uint32_t kRodCooldownMs = 900;

[[nodiscard]] constexpr SkillLevel RodShot(std::uint8_t amount, std::int32_t damage,
                                           std::int32_t reach_cells) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = kRodCooldownMs,
                    .amount = amount,
                    .reach_units = Cells(reach_cells),
                    .speed_units_per_s = Cells(kRodSpeedCellsPerSecond),
                    .hit_radius_units = CellTenths(kRodHitRadiusTenths)};
}

// Le coup de dague du Voleur, puis le double coup de katar de l'Assassin.
constexpr std::int32_t kStabReachTenths = 15;
constexpr std::uint32_t kKatarIntervalMs = 120;

[[nodiscard]] constexpr SkillLevel Stab(std::uint8_t amount, std::int32_t damage,
                                        std::uint32_t cooldown_ms) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = amount,
                    .interval_ms = kKatarIntervalMs,
                    .reach_units = CellTenths(kStabReachTenths)};
}

// La taille d'épée de l'Épéiste, large et courte, puis l'estoc de lance du
// Chevalier, étroit et long : un cône devant le héros.
constexpr std::uint32_t kSlashCooldownMs = 1000;
[[nodiscard]] constexpr SkillLevel Slash(std::int32_t damage, std::int32_t reach_tenths,
                                         std::uint8_t half_steps) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = kSlashCooldownMs,
                    .reach_units = CellTenths(reach_tenths),
                    .cone_half_steps = half_steps};
}

// Le coup de hache du Marchand, puis le coup de marteau de l'Alchimiste, qui
// éclate autour du monstre frappé.
constexpr std::int32_t kBlowReachTenths = 15;
constexpr std::int32_t kBlowSplashPct = 50;
constexpr std::int32_t kAxeKnockbackTenths = 3;
constexpr std::uint32_t kBlowCooldownMs = 1300;

[[nodiscard]] constexpr SkillLevel Blow(std::int32_t damage, std::int32_t radius_tenths) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = kBlowCooldownMs,
                    .amount = 1,
                    .reach_units = CellTenths(kBlowReachTenths),
                    .area_radius_units = CellTenths(radius_tenths),
                    .splash_pct = kBlowSplashPct,
                    .knockback_units = CellTenths(kAxeKnockbackTenths)};
}

// La flèche de l'Archer : un projectile droit.
constexpr std::int32_t kBowSpeedCellsPerSecond = 15;
constexpr std::int32_t kBowHitRadiusTenths = 3;

[[nodiscard]] constexpr SkillLevel BowShot(std::int32_t damage, std::uint32_t cooldown_ms,
                                           std::int32_t reach_cells) noexcept {
  return SkillLevel{.damage = damage,
                    .cooldown_ms = cooldown_ms,
                    .amount = 1,
                    .reach_units = Cells(reach_cells),
                    .speed_units_per_s = Cells(kBowSpeedCellsPerSecond),
                    .hit_radius_units = CellTenths(kBowHitRadiusTenths)};
}

constexpr std::array<SkillLevel, kWeaponLevelCount> kRodShotLevels{
    {RodShot(1, 10, 8), RodShot(2, 9, 9)}};
constexpr std::array<SkillLevel, kWeaponLevelCount> kDaggerStabLevels{
    {Stab(1, 8, 600), Stab(2, 7, 550)}};
// Un demi-angle de 8 caps sur 64 : un quart de tour en tout ; 3 caps : l'estoc.
constexpr std::array<SkillLevel, kWeaponLevelCount> kSwordSlashLevels{
    {Slash(14, 20, 8), Slash(16, 35, 3)}};
constexpr std::array<SkillLevel, kWeaponLevelCount> kAxeBlowLevels{{Blow(18, 4), Blow(20, 10)}};
constexpr std::array<SkillLevel, kWeaponLevelCount> kBowShotLevels{
    {BowShot(11, 1000, 10), BowShot(13, 850, 11)}};

constexpr std::array<std::uint16_t, kWeaponLevelCount> kRodItems{kRodItemId, kRodItemId};
constexpr std::array<std::uint16_t, kWeaponLevelCount> kDaggerItems{kKnifeItemId, kJurItemId};
constexpr std::array<std::uint16_t, kWeaponLevelCount> kSwordItems{kSwordItemId, kJavelinItemId};
constexpr std::array<std::uint16_t, kWeaponLevelCount> kAxeItems{kAxeItemId,
                                                                 kHollgrehennHammerItemId};
constexpr std::array<std::uint16_t, kWeaponLevelCount> kBowItems{kBowItemId, kHunterBowItemId};

constexpr std::array<SurvivorsSkillRule, kWeaponSkillCount> kWeaponSkills{{
    {.skill_id = kRodShotSkillId,
     .key = "rod_shot",
     .description = core::TrText("Votre baguette tire un trait droit sur le plus proche."),
     .archetype = SkillArchetype::kLinear,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kNeutral,
     .levels = kRodShotLevels,
     .weapon_item_ids = kRodItems},
    {.skill_id = kDaggerStabSkillId,
     .key = "dagger_stab",
     .description = core::TrText("Un coup de lame vif au plus proche ; au katar, deux."),
     .archetype = SkillArchetype::kTargetStrike,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kNeutral,
     .levels = kDaggerStabLevels,
     .weapon_item_ids = kDaggerItems},
    {.skill_id = kSwordSlashSkillId,
     .key = "sword_slash",
     .description = core::TrText("Une taille d'épée devant vous ; à la lance, un long estoc."),
     .archetype = SkillArchetype::kCone,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kNeutral,
     .levels = kSwordSlashLevels,
     .weapon_item_ids = kSwordItems},
    {.skill_id = kAxeBlowSkillId,
     .key = "axe_blow",
     .description = core::TrText("Un lourd coup de hache ; au marteau, il éclate alentour."),
     .archetype = SkillArchetype::kClusterBlast,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kNeutral,
     .levels = kAxeBlowLevels,
     .weapon_item_ids = kAxeItems},
    {.skill_id = kBowShotSkillId,
     .key = "bow_shot",
     .description = core::TrText("Une flèche droite vers le plus proche."),
     .archetype = SkillArchetype::kLinear,
     .target = TargetRule::kNearest,
     .element = SurvivorsElement::kNeutral,
     .levels = kBowShotLevels,
     .weapon_item_ids = kBowItems},
}};

// ── LES CARTES DE L'ARME ───────────────────────────────────────────────────

// La carte Mantis : le coup de l'arme revient plus vite.
constexpr std::array<std::int32_t, kMostCardLevels> kMantisLevels{8, 16, 24, 32, 40};
// La carte Orc Skeleton : il traverse des monstres de plus.
constexpr std::array<std::int32_t, kMostCardLevels> kOrcSkeletonLevels{1, 1, 2, 2, 3};
// La carte Nightmare : une part de ses coups rebondit sur un monstre proche.
constexpr std::array<std::int32_t, kMostCardLevels> kNightmareLevels{10, 20, 30, 40, 50};

constexpr std::array<CardRule, kWeaponCardCount> kWeaponCards{{
    {.card_item_id = kMantisCardId,
     .key = "mantis_card",
     .description = core::TrText("Le coup de votre arme revient plus vite."),
     .stat = SurvivorsStat::kWeaponDelayCutPct,
     .per_level = kMantisLevels},
    {.card_item_id = kOrcSkeletonCardId,
     .key = "orc_skeleton_card",
     .description =
         core::TrText("Le coup de votre arme transperce : il touche des monstres de plus."),
     .stat = SurvivorsStat::kWeaponPierce,
     .per_level = kOrcSkeletonLevels},
    {.card_item_id = kNightmareCardId,
     .key = "nightmare_card",
     .description = core::TrText("Le coup de votre arme peut rebondir sur un monstre proche."),
     .stat = SurvivorsStat::kWeaponSplitChancePct,
     .per_level = kNightmareLevels},
}};

}  // namespace

std::span<const SurvivorsSkillRule> WeaponSkills() { return kWeaponSkills; }

std::span<const CardRule> WeaponCards() { return kWeaponCards; }

}  // namespace ro::sim::survivors

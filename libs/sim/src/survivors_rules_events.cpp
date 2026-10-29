#include <array>
#include <cstdint>
#include <span>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"

#include "survivors_shipped.h"

namespace ro::sim::survivors {

namespace {

// ── LES ÉVÉNEMENTS DE L'ARÈNE 1, ses élites, son coffre et ses plantes ──────

constexpr std::array<EventRule, 8> kEvents{{
    {.at_ms = 90000, .kind = EventKind::kSwarm, .foe_id = kChonchonId, .count = 24},
    {.at_ms = 150000,
     .kind = EventKind::kRing,
     .foe_id = kPupaId,
     .count = 28,
     .radius_units = Cells(6)},
    {.at_ms = 270000, .kind = EventKind::kSwarm, .foe_id = kHornetId, .count = 30},
    {.at_ms = 340000, .kind = EventKind::kGroup, .foe_id = kThiefBugId, .count = 6},
    {.at_ms = 390000,
     .kind = EventKind::kRing,
     .foe_id = kSporeId,
     .count = 12,
     .radius_units = Cells(7)},
    {.at_ms = 510000, .kind = EventKind::kSwarm, .foe_id = kFamiliarId, .count = 40},
    {.at_ms = 630000, .kind = EventKind::kRaid, .foe_id = kGoblinId, .count = 18},
    {.at_ms = 810000,
     .kind = EventKind::kRing,
     .foe_id = kGhoulId,
     .count = 30,
     .radius_units = Cells(8)},
}};

// Une élite à 2:00, puis toutes les 75 s jusqu'à 14:30 : onze en tout, tirées
// de la vague en vigueur, dix fois plus robustes et lâchant un coffre.
constexpr EliteRule kElites{.first_ms = 120000,
                            .interval_ms = 75000,
                            .last_ms = 870000,
                            .hp_pct = 1000,
                            .touch_pct = 150,
                            .exp_pct = 500};

// L'Old Blue Box : un gain, trois sous 12 % (sur 10 000), cinq sous 2 %, la
// chance du héros majorant ces deux seuils. Les évolutions s'y ouvrent à 6:00.
constexpr std::uint16_t kOldBlueBoxId = 603;
constexpr ChestRule kChest{.item_id = kOldBlueBoxId,
                           .evolution_from_ms = 6 * kMinuteMs,
                           .triple_per_10000 = 1200,
                           .quintuple_per_10000 = 200};

// Les objets au sol : le coffre, puis ce que lâchent les plantes — Meat (+60
// PV), Gold (+25 Zeny), Battle Manual (XP doublée 60 s), Speed Potion (marche
// +50 % 10 s), Holy Water (purifie à douze cases), Bubble Gum (toutes les
// gemmes).
constexpr std::array<PickupRule, 7> kPickups{{
    {.kind = PickupKind::kChest, .item_id = kOldBlueBoxId},
    {.kind = PickupKind::kMeat, .item_id = 517, .amount = 60},
    {.kind = PickupKind::kGold, .item_id = 969, .amount = 25},
    {.kind = PickupKind::kManual, .item_id = 12208, .amount = 100, .duration_ms = 60000},
    {.kind = PickupKind::kHaste, .item_id = 12016, .amount = 50, .duration_ms = 10000},
    {.kind = PickupKind::kHolyWater, .item_id = 523, .reach_units = Cells(12)},
    {.kind = PickupKind::kMagnetAll, .item_id = 12210},
}};

// Les plantes, nées au fil des tués : la classe RO de la plante, l'objet
// qu'elle lâche, son poids au tirage. L'expérience d'abord (manuel, aimant),
// puis la viande ; l'or, rare, vaut plus.
constexpr std::array<PlantRule, 6> kPlants{{
    {.foe_id = 1078, .pickup = PickupKind::kMeat, .weight = 30},       // Red Plant
    {.foe_id = 1081, .pickup = PickupKind::kGold, .weight = 10},       // Yellow Plant
    {.foe_id = 1080, .pickup = PickupKind::kHaste, .weight = 10},      // Green Plant
    {.foe_id = 1079, .pickup = PickupKind::kManual, .weight = 30},     // Blue Plant
    {.foe_id = 1083, .pickup = PickupKind::kMagnetAll, .weight = 15},  // Shining Plant
    {.foe_id = 1082, .pickup = PickupKind::kHolyWater, .weight = 5},   // White Plant
}};

}  // namespace

std::span<const EventRule> ShippedEvents() { return kEvents; }

EliteRule ShippedElites() { return kElites; }

ChestRule ShippedChest() { return kChest; }

std::span<const PlantRule> ShippedPlants() { return kPlants; }

std::span<const PickupRule> ShippedPickups() { return kPickups; }

}  // namespace ro::sim::survivors

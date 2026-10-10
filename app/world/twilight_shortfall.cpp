#include "world/twilight_shortfall.h"

#include <algorithm>
#include <array>
#include <span>

namespace moonclient {
namespace {

constexpr std::uint16_t kTwilightAlchemy1 = 496;
constexpr std::uint16_t kTwilightAlchemy2 = 497;
constexpr std::uint16_t kTwilightAlchemy3 = 498;

// LES OBJETS DES RECETTES.
constexpr std::uint32_t kNormalPotionGuide = 7144;
constexpr std::uint32_t kSlimPotionGuide = 7133;
constexpr std::uint32_t kFireBottleGuide = 7128;
constexpr std::uint32_t kAcidBottleGuide = 7129;
constexpr std::uint32_t kWhiteHerb = 509;
constexpr std::uint32_t kWhitePotion = 504;
constexpr std::uint32_t kEmptyPotionBottle = 1093;
constexpr std::uint32_t kEmptyTestTube = 1092;
constexpr std::uint32_t kWitchedStarsand = 1061;
constexpr std::uint32_t kAlcohol = 970;
constexpr std::uint32_t kFabric = 1059;
constexpr std::uint32_t kImmortalHeart = 929;
constexpr std::uint32_t kEmptyBottle = 713;

// UN MANUEL ne se consomme pas : il suffit d'en porter un.
constexpr std::int32_t kGuideCopies = 1;
// Les deux premières fabriquent deux cents potions d'un coup.
constexpr std::int32_t kPotionBatch = 200;
// La troisième, cinquante de chaque bouteille.
constexpr std::int32_t kBottleBatch = 50;
// Et elle exige cent Empty Bottle en tout, une par bouteille fabriquée.
constexpr std::int32_t kEmptyBottlesTotal = 100;

struct RecipeLine {
  std::uint32_t item_id = 0;
  std::int32_t required = 0;
};

constexpr std::array<RecipeLine, 3> kTwilight1 = {{
    {kNormalPotionGuide, kGuideCopies},
    {kWhiteHerb, kPotionBatch},
    {kEmptyPotionBottle, kPotionBatch},
}};

constexpr std::array<RecipeLine, 4> kTwilight2 = {{
    {kSlimPotionGuide, kGuideCopies},
    {kWhitePotion, kPotionBatch},
    {kEmptyTestTube, kPotionBatch},
    {kWitchedStarsand, kPotionBatch},
}};

constexpr std::array<RecipeLine, 6> kTwilight3 = {{
    {kFireBottleGuide, kGuideCopies},
    {kAcidBottleGuide, kGuideCopies},
    {kAlcohol, kBottleBatch},
    {kFabric, kBottleBatch},
    {kImmortalHeart, kBottleBatch},
    {kEmptyBottle, kEmptyBottlesTotal},
}};

[[nodiscard]] std::span<const RecipeLine> RecipeOf(std::uint16_t skill_id) noexcept {
  switch (skill_id) {
    case kTwilightAlchemy1:
      return kTwilight1;
    case kTwilightAlchemy2:
      return kTwilight2;
    case kTwilightAlchemy3:
      return kTwilight3;
    default:
      return {};
  }
}

// Combien d'exemplaires de l'objet le sac porte, toutes cases réunies.
[[nodiscard]] std::int32_t Owned(std::uint32_t item_id, const ro::sim::Inventory& inventory) {
  std::int32_t owned = 0;
  for (const ro::sim::InventoryItem& item : inventory.items()) {
    if (item.item_id == item_id) {
      owned += std::max<std::int32_t>(item.amount, 0);
    }
  }
  return owned;
}

}  // namespace

bool IsTwilightAlchemy(std::uint16_t skill_id) noexcept { return !RecipeOf(skill_id).empty(); }

std::vector<TwilightNeed> TwilightShortfall(std::uint16_t skill_id,
                                            const ro::sim::Inventory& inventory) {
  std::vector<TwilightNeed> missing;
  for (const RecipeLine& line : RecipeOf(skill_id)) {
    const std::int32_t owned = Owned(line.item_id, inventory);
    if (owned < line.required) {
      missing.push_back(
          TwilightNeed{.item_id = line.item_id, .required = line.required, .owned = owned});
    }
  }
  return missing;
}

}  // namespace moonclient

// Les formules du serveur, rejouées terme à terme. Voir l'en-tête pour la règle
// qui les tient toutes : une moyenne exacte des issues de chaque tirage.

#include "items/make_item_odds.h"

#include <algorithm>
#include <cmath>

namespace moonclient {
namespace {

// L'échelle du serveur : dix mille pour cent pour cent.
constexpr int kScale = 10000;
constexpr double kScaleAsDouble = 10000.0;
constexpr double kPercent = 100.0;
constexpr int kLeastPer = 1;

// ── LES TERMES COMMUNS ──────────────────────────────────────────────────────
constexpr int kJobLevelWeight = 20;
constexpr int kDexWeight = 10;
constexpr int kLukWeight = 10;
// Le tirage uniforme qui s'ajoute au seuil des métaux et de la forge : de un à
// cent, pas de dix.
constexpr int kRollCount = 100;
constexpr int kRollStep = 10;

// ── LES MÉTAUX ──────────────────────────────────────────────────────────────
constexpr int kMetalBase = 1000;
constexpr int kIronBase = 4000;
constexpr int kSteelBase = 3000;
constexpr int kMetalSkillWeight = 500;

// ── LA FORGE ────────────────────────────────────────────────────────────────
// Sous le niveau quatre, l'arme ajoute `(4 / niveau) × 1000` — en division
// ENTIÈRE : quatre mille, deux mille, mille.
constexpr int kWeaponLevelNumerator = 4;
constexpr int kWeaponLevelWeight = 1000;
constexpr int kForgeSkillWeight = 500;
constexpr int kResearchWeight = 100;
constexpr int kOrideconLeastWeaponLevel = 3;
constexpr int kElementMalus = 2500;
constexpr int kStarCrumbMalus = 1500;

// ── LA PHARMACIE ────────────────────────────────────────────────────────────
constexpr int kPotionResearchWeight = 50;
constexpr int kPharmacyWeight = 300;
constexpr int kIntDivisor = 2;
constexpr int kIntWeight = 10;

// Le bonus propre à un produit : une base, et un tirage de `rolls` issues qui
// s'ajoute — ou se retranche, pour les potions « slim ».
struct PotionBonus {
  std::uint32_t product = 0;
  int base = 0;
  int rolls = 0;
  bool subtract = false;
};
constexpr PotionBonus kPotionBonuses[] = {
    {501, 2000, 100, false}, {503, 2000, 100, false}, {504, 2000, 100, false},
    {970, 1000, 100, false}, {7135, 0, 100, false},   {7136, 0, 100, false},
    {7137, 0, 100, false},   {7138, 0, 100, false},   {546, 0, 50, true},
    {547, 0, 100, true},     {7139, 0, 100, true},
};

// ── LA CUISINE ──────────────────────────────────────────────────────────────
constexpr int kLeastDishLevel = 11;
constexpr int kMostDishLevel = 20;
constexpr int kDishPivot = 10;
constexpr int kKitWeight = 1200;
constexpr int kBaseLevelWeight = 20;
constexpr int kCookDexWeight = 20;
constexpr int kDishWeight = 400;
constexpr int kLukPivot = 101;
constexpr int kCookLukWeight = 10;
// Le tirage gouverné par la maîtrise : de `6 + m/80` à `30 + 5 × (m/400)`,
// pas de cent ; puis un second tirage de un à quatre, pas de cent.
constexpr int kMasteryLowBase = 6;
constexpr int kMasteryLowDivisor = 80;
constexpr int kMasteryHighBase = 30;
constexpr int kMasteryHighWeight = 5;
constexpr int kMasteryHighDivisor = 400;
constexpr int kCookRollStep = 100;
constexpr int kSecondRollCount = 4;

[[nodiscard]] int Clamp(int per) { return std::clamp(per, kLeastPer, kScale); }

[[nodiscard]] int Rounded(double percent) { return static_cast<int>(std::lround(percent)); }

}  // namespace

int MetalChancePercent(std::uint32_t product, int recipe_skill, int skill_level,
                       const CrafterStats& stats) {
  if (product == kStarCrumb) {
    return static_cast<int>(kPercent);
  }
  int base = kMetalBase;
  if (recipe_skill == kIronTemperingSkill) {
    base = kIronBase;
  } else if (recipe_skill == kSteelTemperingSkill) {
    base = kSteelBase;
  }
  const int fixed = stats.job_level * kJobLevelWeight + stats.dex * kDexWeight +
                    stats.luk * kLukWeight + base + skill_level * kMetalSkillWeight;
  double sum = 0.0;
  for (int roll = 1; roll <= kRollCount; ++roll) {
    sum += Clamp(fixed + roll * kRollStep) / kScaleAsDouble;
  }
  // Cent issues d'un poids de un centième : la somme EST le pourcentage.
  return Rounded(sum);
}

int ForgeChancePercent(const ForgeInputs& forge, const CrafterStats& stats) {
  if (forge.produce_rate < 0 || forge.weapon_level <= 0 || forge.recipe_skill <= 0) {
    return kUnknownChance;
  }
  int fixed = stats.job_level * kJobLevelWeight + stats.dex * kDexWeight + stats.luk * kLukWeight;
  if (forge.weapon_level < kWeaponLevelNumerator) {
    fixed += (kWeaponLevelNumerator / forge.weapon_level) * kWeaponLevelWeight;
  }
  fixed += forge.skill_level * kForgeSkillWeight;
  fixed += forge.weapon_research_level * kResearchWeight;
  if (forge.oridecon_research_fix && forge.weapon_level >= kOrideconLeastWeaponLevel) {
    fixed += forge.oridecon_research_level * kResearchWeight;
  }
  if (forge.element) {
    fixed -= kElementMalus;
  }
  fixed -= forge.star_crumbs * kStarCrumbMalus;
  fixed += forge.anvil_bonus;
  double sum = 0.0;
  for (int roll = 1; roll <= kRollCount; ++roll) {
    // ⚠ LE MULTIPLICATEUR APRÈS LE TIRAGE, et la borne APRÈS le multiplicateur.
    const int per = (fixed + roll * kRollStep) * forge.produce_rate / static_cast<int>(kPercent);
    sum += Clamp(per) / kScaleAsDouble;
  }
  return Rounded(sum);
}

int PharmacyChancePercent(const PharmacyInputs& pharmacy, const CrafterStats& stats) {
  if (pharmacy.produce_rate < 0) {
    return kUnknownChance;
  }
  const int fixed = pharmacy.potion_research_level * kPotionResearchWeight +
                    pharmacy.pharmacy_level * kPharmacyWeight + stats.job_level * kJobLevelWeight +
                    (stats.int_ / kIntDivisor) * kIntWeight + stats.dex * kDexWeight +
                    stats.luk * kLukWeight;
  const PotionBonus* bonus = nullptr;
  for (const PotionBonus& entry : kPotionBonuses) {
    if (entry.product == pharmacy.product) {
      bonus = &entry;
      break;
    }
  }
  const int rolls = bonus != nullptr ? bonus->rolls : 1;
  double sum = 0.0;
  for (int roll = 1; roll <= rolls; ++roll) {
    int per = fixed;
    if (bonus != nullptr) {
      per += bonus->base + (bonus->subtract ? -(roll * kRollStep) : roll * kRollStep);
    }
    per = per * pharmacy.produce_rate / static_cast<int>(kPercent);
    sum += Clamp(per) / kScaleAsDouble;
  }
  return Rounded(sum * kPercent / rolls);
}

int CookingChancePercent(int kit_level, int dish_level, int mastery, const CrafterStats& stats) {
  if (kit_level < kLeastDishLevel || kit_level > kMostDishLevel) {
    return kUnknownChance;
  }
  if (kit_level >= kSureCookingKit) {
    return static_cast<int>(kPercent);
  }
  if (mastery < 0 || dish_level < kLeastDishLevel || dish_level > kMostDishLevel) {
    return kUnknownChance;
  }
  const int fixed = kKitWeight * (kit_level - kDishPivot) +
                    kBaseLevelWeight * (stats.base_level + 1) + kCookDexWeight * (stats.dex + 1) -
                    kDishWeight * (dish_level - kDishPivot) -
                    kCookLukWeight * (kLukPivot - stats.luk);
  const int low = kMasteryLowBase + mastery / kMasteryLowDivisor;
  const int span = kMasteryHighBase + kMasteryHighWeight * (mastery / kMasteryHighDivisor) - low;
  if (span <= 0) {
    return kUnknownChance;
  }
  double sum = 0.0;
  for (int first = 0; first < span; ++first) {
    for (int second = 0; second < kSecondRollCount; ++second) {
      sum += Clamp(fixed + kCookRollStep * (low + first) - kCookRollStep * (second + 1)) /
             kScaleAsDouble;
    }
  }
  return Rounded(sum * kPercent / (span * kSecondRollCount));
}

}  // namespace moonclient

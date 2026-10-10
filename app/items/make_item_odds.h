#pragma once

#include <cstdint>

namespace moonclient {

// ── LES CHANCES D'UNE FABRICATION, telles que le serveur les calcule ────────
//
// 🔴 CE SONT DES PROBABILITÉS EXACTES. Quand le serveur ajoute un tirage À
// L'INTÉRIEUR de son seuil, la probabilité ne devient pas indéterminable : elle
// est la MOYENNE des issues du tirage, toutes également probables. C'est ce que
// chaque fonction fait, issue par issue.
//
// Tout se compte sur dix mille, comme le serveur ; le résultat est en pour cent,
// arrondi. Moins un veut dire « on ne sait pas » — et l'appelant se tait.
//
// ⚠ LES CARACTÉRISTIQUES SONT LES VALEURS EFFECTIVES, équipement compris : celles
// que le serveur lit.

inline constexpr int kUnknownChance = -1;

// Les skills qui décident d'une formule.
inline constexpr int kIronTemperingSkill = 94;
inline constexpr int kSteelTemperingSkill = 95;
inline constexpr int kEnchantedStoneSkill = 96;
inline constexpr int kOrideconResearchSkill = 97;
inline constexpr int kForgeFirstSkill = 98;  // Smith Dagger
inline constexpr int kForgeLastSkill = 104;  // Smith Spear
inline constexpr int kWeaponResearchSkill = 107;
inline constexpr int kPotionResearchSkill = 227;
inline constexpr int kPharmacySkill = 228;
inline constexpr int kMakingArrowSkill = 147;

// Le Star Crumb, et les quatre pierres élémentaires qui le précèdent.
inline constexpr std::uint32_t kStarCrumb = 1000;
inline constexpr std::uint32_t kFirstElementStone = 994;
inline constexpr std::uint32_t kLastElementStone = 997;

struct CrafterStats {
  int job_level = 0;
  int base_level = 0;
  int int_ = 0;
  int dex = 0;
  int luk = 0;
};

// FER, ACIER, PIERRES : l'affinage des métaux.
[[nodiscard]] int MetalChancePercent(std::uint32_t product, int recipe_skill, int skill_level,
                                     const CrafterStats& stats);

// LA FORGE D'UNE ARME.
struct ForgeInputs {
  int weapon_level = 0;
  // Le skill de la recette, et son niveau appris.
  int recipe_skill = 0;
  int skill_level = 0;
  int weapon_research_level = 0;
  int oridecon_research_level = 0;
  bool oridecon_research_fix = false;
  // Le réglage serveur qui multiplie le taux, en pour cent. Moins un : inconnu.
  int produce_rate = kUnknownChance;
  int star_crumbs = 0;
  bool element = false;
  // Le bonus de la meilleure enclume portée, sur dix mille.
  int anvil_bonus = 0;
};
[[nodiscard]] int ForgeChancePercent(const ForgeInputs& forge, const CrafterStats& stats);

// LA PHARMACIE.
struct PharmacyInputs {
  std::uint32_t product = 0;
  int pharmacy_level = 0;
  int potion_research_level = 0;
  int produce_rate = kUnknownChance;
};
[[nodiscard]] int PharmacyChancePercent(const PharmacyInputs& pharmacy, const CrafterStats& stats);

// LA CUISINE. `mastery` est la maîtrise culinaire, moins un si jamais reçue.
[[nodiscard]] int CookingChancePercent(int kit_level, int dish_level, int mastery,
                                       const CrafterStats& stats);

// Les bornes de la maîtrise culinaire, qui encadrent la fourchette affichée
// quand le serveur ne l'a pas transmise.
inline constexpr int kLeastCookMastery = 0;
inline constexpr int kMostCookMastery = 1999;
// Un kit à ce niveau réussit toujours, sans tirage.
inline constexpr int kSureCookingKit = 15;

// LES ENCLUMES, la meilleure d'abord, et ce qu'elles ajoutent sur dix mille.
struct Anvil {
  std::uint32_t item_id = 0;
  int bonus = 0;
};
inline constexpr Anvil kAnvils[] = {{989, 1000}, {988, 500}, {987, 250}, {986, 0}};

}  // namespace moonclient

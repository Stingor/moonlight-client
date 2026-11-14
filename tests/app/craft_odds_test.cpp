// LES CHANCES DES ATELIERS : les termes lus dans le livre des recettes, et les
// formules du serveur rejouées.
//
// 🔴 CE BANC TIENT CE QUE LE JEU NE MONTRE JAMAIS. Une chance fausse ne se voit
// pas : elle s'affiche avec l'autorité d'un chiffre juste. Chaque attente
// ci-dessous est calculée à la main, terme à terme, depuis la formule du
// serveur — la moyenne exacte des issues de chaque tirage.

#include <cstdint>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "items/craft_book.h"
#include "items/make_item_odds.h"

using moonclient::CraftBook;
using moonclient::CrafterStats;
using moonclient::kUnknownChance;
using moonclient::kUnknownCraftRate;

namespace {

constexpr std::string_view kTermsText = R"(version: 1
recipes:
  - {id: 1101, lv: 1, skill: 99, skill_lv: 1, mats: [[998, 2]]}
rates:
  weapon_produce: 500
  potion_produce: 100
  oridecon_research_fix: 1
cooking_kits:
  12125: 11
  12849: 30
refine_weapon:
  1: {1: 10000, 8: 6000, 10: 2000}
  4: {5: 6000}
weapon_lv:
  1101-1109: 1
  1122: 2
  1130-1141: 4
)";

[[nodiscard]] CraftBook Terms() {
  auto book = CraftBook::Parse(kTermsText);
  REQUIRE(book.has_value());
  return std::move(*book);
}

}  // namespace

TEST_CASE("Livre des recettes : les réglages qui multiplient les chances", "[craft][chances]") {
  const CraftBook book = Terms();
  CHECK(book.rates().weapon_produce == 500);
  CHECK(book.rates().potion_produce == 100);
  CHECK(book.rates().oridecon_research_fix);
}

TEST_CASE("Livre des recettes : un réglage absent vaut « inconnu », pas cent", "[craft][chances]") {
  auto book = CraftBook::Parse("recipes:\n  - {id: 1101, lv: 1, skill: 99, mats: [[998, 2]]}\n");
  REQUIRE(book.has_value());
  CHECK(book->rates().weapon_produce == kUnknownCraftRate);
  CHECK(book->rates().potion_produce == kUnknownCraftRate);
  CHECK(book->RefineRate(1, 1) == kUnknownCraftRate);
  CHECK(book->WeaponLevel(1101) == 0);
}

TEST_CASE("Livre des recettes : les niveaux d'arme se lisent par plages", "[craft][chances]") {
  const CraftBook book = Terms();
  CHECK(book.WeaponLevel(1101) == 1);
  CHECK(book.WeaponLevel(1109) == 1);
  CHECK(book.WeaponLevel(1110) == 0);
  CHECK(book.WeaponLevel(1122) == 2);
  CHECK(book.WeaponLevel(1135) == 4);
  CHECK(book.WeaponLevel(1100) == 0);
  CHECK(book.WeaponLevel(1142) == 0);
}

TEST_CASE("Livre des recettes : kits de cuisine et table d'affinage", "[craft][chances]") {
  const CraftBook book = Terms();
  CHECK(book.CookingKitLevel(12125) == 11);
  CHECK(book.CookingKitLevel(12849) == 30);
  CHECK(book.CookingKitLevel(501) == 0);
  CHECK(book.RefineRate(1, 1) == 10000);
  CHECK(book.RefineRate(1, 8) == 6000);
  CHECK(book.RefineRate(4, 5) == 6000);
  CHECK(book.RefineRate(1, 2) == kUnknownCraftRate);
  CHECK(book.RefineRate(0, 1) == kUnknownCraftRate);
  CHECK(book.RefineRate(1, 11) == kUnknownCraftRate);
}

TEST_CASE("Affinage : la chance est exacte, et le job level peut la faire baisser",
          "[craft][chances]") {
  // 6000 / 100 + (70 - 50) / 2 = 60 + 10.
  CHECK(moonclient::RefineChancePercent(6000, 70) == 70);
  // Sous le job level cinquante, le bonus est négatif : 100 - 10.
  CHECK(moonclient::RefineChancePercent(10000, 30) == 90);
  // Borné à zéro : 20 + (-49 / 2 = -24).
  CHECK(moonclient::RefineChancePercent(2000, 1) == 0);
  // Borné à cent.
  CHECK(moonclient::RefineChancePercent(10000, 99) == 100);
}

TEST_CASE("Métaux : le Star Crumb réussit toujours, le fer se moyenne sur son tirage",
          "[craft][chances]") {
  const CrafterStats stats{.job_level = 50, .dex = 50, .luk = 10};
  CHECK(moonclient::MetalChancePercent(moonclient::kStarCrumb, moonclient::kIronTemperingSkill, 5,
                                       stats) == 100);
  // 1000 + 500 + 100 + 4000 + 5 × 500 = 8100, puis un tirage moyen de 505.
  CHECK(moonclient::MetalChancePercent(998, moonclient::kIronTemperingSkill, 5, stats) == 86);
}

TEST_CASE("Forge : le multiplicateur, puis les Star Crumb et la pierre", "[craft][chances]") {
  const CrafterStats stats{.job_level = 50, .dex = 50, .luk = 10};
  moonclient::ForgeInputs forge{.weapon_level = 1,
                                .recipe_skill = 99,
                                .skill_level = 3,
                                .weapon_research_level = 10,
                                .produce_rate = 500};
  // 1600 + 4000 + 1500 + 1000 = 8100, × 5 : toujours au plafond.
  CHECK(moonclient::ForgeChancePercent(forge, stats) == 100);
  // 8100 - 3 × 1500 - 2500 = 1100 ; (1100 + 10r) × 5 dépasse 10000 à partir de
  // r = 90 : la moyenne vaut 79,975 %.
  forge.star_crumbs = 3;
  forge.element = true;
  CHECK(moonclient::ForgeChancePercent(forge, stats) == 80);
  // Sans le réglage du serveur, on se tait.
  forge.produce_rate = kUnknownChance;
  CHECK(moonclient::ForgeChancePercent(forge, stats) == kUnknownChance);
}

TEST_CASE("Pharmacie : sans bonus de produit, la chance est déterminée", "[craft][chances]") {
  const CrafterStats stats{.job_level = 50, .int_ = 50, .dex = 50, .luk = 10};
  moonclient::PharmacyInputs pharmacy{
      .product = 505, .pharmacy_level = 10, .potion_research_level = 10, .produce_rate = 100};
  // 500 + 3000 + 1000 + 250 + 500 + 100 = 5350.
  CHECK(moonclient::PharmacyChancePercent(pharmacy, stats) == 54);
  pharmacy.produce_rate = kUnknownChance;
  CHECK(moonclient::PharmacyChancePercent(pharmacy, stats) == kUnknownChance);
}

TEST_CASE("Cuisine : le kit décide, la maîtrise se moyenne", "[craft][chances]") {
  const CrafterStats stats{.base_level = 99, .dex = 50, .luk = 30};
  CHECK(moonclient::CookingChancePercent(moonclient::kSureCookingKit, 20, -1, stats) == 100);
  CHECK(moonclient::CookingChancePercent(12, 11, -1, stats) == kUnknownChance);
  // Hors des plats, ce n'est pas de la cuisine.
  CHECK(moonclient::CookingChancePercent(30, 11, 0, stats) == kUnknownChance);
  // 2400 + 2000 + 1020 - 400 - 710 = 4310 ; les deux tirages ajoutent en
  // moyenne 1750 puis retirent 250.
  CHECK(moonclient::CookingChancePercent(12, 11, 0, stats) == 58);
}

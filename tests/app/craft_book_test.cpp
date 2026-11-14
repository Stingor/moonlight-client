// LE LIVRE DES RECETTES : ce que la lecture retient du fichier, et l'index
// inverse qu'elle bâtit.
//
// 🔴 CE BANC TIENT CE QUE LE JEU NE MONTRE PAS. Une recette perdue à la lecture
// ne fait aucun bruit : l'Atlas montre simplement une ligne de moins, et le
// joueur conclut que l'item ne se fabrique pas. Le texte est donc inventé, et
// chaque règle de la lecture s'y trouve en miniature — le doublon du serveur, le
// guide non consommé, la fabrication par item, le skill qu'aucune classe
// n'apprend, les flèches dont la clé est la source.

#include "items/craft_book.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <catch2/catch_test_macros.hpp>

using moonclient::CraftBook;
using moonclient::CraftYieldMode;

namespace {

constexpr std::uint32_t kSword = 1101;
constexpr std::uint32_t kKnife = 1201;
constexpr std::uint32_t kDish = 12041;
constexpr std::uint32_t kIronOre = 998;
constexpr std::uint32_t kJellopy = 909;
constexpr std::uint32_t kGuide = 7433;
constexpr std::uint32_t kEmperium = 714;
constexpr std::uint32_t kSilverArrow = 1757;
constexpr std::uint32_t kFireArrow = 1752;
constexpr std::uint32_t kWhitePotion = 504;
constexpr int kSmithSword = 99;
constexpr int kSmithDagger = 98;
constexpr int kPharmacy = 228;
constexpr int kRuneMastery = 2010;
constexpr int kTwilight = 496;

constexpr std::string_view kBookText = R"(version: 1
recipes:
  - {id: 1101, lv: 1, skill: 99, skill_lv: 1, mats: [[998, 2]]}
  - {id: 1201, lv: 1, skill: 98, skill_lv: 1, mats: [[998, 1], [909, 10]]}
  - {id: 1101, lv: 3, skill: 98, skill_lv: 3, mats: [[909, 99]]}
  - {id: 12041, lv: 11, skill: 0, skill_lv: 0, mats: [[7433, 0], [909, 3]]}
  - {id: 504, lv: 0, skill: 228, skill_lv: 1, mats: [[909, 1]]}
  - {lv: 1, skill: 99, mats: [[998, 1]]}
  - {id: 2001, lv: 1, skill: 2010, skill_lv: 1, mats: [[998], [909, 1]]}
arrows:
  - {src: 714, yields: [[1757, 600], [1752, 600]]}
  - {src: 998, yields: [[1752, 10]]}
  - {src: 999, yields: []}
by_skill:
  99: [1101]
  98: [1201]
  0: [12041]
  228: [504]
  2010: [2001]
  5000: []
server:
  renewal: false
  unavailable_skills: [2010]
produce_qty:
  0: {min: 1, max: 5, mode: bonus_roll, chance_permille: 162}
  2010: {min: 2, max: 6, mode: per_unit}
  98: {min: 1, max: 9, mode: inconnu}
bulk_recipes:
  496: [[504, 200, 200]]
  497: [[504]]
)";

[[nodiscard]] CraftBook Read(std::string_view text = kBookText) {
  ro::core::Result<CraftBook> book = CraftBook::Parse(text);
  REQUIRE(book.has_value());
  return *std::move(book);
}

}  // namespace

TEST_CASE("Recettes : une recette se lit, ses matériaux dans l'ordre", "[app][recettes]") {
  const CraftBook book = Read();
  const moonclient::CraftRecipe* knife = book.RecipeOf(kKnife);
  REQUIRE(knife != nullptr);
  CHECK(knife->skill_id == kSmithDagger);
  CHECK(knife->skill_level == 1);
  REQUIRE(knife->ingredients.size() == 2);
  CHECK(knife->ingredients[0].item_id == kIronOre);
  CHECK(knife->ingredients[1].item_id == kJellopy);
  CHECK(knife->ingredients[1].quantity == 10);
  CHECK_FALSE(knife->ingredients[1].kept);
}

TEST_CASE("Recettes : le PREMIER doublon d'un produit gagne", "[app][recettes]") {
  const CraftBook book = Read();
  const moonclient::CraftRecipe* sword = book.RecipeOf(kSword);
  REQUIRE(sword != nullptr);
  CHECK(sword->skill_id == kSmithSword);
  CHECK(sword->item_level == 1);
  // Le doublon ne compte pas non plus dans l'index inverse : son matériau
  // n'annonce pas une recette que l'Atlas ne montrera jamais.
  for (const int rank : book.RecipesUsing(kJellopy)) {
    CHECK(book.recipes()[static_cast<std::size_t>(rank)].item_level != 3);
  }
}

TEST_CASE("Recettes : une quantité nulle est un guide, pas une absence", "[app][recettes]") {
  const CraftBook book = Read();
  const moonclient::CraftRecipe* dish = book.RecipeOf(kDish);
  REQUIRE(dish != nullptr);
  REQUIRE(dish->ingredients.size() == 2);
  CHECK(dish->ingredients[0].item_id == kGuide);
  CHECK(dish->ingredients[0].kept);
  CHECK(dish->skill_id == moonclient::kCraftByItemSkill);
  CHECK(moonclient::IsDishItemLevel(dish->item_level));
}

TEST_CASE("Recettes : une ligne sans produit ou une paire tronquée se sautent", "[app][recettes]") {
  const CraftBook book = Read();
  // Sept entrées dans le fichier : un doublon, une sans identifiant.
  CHECK(book.recipes().size() == 5);
  const moonclient::CraftRecipe* rune = book.RecipeOf(2001);
  REQUIRE(rune != nullptr);
  REQUIRE(rune->ingredients.size() == 1);
  CHECK(rune->ingredients[0].item_id == kJellopy);
}

TEST_CASE("Recettes : l'index inverse répond « ça sert à quoi ? »", "[app][recettes]") {
  const CraftBook book = Read();
  std::unordered_map<std::uint32_t, int> products;
  for (const int rank : book.RecipesUsing(kJellopy)) {
    ++products[book.recipes()[static_cast<std::size_t>(rank)].product_id];
  }
  CHECK(products.size() == 4);
  CHECK(products[kKnife] == 1);
  CHECK(products[kDish] == 1);
  CHECK(products[kWhitePotion] == 1);
  CHECK(products[2001] == 1);
  CHECK(book.RecipesUsing(kSword).empty());
  CHECK(book.material_count() == 3);
}

TEST_CASE("Recettes : les skills qui fabriquent, zéro compris, triés", "[app][recettes]") {
  const CraftBook book = Read();
  const std::span<const int> skills = book.skills();
  REQUIRE(skills.size() == 5);
  CHECK(skills.front() == moonclient::kCraftByItemSkill);
  CHECK(std::ranges::is_sorted(skills));
  // Un skill à la liste vide ne s'inscrit pas.
  CHECK(book.ProductsOfSkill(5000).empty());
  REQUIRE(book.ProductsOfSkill(kSmithDagger).size() == 1);
  CHECK(book.ProductsOfSkill(kSmithDagger)[0] == kKnife);
}

TEST_CASE("Recettes : un skill qu'aucune classe n'apprend se dit injouable", "[app][recettes]") {
  const CraftBook book = Read();
  CHECK_FALSE(book.SkillIsLearnable(kRuneMastery));
  CHECK(book.SkillIsLearnable(kSmithSword));
  // La fabrication par item ne s'apprend pas : elle reste ouverte.
  CHECK(book.SkillIsLearnable(moonclient::kCraftByItemSkill));
}

TEST_CASE("Recettes : sans la liste du serveur, dans le doute on MONTRE", "[app][recettes]") {
  const CraftBook book = Read("recipes:\n  - {id: 1101, lv: 1, skill: 2010, mats: [[998, 1]]}\n");
  CHECK(book.SkillIsLearnable(kRuneMastery));
}

TEST_CASE("Recettes : le rendement, UN par défaut et un mode inconnu se tait", "[app][recettes]") {
  const CraftBook book = Read();
  const moonclient::CraftYieldRule dish = book.YieldOf(moonclient::kCraftByItemSkill);
  CHECK(dish.mode == CraftYieldMode::kBonusRoll);
  CHECK(dish.most == 5);
  CHECK(dish.bonus_permille == 162);
  const moonclient::CraftYieldRule rune = book.YieldOf(kRuneMastery);
  CHECK(rune.mode == CraftYieldMode::kPerUnit);
  CHECK(rune.least == 2);
  CHECK(rune.most == 6);
  CHECK(book.YieldOf(kSmithDagger).IsSingle());
  CHECK(book.YieldOf(kPharmacy).IsSingle());
}

TEST_CASE("Recettes : la production en masse, par item", "[app][recettes]") {
  const CraftBook book = Read();
  const std::span<const moonclient::CraftBulkSource> sources = book.BulkSourcesOf(kWhitePotion);
  REQUIRE(sources.size() == 1);
  CHECK(sources[0].skill_id == kTwilight);
  CHECK(sources[0].least == 200);
  CHECK(sources[0].most == 200);
  CHECK(book.BulkSourcesOf(kKnife).empty());
}

TEST_CASE("Recettes : les flèches se lisent par la SOURCE, et à l'envers", "[app][recettes]") {
  const CraftBook book = Read();
  const moonclient::CraftArrowRecipe* emperium = book.ArrowFrom(kEmperium);
  REQUIRE(emperium != nullptr);
  REQUIRE(emperium->yields.size() == 2);
  CHECK(emperium->yields[0].item_id == kSilverArrow);
  CHECK(emperium->yields[0].quantity == 600);
  // Une source sans rendement ne dit rien, et ne s'inscrit pas.
  CHECK(book.ArrowFrom(999) == nullptr);
  CHECK(book.arrows().size() == 2);
  CHECK(book.ArrowsYielding(kFireArrow).size() == 2);
  CHECK(book.ArrowsYielding(kSilverArrow).size() == 1);
}

TEST_CASE("Recettes : un texte illisible ou sans recette est refusé", "[app][recettes]") {
  CHECK_FALSE(CraftBook::Parse("recipes: [").has_value());
  CHECK_FALSE(CraftBook::Parse("- une liste").has_value());
  CHECK_FALSE(CraftBook::Parse("version: 1\n").has_value());
}

TEST_CASE("Recettes : ce que le sac permet, guides compris", "[app][recettes]") {
  const CraftBook book = Read();
  std::unordered_map<std::uint32_t, int> bag;
  const auto owned = [&bag](std::uint32_t item_id) {
    const auto found = bag.find(item_id);
    return found == bag.end() ? 0 : found->second;
  };
  const moonclient::CraftRecipe& dish = *book.RecipeOf(kDish);
  bag[kJellopy] = 10;
  // 🔴 SANS LE GUIDE, RIEN : il ne se consomme pas, mais son absence interdit tout.
  CHECK(moonclient::CraftableCount(dish, owned) == 0);
  bag[kGuide] = 1;
  CHECK(moonclient::CraftableCount(dish, owned) == 3);

  const moonclient::CraftRecipe& knife = *book.RecipeOf(kKnife);
  bag[kIronOre] = 5;
  CHECK(moonclient::CraftableCount(knife, owned) == 1);
  bag[kJellopy] = 50;
  CHECK(moonclient::CraftableCount(knife, owned) == 5);
}

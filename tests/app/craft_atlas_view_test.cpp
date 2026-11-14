// L'ATLAS, VU DE LA PARTIE : ce que la vue compose du livre et du sac.
//
// 🔴 LES DEUX CHIFFRES D'UN MATÉRIAU DOIVENT ÊTRE LE MÊME. La liste annonce
// « sert dans N recettes » et la fiche en détaille la liste : si l'un compte les
// recettes injouables et l'autre non, le joueur cherche celles qui manquent.
// Ce banc tient aussi le reste de ce que le réglage « injouables » masque, et
// l'étiquette « Craft » d'une description.
//
// ⚠ SANS TABLE DES ITEMS : les noms sont alors « #id », ce qui suffit à vérifier
// le tri — il compare des chaînes.

#include "items/craft_atlas_view.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

using moonclient::CraftAtlasFilters;
using moonclient::CraftAtlasView;
using moonclient::CraftBook;

namespace {

constexpr std::uint32_t kSword = 1101;
constexpr std::uint32_t kRune = 2001;
constexpr std::uint32_t kDish = 12041;
constexpr std::uint32_t kIronOre = 998;
constexpr std::uint32_t kJellopy = 909;
constexpr std::uint32_t kGuide = 7433;
constexpr std::uint32_t kEmperium = 714;
constexpr std::uint32_t kSilverArrow = 1757;
constexpr std::uint16_t kFirstSlot = 2;
constexpr std::int16_t kJellopyStock = 7;

constexpr std::string_view kBookText = R"(recipes:
  - {id: 1101, lv: 1, skill: 99, skill_lv: 1, mats: [[998, 2], [909, 1]]}
  - {id: 12041, lv: 11, skill: 0, skill_lv: 0, mats: [[7433, 0], [909, 3]]}
  - {id: 2001, lv: 1, skill: 2010, skill_lv: 1, mats: [[909, 1]]}
arrows:
  - {src: 714, yields: [[1757, 600]]}
by_skill:
  99: [1101]
  0: [12041]
  2010: [2001]
server:
  unavailable_skills: [2010]
)";

struct AtlasProbe {
  CraftAtlasView atlas{nullptr, nullptr, nullptr};
  ro::sim::Inventory bag;
  ro::ui::CraftAtlasWindowState state;

  AtlasProbe() {
    ro::core::Result<CraftBook> book = CraftBook::Parse(kBookText);
    REQUIRE(book.has_value());
    atlas.UseBook(*std::move(book));
    bag.Put(ro::sim::InventoryItem{
        .client_slot = kFirstSlot, .item_id = kJellopy, .amount = kJellopyStock});
    state.open = true;
  }

  const ro::ui::CraftAtlasView& Refresh(bool show_unlearnable = false) {
    atlas.Refresh(state, bag,
                  CraftAtlasFilters{.show_unlearnable = show_unlearnable, .can_share = true});
    return atlas.view();
  }
};

[[nodiscard]] bool Lists(std::span<const ro::ui::CraftAtlasRow> rows, std::uint32_t item_id) {
  return std::ranges::any_of(
      rows, [item_id](const ro::ui::CraftAtlasRow& row) { return row.item_id == item_id; });
}

}  // namespace

TEST_CASE("Atlas : sans livre, la vue dit l'absence", "[app][atlas]") {
  CraftAtlasView atlas(nullptr, nullptr, nullptr);
  const ro::sim::Inventory bag;
  ro::ui::CraftAtlasWindowState state;
  atlas.Refresh(state, bag, CraftAtlasFilters{});
  CHECK_FALSE(atlas.view().available);
  CHECK(atlas.view().products.empty());
  std::string label;
  std::string tip;
  CHECK_FALSE(atlas.CraftTagOf(kJellopy, &label, &tip));
}

TEST_CASE("Atlas : les produits se trient par nom, les injouables masqués", "[app][atlas]") {
  AtlasProbe probe;
  const ro::ui::CraftAtlasView& view = probe.Refresh();
  REQUIRE(view.available);
  REQUIRE(view.products.size() == 2);
  CHECK(view.products[0].name == "#1101");
  CHECK(view.products[1].name == "#12041");
  CHECK_FALSE(Lists(view.products, kRune));
  // Le groupe du skill injouable disparaît en entier.
  CHECK(view.groups.size() == 2);

  const ro::ui::CraftAtlasView& all = probe.Refresh(/*show_unlearnable=*/true);
  CHECK(Lists(all.products, kRune));
  CHECK(all.groups.size() == 3);
}

TEST_CASE("Atlas : le groupe des plats se nomme par ce qu'il contient", "[app][atlas]") {
  AtlasProbe probe;
  const ro::ui::CraftAtlasView& view = probe.Refresh();
  REQUIRE_FALSE(view.groups.empty());
  CHECK(view.groups[0].skill_id == moonclient::kCraftByItemSkill);
  CHECK(view.groups[0].label == "Cuisine (kit)");
  REQUIRE(view.groups[0].products.size() == 1);
  CHECK(view.groups[0].products[0].item_id == kDish);
}

TEST_CASE("Atlas : un matériau compte les MÊMES recettes dans la liste et la fiche",
          "[app][atlas]") {
  AtlasProbe probe;
  probe.state.selected_id = kJellopy;
  const ro::ui::CraftAtlasView& view = probe.Refresh();
  const auto jellopy = std::ranges::find(view.materials, kJellopy, &ro::ui::CraftAtlasRow::item_id);
  REQUIRE(jellopy != view.materials.end());
  CHECK(jellopy->uses == 2);
  CHECK(jellopy->owned == kJellopyStock);
  CHECK(view.sheet.uses.size() == 2);
  CHECK_FALSE(Lists(view.sheet.uses, kRune));
}

TEST_CASE("Atlas : la fiche d'un produit dit sa recette et ce que le sac permet", "[app][atlas]") {
  AtlasProbe probe;
  probe.state.selected_id = kSword;
  const ro::ui::CraftAtlasView& view = probe.Refresh();
  REQUIRE(view.sheet.item.item_id == kSword);
  REQUIRE(view.sheet.recipe.has_value());
  CHECK(view.sheet.recipe->skill_level == 1);
  // Pas de minerai : rien à fabriquer, et la fiche le dit par un zéro.
  CHECK(view.sheet.recipe->craftable == 0);
  REQUIRE(view.sheet.recipe->ingredients.size() == 2);
  CHECK(view.sheet.recipe->ingredients[0].item_id == kIronOre);
  CHECK(view.sheet.recipe->ingredients[0].quantity == 2);
  CHECK(view.sheet.recipe->ingredients[1].owned == kJellopyStock);
  CHECK(view.can_share_recipe);
}

TEST_CASE("Atlas : la fiche d'une source de flèches, et celle de la flèche", "[app][atlas]") {
  AtlasProbe probe;
  probe.state.selected_id = kEmperium;
  const ro::ui::CraftAtlasView& source = probe.Refresh();
  REQUIRE(source.sheet.arrow_yields.size() == 1);
  CHECK(source.sheet.arrow_yields[0].item_id == kSilverArrow);
  CHECK(source.sheet.arrow_yields[0].quantity == 600);
  CHECK_FALSE(source.can_share_recipe);

  probe.state.selected_id = kSilverArrow;
  const ro::ui::CraftAtlasView& arrow = probe.Refresh();
  REQUIRE(arrow.sheet.arrow_sources.size() == 1);
  CHECK(arrow.sheet.arrow_sources[0].item_id == kEmperium);
  CHECK(arrow.sheet.arrow_sources[0].quantity == 600);
}

TEST_CASE("Atlas : l'étiquette « Craft » annonce d'abord la recette", "[app][atlas]") {
  AtlasProbe probe;
  std::string label;
  std::string tip;
  REQUIRE(probe.atlas.CraftTagOf(kDish, &label, &tip));
  CHECK(label == "Craft : Cuisine (kit)");
  REQUIRE(probe.atlas.CraftTagOf(kJellopy, &label, &tip));
  CHECK(label == "Craft : 3 recettes");
  CHECK(tip.find("Entre dans 3 recettes.") != std::string::npos);
  REQUIRE(probe.atlas.CraftTagOf(kSilverArrow, &label, &tip));
  CHECK(label == "Craft : flèches");
  // 🔴 ON N'ANNONCE QUE LE POSITIF : rien pour un item étranger à la fabrication.
  CHECK_FALSE(probe.atlas.CraftTagOf(kSword + 1, &label, &tip));
}

TEST_CASE("Atlas : l'aperçu d'un lien de recette se consomme, et se tait sans recette",
          "[app][atlas]") {
  AtlasProbe probe;
  CHECK(probe.atlas.TakePreview() == nullptr);
  probe.atlas.NoteHoveredRecipe(kDish);
  const ro::ui::CraftRecipePreview* preview = probe.atlas.TakePreview();
  REQUIRE(preview != nullptr);
  CHECK(preview->product_name == "#12041");
  CHECK(preview->skill_label == "Cuisine (kit)");
  REQUIRE(preview->ingredients.size() == 2);
  CHECK(preview->ingredients[0].kept);
  CHECK(probe.atlas.TakePreview() == nullptr);
  probe.atlas.NoteHoveredRecipe(kSilverArrow);
  CHECK(probe.atlas.TakePreview() == nullptr);
}

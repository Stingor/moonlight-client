#include "game/craft_atlas_bug_report.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <format>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/core/text.h"

namespace moonclient {
namespace {

using ro::ui::CraftAtlasRecipeView;
using ro::ui::CraftAtlasRow;
using ro::ui::CraftAtlasSheet;
using ro::ui::CraftAtlasTab;
using ro::ui::CraftAtlasYieldMode;

// Le libellé d'un rapport — « Atlas des recettes : Steel (#999) » —, jamais
// plus long.
constexpr std::size_t kLabelBytes = 192;
// Les ingrédients que le rapport cite. ⚠ UNE BORNE : le contexte tient en un
// kilo-octet côté serveur, et une recette en compte rarement davantage.
constexpr std::size_t kIngredientsCited = 4;
// La recherche, le nom de l'item et celui du skill, en octets avant
// échappement.
constexpr std::size_t kSearchBytes = 20;
constexpr std::size_t kNameBytes = 32;
constexpr std::size_t kSkillBytes = 24;
// Le manque se mesure pour UNE fabrication : celle que la fiche annonce.
constexpr int kCraftsMeasured = 1;

[[nodiscard]] const char* Flag(bool value) { return value ? "true" : "false"; }

// Un texte borné, sans caractère coupé en queue.
[[nodiscard]] std::string Clipped(std::string_view text, std::size_t most_bytes) {
  return JsonEscaped(ro::core::WithoutDanglingUtf8(text.substr(0, most_bytes)));
}

[[nodiscard]] const char* TabWord(CraftAtlasTab tab) {
  switch (tab) {
    case CraftAtlasTab::kSkills:
      return "skills";
    case CraftAtlasTab::kProducts:
      return "products";
    case CraftAtlasTab::kMaterials:
      return "materials";
    case CraftAtlasTab::kArrows:
      return "arrows";
  }
  return "?";
}

[[nodiscard]] const char* YieldWord(CraftAtlasYieldMode mode) {
  switch (mode) {
    case CraftAtlasYieldMode::kFixed:
      return "fixed";
    case CraftAtlasYieldMode::kPerUnit:
      return "per_unit";
    case CraftAtlasYieldMode::kAllOrNone:
      return "all_or_none";
    case CraftAtlasYieldMode::kBonusRoll:
      return "bonus_roll";
    case CraftAtlasYieldMode::kTable:
      return "table";
  }
  return "?";
}

// La recette de l'item choisi, ingrédients bornés.
[[nodiscard]] std::string RecipeFields(const CraftAtlasRecipeView& recipe) {
  std::string fields = std::format(
      R"("recipe":{{"skill_id":{},"skill":"{}","skill_level":{},"item_level":{},"learnable":{},)"
      R"("yield":{{"mode":"{}","least":{},"most":{})",
      recipe.skill_id, Clipped(recipe.skill_label, kSkillBytes), recipe.skill_level,
      recipe.item_level, Flag(recipe.learnable), YieldWord(recipe.yield_mode), recipe.yield_least,
      recipe.yield_most);
  // Le bonus ne vaut que pour le tirage qui le porte.
  if (recipe.yield_mode == CraftAtlasYieldMode::kBonusRoll) {
    fields += std::format(R"(,"bonus_permille":{})", recipe.yield_bonus_permille);
  }
  fields += std::format(R"(}},"craftable":{},"ingredients":[)", recipe.craftable);
  const std::size_t cited = std::min(recipe.ingredients.size(), kIngredientsCited);
  for (std::size_t index = 0; index < cited; ++index) {
    const CraftAtlasRow& ingredient = recipe.ingredients[index];
    fields += std::format(
        R"({}{{"item_id":{},"need":{},"owned":{},"missing":{}{}}})", index == 0 ? "" : ",",
        ingredient.item_id, ro::ui::CraftIngredientNeed(ingredient, kCraftsMeasured),
        ingredient.owned, ro::ui::CraftIngredientShortfall(ingredient, kCraftsMeasured),
        ingredient.kept ? R"(,"kept":true)" : "");
  }
  fields += std::format(R"(],"more":{}}})", recipe.ingredients.size() - cited);
  return fields;
}

// La fiche de l'item choisi.
[[nodiscard]] std::string SheetFields(const CraftAtlasSheet& sheet) {
  std::string fields =
      std::format(R"("sheet":{{"item_id":{},"name":"{}","owned":{},)", sheet.item.item_id,
                  Clipped(sheet.item.name, kNameBytes), sheet.item.owned);
  if (sheet.recipe.has_value()) {
    fields += RecipeFields(*sheet.recipe);
  } else {
    fields += R"("recipe":null)";
  }
  fields += std::format(R"(,"bulk":{},"uses":{},"arrow_yields":{},"arrow_sources":{}}})",
                        sheet.bulk.size(), sheet.uses.size(), sheet.arrow_yields.size(),
                        sheet.arrow_sources.size());
  return fields;
}

}  // namespace

BugReportContext CraftAtlasBugReport(const ro::ui::CraftAtlasWindowState& window,
                                     const ro::ui::CraftAtlasView& view) {
  const std::string_view search(window.filter.data());
  const std::string free_slots =
      view.free_bag_slots.has_value() ? std::to_string(*view.free_bag_slots) : "null";
  std::string fields =
      std::format(R"("craft_atlas":{{"available":{},"tab":"{}","search":"{}","only_craftable":{},)"
                  R"("selected_id":{},"back":{},"staff":{},"free_bag_slots":{})",
                  Flag(view.available), TabWord(window.tab), Clipped(search, kSearchBytes),
                  Flag(view.only_craftable), window.selected_id, window.back.size(),
                  Flag(view.staff_items), free_slots);
  // Le nombre de fabrications que le staff prépare : ce que « Obtenir » demande.
  if (view.staff_items) {
    fields += std::format(R"(,"staff_crafts":{})", window.staff_crafts);
  }
  const bool sheet_shown = view.sheet.item.item_id != 0;
  if (sheet_shown) {
    fields += ',';
    fields += SheetFields(view.sheet);
  }
  fields += '}';

  // Le libellé : l'item choisi, sinon la recherche, sinon l'Atlas seul.
  std::array<char, kLabelBytes> label{};
  if (sheet_shown) {
    const std::string name(
        ro::core::WithoutDanglingUtf8(view.sheet.item.name.substr(0, kNameBytes)));
    std::snprintf(label.data(), label.size(), ro::core::Tr("Atlas des recettes : %s (#%u)"),
                  name.empty() ? "?" : name.c_str(),
                  static_cast<unsigned>(view.sheet.item.item_id));
  } else if (!search.empty()) {
    std::snprintf(
        label.data(), label.size(), ro::core::Tr("Atlas des recettes : recherche « %s »"),
        std::string(ro::core::WithoutDanglingUtf8(search.substr(0, kSearchBytes))).c_str());
  } else {
    std::snprintf(label.data(), label.size(), "%s", ro::core::Tr("Atlas des recettes"));
  }
  return FieldsBugReport(label.data(), fields);
}

}  // namespace moonclient

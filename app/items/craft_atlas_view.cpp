#include "items/craft_atlas_view.h"

#include <algorithm>
#include <string>
#include <utility>

#include <imgui.h>

#include "ro/core/ascii.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"

namespace moonclient {
namespace {

[[nodiscard]] std::string Numbered(unsigned number) { return ro::core::Printed("#%u", number); }

[[nodiscard]] ro::ui::CraftAtlasYieldMode ModeOf(CraftYieldMode mode) {
  switch (mode) {
    case CraftYieldMode::kFixed:
      return ro::ui::CraftAtlasYieldMode::kFixed;
    case CraftYieldMode::kPerUnit:
      return ro::ui::CraftAtlasYieldMode::kPerUnit;
    case CraftYieldMode::kAllOrNone:
      return ro::ui::CraftAtlasYieldMode::kAllOrNone;
    case CraftYieldMode::kBonusRoll:
      return ro::ui::CraftAtlasYieldMode::kBonusRoll;
    case CraftYieldMode::kTable:
      return ro::ui::CraftAtlasYieldMode::kTable;
  }
  return ro::ui::CraftAtlasYieldMode::kFixed;
}

}  // namespace

CraftAtlasView::CraftAtlasView(const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons,
                               const ro::sim::SkillTree* skill_tree)
    : items_(items), icons_(icons), skill_tree_(skill_tree) {}

void CraftAtlasView::Load(ro::vfs::Vfs& vfs) {
  for (const std::string_view path : kCraftBookPaths) {
    auto bytes = vfs.Read(path);
    if (!bytes) {
      continue;
    }
    const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    ro::core::Result<CraftBook> book = CraftBook::Parse(text);
    if (!book) {
      ro::core::LogWarning("atlas des recettes : {} — {}", path, ro::core::Describe(book.error()));
      return;
    }
    ro::core::LogInfo("atlas des recettes : {} — {} recettes, {} recettes de flèches, {} skills",
                      path, book->recipes().size(), book->arrows().size(), book->skills().size());
    UseBook(std::move(*book));
    return;
  }
  // ⚠ NON FATAL, ET NON SILENCIEUX : le fichier arrive avec le patch, et un
  // joueur peut ne pas l'avoir encore.
  ro::core::LogWarning("atlas des recettes : {} introuvable, l'Atlas restera vide",
                       kCraftBookPaths.front());
}

void CraftAtlasView::UseBook(CraftBook book) {
  book_ = std::move(book);
  indexed_ = false;
}

std::string_view CraftAtlasView::NameOf(std::uint32_t item_id) {
  auto [found, fresh] = names_.try_emplace(item_id);
  if (fresh) {
    const ro::vfs::ItemEntry* entry = items_ != nullptr ? items_->Find(item_id) : nullptr;
    found->second = entry != nullptr && !entry->name.empty() ? entry->name : Numbered(item_id);
  }
  return found->second;
}

std::string_view CraftAtlasView::SkillLabel(int skill_id, int item_level) {
  if (skill_id == kCraftByItemSkill) {
    // 🔴 UN `itemlv` DE PLAT dit un plat : c'est le test du serveur lui-même.
    return IsDishItemLevel(item_level) ? ro::core::Tr("Cuisine (kit)")
                                       : ro::core::Tr("Sans skill (item ou script)");
  }
  auto [found, fresh] = skill_names_.try_emplace(skill_id);
  if (fresh) {
    const ro::sim::SkillNode* node =
        skill_tree_ != nullptr ? skill_tree_->Find(static_cast<std::uint16_t>(skill_id)) : nullptr;
    found->second = node != nullptr && !node->display_name.empty()
                        ? node->display_name
                        : Numbered(static_cast<unsigned>(skill_id));
  }
  return found->second;
}

bool CraftAtlasView::CraftTagOf(std::uint32_t item_id, std::string* label, std::string* tip) {
  if (!book_.has_value() || item_id == 0) {
    return false;
  }
  const CraftRecipe* recipe = book_->RecipeOf(item_id);
  const auto used_in = static_cast<int>(book_->RecipesUsing(item_id).size());
  const bool arrows =
      book_->ArrowFrom(item_id) != nullptr || !book_->ArrowsYielding(item_id).empty();
  if (recipe == nullptr && used_in == 0 && !arrows) {
    return false;
  }
  std::string text;
  if (recipe != nullptr) {
    const std::string_view skill = SkillLabel(recipe->skill_id, recipe->item_level);
    text = ro::core::Printed(ro::core::Tr("Craft : %.*s"), static_cast<int>(skill.size()),
                             skill.data());
  } else if (used_in > 0) {
    text = ro::core::Printed(ro::core::Tr("Craft : %d recettes"), used_in);
  } else {
    text = ro::core::Printed("%s", ro::core::Tr("Craft : flèches"));
  }
  *label = text;

  // L'INFOBULLE DIT LES TROIS CHOSES D'UN COUP : un item peut être à la fois
  // fabricable, matériau et transformable en flèches.
  tip->clear();
  if (recipe != nullptr) {
    *tip += ro::core::Tr("Se fabrique");
    if (recipe->skill_id != kCraftByItemSkill) {
      *tip += " (";
      *tip += SkillLabel(recipe->skill_id, recipe->item_level);
      *tip += ")";
    }
    *tip += ".\n";
  }
  if (used_in > 0) {
    text = ro::core::Printed(ro::core::Tr("Entre dans %d recettes."), used_in);
    *tip += text;
    *tip += '\n';
  }
  if (arrows) {
    *tip += ro::core::Tr("Concerné par la fabrication de flèches.");
    *tip += '\n';
  }
  *tip += '\n';
  *tip += ro::core::Tr("Clique pour l'ouvrir dans l'Atlas des recettes.");
  return true;
}

void CraftAtlasView::NoteDrawn(std::span<const std::uint32_t> item_ids) {
  if (icons_ == nullptr) {
    return;
  }
  if (icons_->generation() != icons_generation_) {
    icons_generation_ = icons_->generation();
    icons_seen_.clear();
  }
  for (const std::uint32_t item_id : item_ids) {
    if (icons_seen_.contains(item_id)) {
      continue;
    }
    const ro::render::IconTexture& icon = icons_->Get(item_id);
    icons_seen_.emplace(
        item_id,
        ro::ui::SkinImage{
            .id = icon.usable() ? static_cast<ImTextureID>(icon.texture.name()) : ImTextureID{0},
            .width = static_cast<float>(icon.width_px),
            .height = static_cast<float>(icon.height_px)});
  }
}

const ro::ui::CraftRecipePreview* CraftAtlasView::TakePreview() {
  const std::uint32_t product_id = std::exchange(hovered_recipe_, 0);
  const CraftRecipe* recipe =
      book_.has_value() && product_id != 0 ? book_->RecipeOf(product_id) : nullptr;
  if (recipe == nullptr) {
    return nullptr;
  }
  preview_ingredients_.clear();
  for (const CraftIngredient& ingredient : recipe->ingredients) {
    preview_ingredients_.push_back(ro::ui::CraftAtlasRow{.item_id = ingredient.item_id,
                                                         .name = NameOf(ingredient.item_id),
                                                         .quantity = ingredient.quantity,
                                                         .kept = ingredient.kept});
  }
  preview_ =
      ro::ui::CraftRecipePreview{.product_name = NameOf(product_id),
                                 .skill_id = recipe->skill_id,
                                 .skill_label = SkillLabel(recipe->skill_id, recipe->item_level),
                                 .skill_level = recipe->skill_level,
                                 .ingredients = preview_ingredients_};
  return &preview_;
}

void CraftAtlasView::BuildIndex() {
  indexed_ = true;
  sorted_products_.clear();
  sorted_materials_.clear();
  sorted_arrow_sources_.clear();
  if (!book_.has_value()) {
    return;
  }
  for (const CraftRecipe& recipe : book_->recipes()) {
    sorted_products_.push_back(recipe.product_id);
    for (const CraftIngredient& ingredient : recipe.ingredients) {
      sorted_materials_.push_back(ingredient.item_id);
    }
  }
  for (const CraftArrowRecipe& arrow : book_->arrows()) {
    sorted_arrow_sources_.push_back(arrow.source_id);
  }
  // PAR NOM, puis par identifiant pour départager deux homonymes.
  const auto by_name = [this](std::uint32_t left, std::uint32_t right) {
    const int compared = ro::core::AsciiCompareFolded(NameOf(left), NameOf(right));
    return compared != 0 ? compared < 0 : left < right;
  };
  for (std::vector<std::uint32_t>* sorted :
       {&sorted_products_, &sorted_materials_, &sorted_arrow_sources_}) {
    std::ranges::sort(*sorted, by_name);
    const auto [first, last] = std::ranges::unique(*sorted);
    sorted->erase(first, last);
  }
}

ro::ui::CraftAtlasRow CraftAtlasView::RowOf(std::uint32_t item_id) {
  const auto icon = icons_seen_.find(item_id);
  return ro::ui::CraftAtlasRow{
      .item_id = item_id,
      .name = NameOf(item_id),
      .icon = icon != icons_seen_.end() ? icon->second : ro::ui::SkinImage{},
      .owned = Owned(item_id)};
}

void CraftAtlasView::NoteStaff(bool staff_tools, const ro::vfs::ItemStacking& stacking,
                               std::optional<int> free_bag_slots) {
  view_.staff_items = staff_tools;
  view_.free_bag_slots = free_bag_slots;
  // ⚠ EN PLACE : la recette de la fiche désigne ces lignes, elle ne les copie pas.
  for (ro::ui::CraftAtlasRow& ingredient : ingredients_) {
    ingredient.stackable = stacking.IsStackable(ingredient.item_id);
  }
}

int CraftAtlasView::Owned(std::uint32_t item_id) const {
  const auto found = owned_.find(item_id);
  return found == owned_.end() ? 0 : found->second;
}

bool CraftAtlasView::Playable(const CraftRecipe& recipe, const CraftAtlasFilters& filters) const {
  return filters.show_unlearnable || book_->SkillIsLearnable(recipe.skill_id);
}

int CraftAtlasView::PlayableUses(std::uint32_t material_id,
                                 const CraftAtlasFilters& filters) const {
  const std::span<const CraftRecipe> recipes = book_->recipes();
  return static_cast<int>(std::ranges::count_if(book_->RecipesUsing(material_id), [&](int rank) {
    return Playable(recipes[static_cast<std::size_t>(rank)], filters);
  }));
}

void CraftAtlasView::Refresh(const ro::ui::CraftAtlasWindowState& state,
                             const ro::sim::Inventory& bag, const CraftAtlasFilters& filters) {
  view_ = ro::ui::CraftAtlasView{};
  view_.only_craftable = filters.only_craftable;
  if (!book_.has_value()) {
    return;
  }
  view_.available = true;
  if (!indexed_) {
    BuildIndex();
  }
  // UN SEUL PARCOURS DU SAC PAR IMAGE : l'Atlas montre le stock sur presque
  // chaque ligne.
  owned_.clear();
  for (const ro::sim::InventoryItem& item : bag.items()) {
    if (item.amount > 0 && item.item_id != 0) {
      owned_[item.item_id] += item.amount;
    }
  }
  ComposeLists(filters);
  ComposeSheet(state.selected_id, filters);
  view_.can_share_recipe = filters.can_share && view_.sheet.recipe.has_value();
}

void CraftAtlasView::ComposeLists(const CraftAtlasFilters& filters) {
  const auto craftable_of = [this](std::uint32_t product_id) {
    const CraftRecipe* recipe = book_->RecipeOf(product_id);
    return recipe == nullptr
               ? -1
               : CraftableCount(*recipe, [this](std::uint32_t id) { return Owned(id); });
  };

  // ── LES SKILLS ────────────────────────────────────────────────────────────
  group_rows_.clear();
  groups_.clear();
  std::vector<std::pair<std::size_t, std::size_t>> spans;
  for (const int skill_id : book_->skills()) {
    // UN SKILL QU'AUCUNE CLASSE N'APPREND ICI n'a que des recettes injouables :
    // le groupe entier disparaît, pas seulement ses lignes.
    if (!filters.show_unlearnable && !book_->SkillIsLearnable(skill_id)) {
      continue;
    }
    const std::span<const std::uint32_t> products = book_->ProductsOfSkill(skill_id);
    // Le libellé d'un groupe sans skill dépend de ce qu'il contient : « Cuisine
    // (kit) » seulement si TOUTES ses recettes sont des plats. Une seule
    // étrangère, et l'on retombe sur le libellé générique — mieux vaut un titre
    // vague qu'un titre faux pour la ligne qui s'y cacherait.
    int group_item_level = 0;
    if (skill_id == kCraftByItemSkill &&
        std::ranges::all_of(products, [this](std::uint32_t product_id) {
          const CraftRecipe* recipe = book_->RecipeOf(product_id);
          return recipe != nullptr && IsDishItemLevel(recipe->item_level);
        })) {
      group_item_level = kDishLeastItemLevel;
    }
    const std::size_t first = group_rows_.size();
    for (const std::uint32_t product_id : products) {
      ro::ui::CraftAtlasRow row = RowOf(product_id);
      row.craftable = craftable_of(product_id);
      group_rows_.push_back(row);
    }
    spans.emplace_back(first, group_rows_.size() - first);
    groups_.push_back(ro::ui::CraftAtlasGroup{
        .skill_id = skill_id, .label = SkillLabel(skill_id, group_item_level), .products = {}});
  }
  // ⚠ LES VUES APRÈS LE REMPLISSAGE : voir la déclaration.
  const std::span<const ro::ui::CraftAtlasRow> all_group_rows(group_rows_);
  for (std::size_t rank = 0; rank < groups_.size(); ++rank) {
    groups_[rank].products = all_group_rows.subspan(spans[rank].first, spans[rank].second);
  }
  view_.groups = groups_;

  // ── LES PRODUITS ──────────────────────────────────────────────────────────
  products_.clear();
  for (const std::uint32_t product_id : sorted_products_) {
    if (!Playable(*book_->RecipeOf(product_id), filters)) {
      continue;
    }
    ro::ui::CraftAtlasRow row = RowOf(product_id);
    row.craftable = craftable_of(product_id);
    products_.push_back(row);
  }
  view_.products = products_;

  // ── LES MATÉRIAUX ─────────────────────────────────────────────────────────
  materials_.clear();
  for (const std::uint32_t material_id : sorted_materials_) {
    const int uses = PlayableUses(material_id, filters);
    if (uses == 0) {
      continue;
    }
    ro::ui::CraftAtlasRow row = RowOf(material_id);
    row.uses = uses;
    materials_.push_back(row);
  }
  view_.materials = materials_;

  // ── LES SOURCES DE FLÈCHES ────────────────────────────────────────────────
  arrow_sources_.clear();
  for (const std::uint32_t source_id : sorted_arrow_sources_) {
    arrow_sources_.push_back(RowOf(source_id));
  }
  view_.arrow_sources = arrow_sources_;
}

void CraftAtlasView::ComposeSheet(std::uint32_t item_id, const CraftAtlasFilters& filters) {
  ingredients_.clear();
  bulk_.clear();
  arrow_yields_.clear();
  uses_.clear();
  sheet_arrow_sources_.clear();
  if (item_id == 0) {
    return;
  }
  ro::ui::CraftAtlasSheet& sheet = view_.sheet;
  sheet.item = RowOf(item_id);

  // ── SA RECETTE ────────────────────────────────────────────────────────────
  //
  // ⚠ MONTRÉE MÊME INJOUABLE : on arrive sur une fiche par un lien ou par un
  // matériau, et la fiche le DIT plutôt que de faire croire que l'item ne se
  // fabrique pas.
  if (const CraftRecipe* recipe = book_->RecipeOf(item_id); recipe != nullptr) {
    for (const CraftIngredient& ingredient : recipe->ingredients) {
      ro::ui::CraftAtlasRow row = RowOf(ingredient.item_id);
      row.quantity = ingredient.quantity;
      row.kept = ingredient.kept;
      ingredients_.push_back(row);
    }
    const CraftYieldRule yield = book_->YieldOf(recipe->skill_id);
    sheet.recipe = ro::ui::CraftAtlasRecipeView{
        .skill_id = recipe->skill_id,
        .skill_label = SkillLabel(recipe->skill_id, recipe->item_level),
        .skill_level = recipe->skill_level,
        .item_level = recipe->item_level,
        .learnable = book_->SkillIsLearnable(recipe->skill_id),
        .yield_mode = ModeOf(yield.mode),
        .yield_least = yield.least,
        .yield_most = yield.most,
        .yield_bonus_permille = yield.bonus_permille,
        .craftable = CraftableCount(*recipe, [this](std::uint32_t id) { return Owned(id); }),
        .ingredients = ingredients_};
  }

  for (const CraftBulkSource& source : book_->BulkSourcesOf(item_id)) {
    bulk_.push_back(ro::ui::CraftAtlasBulkView{
        .skill_label = SkillLabel(source.skill_id), .least = source.least, .most = source.most});
  }
  sheet.bulk = bulk_;

  if (const CraftArrowRecipe* arrow = book_->ArrowFrom(item_id); arrow != nullptr) {
    for (const CraftYield& yield : arrow->yields) {
      ro::ui::CraftAtlasRow row = RowOf(yield.item_id);
      row.quantity = yield.quantity;
      arrow_yields_.push_back(row);
    }
  }
  sheet.arrow_yields = arrow_yields_;

  // ── L'INDEX INVERSE : OÙ CET ITEM SERT ────────────────────────────────────
  const std::span<const CraftRecipe> recipes = book_->recipes();
  for (const int rank : book_->RecipesUsing(item_id)) {
    const CraftRecipe& recipe = recipes[static_cast<std::size_t>(rank)];
    if (!Playable(recipe, filters)) {
      continue;
    }
    ro::ui::CraftAtlasRow row = RowOf(recipe.product_id);
    for (const CraftIngredient& ingredient : recipe.ingredients) {
      if (ingredient.item_id == item_id) {
        row.quantity = ingredient.quantity;
        row.kept = ingredient.kept;
        break;
      }
    }
    row.skill_label = SkillLabel(recipe.skill_id, recipe.item_level);
    uses_.push_back(row);
  }
  sheet.uses = uses_;

  // ── D'OÙ VIENNENT CES FLÈCHES ─────────────────────────────────────────────
  const std::span<const CraftArrowRecipe> arrows = book_->arrows();
  for (const int rank : book_->ArrowsYielding(item_id)) {
    const CraftArrowRecipe& arrow = arrows[static_cast<std::size_t>(rank)];
    ro::ui::CraftAtlasRow row = RowOf(arrow.source_id);
    for (const CraftYield& yield : arrow.yields) {
      if (yield.item_id == item_id) {
        row.quantity = yield.quantity;
        break;
      }
    }
    sheet_arrow_sources_.push_back(row);
  }
  sheet.arrow_sources = sheet_arrow_sources_;
}

}  // namespace moonclient

// LA FABRICATION : les recettes des produits proposés — lues dans le livre du
// serveur —, ce que le sac permet d'en faire, et les noms et icônes.

#include <algorithm>

#include "ro/sim/inventory.h"
#include "ro/sim/world.h"

#include "items/make_item_flow.h"
#include "items/make_item_odds.h"

namespace moonclient {
namespace {

[[nodiscard]] int Stock(const ro::sim::World& world, std::uint32_t item_id) {
  int stock = 0;
  for (const ro::sim::InventoryItem& item : world.inventory().items()) {
    if (item.item_id == item_id) {
      stock += item.amount;
    }
  }
  return stock;
}

}  // namespace

// ── LES RECETTES ────────────────────────────────────────────────────────────

void MakeItemFlow::ResolveMaterials(Entry& entry) const {
  entry.materials.clear();
  if (book_ == nullptr) {
    return;
  }
  // 🔴 UNE LISTE DE FLÈCHES ÉNUMÈRE DES MATÉRIAUX : sa recette est ce que la
  // source REND, et sa clé est la source.
  if (arrow_list_) {
    if (const CraftArrowRecipe* arrow = book_->ArrowFrom(entry.item_id); arrow != nullptr) {
      for (const CraftYield& yield : arrow->yields) {
        entry.materials.push_back(Material{
            .item_id = yield.item_id, .quantity = yield.quantity, .kept = false, .output = true});
      }
    }
    return;
  }
  if (const CraftRecipe* recipe = book_->RecipeOf(entry.item_id); recipe != nullptr) {
    for (const CraftIngredient& ingredient : recipe->ingredients) {
      entry.materials.push_back(Material{.item_id = ingredient.item_id,
                                         .quantity = ingredient.kept ? 1 : ingredient.quantity,
                                         .kept = ingredient.kept,
                                         .output = false});
    }
  }
}

void MakeItemFlow::RecountOwned(const ro::sim::World& world) {
  for (Entry& entry : entries_) {
    entry.owned = Stock(world, entry.item_id);
    entry.craftable = Craftable(entry, world);
  }
}

int MakeItemFlow::Craftable(const Entry& entry, const ro::sim::World& world) const {
  if (entry.materials.empty()) {
    return -1;
  }
  int best = -1;
  for (const Material& material : entry.materials) {
    if (material.quantity <= 0 || material.output) {
      continue;
    }
    if (material.kept) {
      if (Stock(world, material.item_id) <= 0) {
        return -1;
      }
      continue;
    }
    const int possible = Stock(world, material.item_id) / material.quantity;
    best = best < 0 ? possible : std::min(best, possible);
  }
  // 🔴 ZÉRO EST NOTRE ERREUR, pas la vérité : le serveur n'a proposé le produit
  // que parce qu'on peut en faire au moins un. On se tait.
  return best <= 0 ? -1 : best;
}

const MakeItemFlow::Entry* MakeItemFlow::EntryOf(std::uint32_t item_id) const {
  const auto found =
      std::ranges::find_if(entries_, [&](const Entry& entry) { return entry.item_id == item_id; });
  return found == entries_.end() ? nullptr : &*found;
}

std::string MakeItemFlow::ItemName(std::uint32_t item_id) const {
  const ro::vfs::ItemEntry* entry = items_ != nullptr ? items_->Find(item_id) : nullptr;
  return entry != nullptr ? entry->name : std::string();
}

ro::ui::SkinImage MakeItemFlow::IconOf(std::uint32_t item_id) const {
  if (icons_ == nullptr) {
    return {};
  }
  const ro::render::IconTexture& icon = icons_->Get(item_id);
  return ro::ui::SkinImage{
      .id = icon.usable() ? static_cast<ImTextureID>(icon.texture.name()) : ImTextureID{0},
      .width = static_cast<float>(icon.width_px),
      .height = static_cast<float>(icon.height_px)};
}

// La forge seule, et pas les métaux : un Star Crumb ne se forge pas avec des
// Star Crumb. Une liste d'objet peut forger sans skill connu.
bool MakeItemFlow::AcceptsForgeSlots(std::uint32_t product) const {
  if (product >= kFirstElementStone && product <= kStarCrumb) {
    return false;
  }
  if (!from_item_) {
    return skill_id_ >= kForgeFirstSkill && skill_id_ <= kForgeLastSkill;
  }
  return true;
}

}  // namespace moonclient

// La réforme vue par la fenêtre. Voir l'en-tête.

#include "game/item_reform_view.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"

namespace moonclient {
namespace {

using ro::core::Printed;
using ro::sim::InventoryItem;
using ro::sim::ItemReformRecipe;
using ro::sim::ItemReformRefusal;

// `texts_` ne doit pas se déplacer pendant que les vues pointent dedans : une place
// par ligne du sac, par matériau et par phrase de la vue, largement comptée.
constexpr std::size_t kTextsPerRow = 2;
constexpr std::size_t kSceneTexts = 10;

}  // namespace

std::string ItemReformRefusalText(ItemReformRefusal refusal, const ItemReformRecipe* recipe,
                                  std::string_view material_name) {
  switch (refusal) {
    case ItemReformRefusal::kNone:
      return {};
    case ItemReformRefusal::kNotOpen:
      return ro::core::Tr("La fenêtre est fermée.");
    case ItemReformRefusal::kWaiting:
      return ro::core::Tr("En attente de la réponse du serveur.");
    case ItemReformRefusal::kNothingChosen:
      return ro::core::Tr("Choisissez la pièce à réformer.");
    case ItemReformRefusal::kPieceGone:
      return ro::core::Tr("La pièce choisie n'est plus dans l'inventory.");
    case ItemReformRefusal::kNotReformable:
      return ro::core::Tr("Cette pièce ne se réforme pas ici.");
    case ItemReformRefusal::kWorn:
      return ro::core::Tr("Cette pièce est portée : ôtez-la d'abord.");
    case ItemReformRefusal::kUnidentified:
      return ro::core::Tr("Cette pièce n'est pas identifiée.");
    case ItemReformRefusal::kRefineTooLow:
      return Printed(ro::core::Tr("Raffinement trop bas : +%d au moins."),
                     recipe != nullptr ? static_cast<int>(recipe->least_refine) : 0);
    case ItemReformRefusal::kRefineTooHigh:
      return Printed(ro::core::Tr("Raffinement trop haut : +%d au plus."),
                     recipe != nullptr ? static_cast<int>(recipe->most_refine) : 0);
    case ItemReformRefusal::kTooFewOptions:
      return Printed(ro::core::Tr("Il faut au moins %d option(s) sur la pièce."),
                     recipe != nullptr ? static_cast<int>(recipe->least_options) : 0);
    case ItemReformRefusal::kCardsForbidden:
      return ro::core::Tr("Une pièce sertie n'est pas acceptée.");
    case ItemReformRefusal::kMissingMaterial:
      return Printed(ro::core::Tr("Matériau insuffisant : %s."),
                     std::string(material_name).c_str());
  }
  return {};
}

std::string ItemReformResultText(std::uint8_t code) {
  return code == ro::sim::kItemReformSucceeded
             ? std::string()
             : std::string(ro::core::Tr("Le serveur a refusé la réforme."));
}

ro::ui::TradeItemView ItemReformViewComposer::HeldOf(const ItemReformScene& scene,
                                                     const InventoryItem& item) {
  ro::ui::TradeItemView view;
  view.item_id = item.item_id;
  view.icon = scene.looks.icon_of(item.item_id);
  view.damaged = item.damaged;
  view.name = texts_.emplace_back(scene.looks.name_of_held(item));
  return view;
}

void ItemReformViewComposer::ComposeChoice(const ItemReformScene& scene) {
  view_.chosen = {};
  view_.result = {};
  const std::optional<std::uint16_t> slot = scene.reform.chosen_slot();
  const InventoryItem* item = slot.has_value() ? scene.inventory.Find(*slot) : nullptr;
  const ItemReformRecipe* recipe =
      item != nullptr ? scene.trigger.FindRecipe(item->item_id) : nullptr;
  if (recipe == nullptr) {
    return;
  }
  view_.chosen = HeldOf(scene, *item);
  // LE RÉSULTAT GARDE CARTES ET OPTIONS : seuls l'objet et le raffinement changent.
  InventoryItem reformed = *item;
  reformed.item_id = recipe->result_item_id;
  reformed.refine_level = ro::sim::ReformedRefine(scene.trigger, *recipe, item->refine_level);
  view_.result = HeldOf(scene, reformed);
  for (const ro::sim::ItemReformMaterial& material : recipe->materials) {
    ro::ui::ItemReformMaterialView row;
    row.item.item_id = material.item_id;
    row.item.icon = scene.looks.icon_of(material.item_id);
    row.item.name = texts_.emplace_back(scene.looks.name_of_id(material.item_id));
    row.owned = static_cast<int>(ro::sim::CountedMaterial(scene.inventory, material.item_id));
    row.needed = material.amount;
    materials_.push_back(row);
  }
}

void ItemReformViewComposer::ComposeTexts(const ItemReformScene& scene) {
  view_.title = texts_.emplace_back(scene.looks.name_of_id(scene.reform.trigger_item_id()));
  view_.instruction = texts_.emplace_back(ro::core::Tr(
      "Choisissez une pièce : elle devient une autre, contre les matériaux demandés."));
  std::string confirmation =
      Printed(ro::core::Tr("Réformer %s en %s ? La pièce et les matériaux seront consommés."),
              std::string(view_.chosen.name).c_str(), std::string(view_.result.name).c_str());
  view_.confirmation = texts_.emplace_back(std::move(confirmation));
  view_.notice = texts_.emplace_back(scene.notice);

  blockers_.clear();
  const ro::sim::ItemReformCheck check =
      ro::sim::CheckItemReformRequest(scene.reform, scene.trigger, scene.inventory);
  if (!check.passed() && check.refusal != ItemReformRefusal::kWaiting) {
    const std::optional<std::uint16_t> slot = scene.reform.chosen_slot();
    const InventoryItem* item = slot.has_value() ? scene.inventory.Find(*slot) : nullptr;
    const ItemReformRecipe* recipe =
        item != nullptr ? scene.trigger.FindRecipe(item->item_id) : nullptr;
    const std::string material =
        check.material_id != 0 ? scene.looks.name_of_id(check.material_id) : std::string();
    blockers_.push_back(
        texts_.emplace_back(ItemReformRefusalText(check.refusal, recipe, material)));
  }
  view_.blockers = blockers_;
}

const ro::ui::ItemReformView& ItemReformViewComposer::Compose(const ItemReformScene& scene) {
  const std::span<const InventoryItem> held = scene.inventory.items();
  std::size_t most_materials = 0;
  for (const ItemReformRecipe& recipe : scene.trigger.recipes) {
    most_materials = std::max(most_materials, recipe.materials.size());
  }
  texts_.clear();
  texts_.reserve(kTextsPerRow * (held.size() + most_materials) + kSceneTexts);
  candidates_.clear();
  materials_.clear();

  for (const InventoryItem& item : held) {
    const ItemReformRecipe* recipe = scene.trigger.FindRecipe(item.item_id);
    if (recipe == nullptr) {
      continue;
    }
    ro::ui::ItemReformPieceView row;
    row.client_slot = item.client_slot;
    row.item = HeldOf(scene, item);
    row.refusal = texts_.emplace_back(
        ItemReformRefusalText(ro::sim::JudgeItemReformPiece(*recipe, item), recipe));
    row.chosen = scene.reform.chosen_slot() == item.client_slot;
    candidates_.push_back(row);
  }
  ComposeChoice(scene);
  ComposeTexts(scene);
  view_.candidates = candidates_;
  view_.materials = materials_;
  view_.choice_seq = scene.reform.choice_seq();
  view_.waiting = scene.reform.waiting();
  return view_;
}

}  // namespace moonclient

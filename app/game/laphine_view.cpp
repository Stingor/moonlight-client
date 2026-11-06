// Le Laphine vu par la fenêtre. Voir l'en-tête.

#include "game/laphine_view.h"

#include <algorithm>
#include <cstddef>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"

namespace moonclient {
namespace {

using ro::core::Printed;
using ro::sim::InventoryItem;
using ro::sim::LaphineKind;
using ro::sim::LaphineRefusal;

// `texts_` ne doit pas se déplacer pendant que les vues pointent dedans : une place
// par ligne du sac, par objet accepté et par phrase de la vue, largement comptée.
constexpr std::size_t kTextsPerRow = 3;
constexpr std::size_t kSceneTexts = 8;

[[nodiscard]] std::string ShownName(const LaphineScene& scene, const InventoryItem& item) {
  std::string name = scene.looks.name_of_held(item);
  const auto options = std::ranges::count_if(
      item.options, [](const ro::sim::ItemOption& option) { return option.filled(); });
  if (item.identified && options > 0) {
    name = Printed(ro::core::Tr("%s [%d option(s)]"), name.c_str(), static_cast<int>(options));
  }
  return name;
}

[[nodiscard]] std::string WarningText(const ro::sim::LaphineWarnings& warnings) {
  std::string text;
  if (warnings.refined_above_need) {
    text = ro::core::Tr("Le raffinement est plus haut que nécessaire.");
  }
  if (warnings.has_cards) {
    if (!text.empty()) {
      text.push_back('\n');
    }
    text.append(ro::core::Tr("Une carte est sertie ou la pièce est enchantée."));
  }
  return text;
}

}  // namespace

std::string LaphineRefusalText(LaphineRefusal refusal, const ro::sim::LaphineRecipe& recipe,
                               std::uint16_t needed_amount) {
  switch (refusal) {
    case LaphineRefusal::kNone:
      return {};
    case LaphineRefusal::kNotOpen:
      return ro::core::Tr("La fenêtre est fermée.");
    case LaphineRefusal::kWaiting:
      return ro::core::Tr("En attente de la réponse du serveur.");
    case LaphineRefusal::kWrongPileCount:
      return Printed(ro::core::Tr("Il faut déposer %d item(s)."),
                     static_cast<int>(recipe.needed_piles));
    case LaphineRefusal::kPileGone:
      return ro::core::Tr("Un item déposé n'est plus dans l'inventory.");
    case LaphineRefusal::kNotAMaterial:
      return ro::core::Tr("Cet item n'est pas accepté.");
    case LaphineRefusal::kTooFewInPile:
      return Printed(ro::core::Tr("Il en faut %d."), static_cast<int>(needed_amount));
    case LaphineRefusal::kWorn:
      return ro::core::Tr("Cette pièce est portée : ôtez-la d'abord.");
    case LaphineRefusal::kUnidentified:
      return ro::core::Tr("Cette pièce n'est pas identifiée.");
    case LaphineRefusal::kRefineTooLow:
      return Printed(ro::core::Tr("Raffinement trop bas : +%d au moins."),
                     static_cast<int>(recipe.least_refine));
    case LaphineRefusal::kRefineTooHigh:
      return Printed(ro::core::Tr("Raffinement trop haut : +%d au plus."),
                     static_cast<int>(recipe.most_refine));
    case LaphineRefusal::kTooFewOptions:
      return Printed(ro::core::Tr("Il faut au moins %d option(s) sur la pièce."),
                     static_cast<int>(recipe.least_options));
    case LaphineRefusal::kCardsForbidden:
      return ro::core::Tr("Une pièce sertie n'est pas acceptée.");
  }
  return {};
}

std::string LaphineResultText(LaphineKind kind, std::uint16_t code) {
  if (kind == LaphineKind::kUpgrade) {
    return code == ro::sim::kLaphineUpgradeSucceeded
               ? std::string()
               : std::string(ro::core::Tr("Le serveur a refusé l'amélioration."));
  }
  switch (code) {
    case ro::sim::kLaphineSynthesisSucceeded:
      return {};
    case ro::sim::kLaphineSynthesisWrongAmount:
    case ro::sim::kLaphineSynthesisWrongItem:
      return ro::core::Tr("Items insuffisants pour la synthèse.");
    default:
      return Printed(ro::core::Tr("Le serveur a refusé la synthèse (code %d)."),
                     static_cast<int>(code));
  }
}

ro::ui::LaphineItemView LaphineViewComposer::RowOf(const LaphineScene& scene,
                                                   const InventoryItem& item, bool deposited) {
  const ro::sim::LaphineMaterial* material = scene.recipe.FindMaterial(item.item_id);
  const std::uint16_t amount = material != nullptr ? material->amount : std::uint16_t{1};
  ro::ui::LaphineItemView row;
  row.client_slot = item.client_slot;
  row.item.item_id = item.item_id;
  row.item.icon = scene.looks.icon_of(item.item_id);
  row.item.damaged = item.damaged;
  row.item.name = texts_.emplace_back(ShownName(scene, item));
  if (scene.laphine.kind() == LaphineKind::kSynthesis) {
    row.amount = amount;
  }
  if (!deposited) {
    const LaphineRefusal refusal =
        ro::sim::JudgeLaphineItem(scene.recipe, scene.laphine.kind(), item);
    row.refusal = texts_.emplace_back(LaphineRefusalText(refusal, scene.recipe, amount));
  }
  row.warning = texts_.emplace_back(WarningText(ro::sim::WarnLaphineItem(scene.recipe, item)));
  return row;
}

void LaphineViewComposer::ComposeTexts(const LaphineScene& scene) {
  const bool upgrade = scene.laphine.kind() == LaphineKind::kUpgrade;
  view_.title = texts_.emplace_back(scene.looks.name_of_id(scene.laphine.trigger_item_id()));
  view_.make_label =
      texts_.emplace_back(upgrade ? ro::core::Tr("Améliorer") : ro::core::Tr("Synthétiser"));
  std::string instruction = upgrade ? std::string(ro::core::Tr("Déposez la pièce à améliorer."))
                                    : Printed(ro::core::Tr("Déposez %d item(s) pour la synthèse."),
                                              static_cast<int>(scene.recipe.needed_piles));
  if (scene.recipe.least_refine > 0) {
    instruction.push_back(' ');
    instruction.append(Printed(ro::core::Tr("Raffinement : +%d à +%d."),
                               static_cast<int>(scene.recipe.least_refine),
                               static_cast<int>(scene.recipe.most_refine)));
  }
  view_.instruction = texts_.emplace_back(std::move(instruction));

  std::string confirmation =
      upgrade ? std::string(ro::core::Tr(
                    "Améliorer cette pièce ? Ses options seront retirées au sort, et l'item qui "
                    "l'améliore sera consommé."))
              : std::string(ro::core::Tr("Synthétiser ? Ces items seront consommés :"));
  for (const ro::ui::LaphineItemView& row : deposited_) {
    confirmation.append("\n   ");
    confirmation.append(row.item.name);
    if (!row.warning.empty()) {
      confirmation.append(" (!)");
    }
  }
  view_.confirmation = texts_.emplace_back(std::move(confirmation));
  view_.notice = texts_.emplace_back(scene.notice);

  blockers_.clear();
  const ro::sim::LaphineCheck check =
      ro::sim::CheckLaphineRequest(scene.laphine, scene.recipe, scene.inventory);
  if (!check.passed() && check.refusal != LaphineRefusal::kWaiting) {
    blockers_.push_back(texts_.emplace_back(LaphineRefusalText(check.refusal, scene.recipe, 1)));
  }
  view_.blockers = blockers_;
}

const ro::ui::LaphineView& LaphineViewComposer::Compose(const LaphineScene& scene) {
  const std::span<const InventoryItem> held = scene.inventory.items();
  texts_.clear();
  texts_.reserve(kTextsPerRow * (held.size() + scene.recipe.materials.size()) + kSceneTexts);
  candidates_.clear();
  deposited_.clear();
  accepted_.clear();

  for (const std::uint16_t client_slot : scene.laphine.piles()) {
    if (const InventoryItem* item = scene.inventory.Find(client_slot); item != nullptr) {
      deposited_.push_back(RowOf(scene, *item, true));
    }
  }
  for (const InventoryItem& item : held) {
    if (scene.recipe.FindMaterial(item.item_id) != nullptr &&
        !scene.laphine.Deposited(item.client_slot)) {
      candidates_.push_back(RowOf(scene, item, false));
    }
  }
  for (const ro::sim::LaphineMaterial& material : scene.recipe.materials) {
    ro::ui::LaphineAcceptedView accepted;
    accepted.item.item_id = material.item_id;
    accepted.item.icon = scene.looks.icon_of(material.item_id);
    accepted.item.name = texts_.emplace_back(scene.looks.name_of_id(material.item_id));
    if (scene.laphine.kind() == LaphineKind::kSynthesis) {
      accepted.amount = material.amount;
    }
    accepted_.push_back(accepted);
  }
  ComposeTexts(scene);
  view_.candidates = candidates_;
  view_.deposited = deposited_;
  view_.accepted = accepted_;
  view_.piles_seq = scene.laphine.piles_seq();
  view_.waiting = scene.laphine.waiting();
  return view_;
}

}  // namespace moonclient

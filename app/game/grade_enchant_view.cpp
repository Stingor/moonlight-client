// L'amélioration de grade vue par la fenêtre. Voir l'en-tête.

#include "game/grade_enchant_view.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/sim/item_reform.h"

namespace moonclient {
namespace {

using ro::core::Printed;
using ro::sim::GradeEnchantOffer;
using ro::sim::GradeEnchantRefusal;
using ro::sim::GradeEnchantResult;
using ro::sim::InventoryItem;

// `texts_` ne doit pas se déplacer pendant que les vues pointent dedans : une place
// par ligne du sac, par mise et par phrase de la vue, largement comptée.
constexpr std::size_t kTextsPerRow = 3;
constexpr std::size_t kSceneTexts = 12;

[[nodiscard]] std::string RiskText(const ro::sim::GradeEnchantStake& stake) {
  if (stake.breaks) {
    return ro::core::Tr("l'arme est détruite si l'amélioration échoue");
  }
  if (stake.downgrades) {
    return ro::core::Tr("le raffinement baisse si l'amélioration échoue");
  }
  return ro::core::Tr("sans risque pour l'arme");
}

}  // namespace

std::string GradeEnchantRefusalText(GradeEnchantRefusal refusal, std::string_view item_name) {
  switch (refusal) {
    case GradeEnchantRefusal::kNone:
      return {};
    case GradeEnchantRefusal::kNotOpen:
      return ro::core::Tr("La fenêtre est fermée.");
    case GradeEnchantRefusal::kWaiting:
      return ro::core::Tr("En attente de la réponse du serveur.");
    case GradeEnchantRefusal::kNothingChosen:
      return ro::core::Tr("Choisissez l'arme à améliorer.");
    case GradeEnchantRefusal::kPieceGone:
      return ro::core::Tr("L'arme choisie n'est plus dans l'inventory.");
    case GradeEnchantRefusal::kNotGradable:
      return ro::core::Tr("Seules les armes prennent un grade.");
    case GradeEnchantRefusal::kWorn:
      return ro::core::Tr("Cette arme est portée : ôtez-la d'abord.");
    case GradeEnchantRefusal::kUnidentified:
      return ro::core::Tr("Cette arme n'est pas identifiée.");
    case GradeEnchantRefusal::kTopGrade:
      return ro::core::Tr("Cette arme a déjà le grade le plus haut.");
    case GradeEnchantRefusal::kAwaitingOffer:
      return ro::core::Tr("Le serveur évalue la pièce…");
    case GradeEnchantRefusal::kLevelUnsupported:
      return ro::core::Tr("Ce niveau d'arme ne prend pas de grade.");
    case GradeEnchantRefusal::kRefineTooLow:
      return ro::core::Tr("Raffinement insuffisant pour ce grade.");
    case GradeEnchantRefusal::kNoSuchStake:
      return ro::core::Tr("Choisissez une mise.");
    case GradeEnchantRefusal::kMissingStake:
    case GradeEnchantRefusal::kMissingBlessing:
      return Printed(ro::core::Tr("Matériau insuffisant : %s."), std::string(item_name).c_str());
    case GradeEnchantRefusal::kNotEnoughZeny:
      return ro::core::Tr("Pas assez de zenys.");
  }
  return {};
}

std::string GradeEnchantResultText(GradeEnchantResult result) {
  switch (result) {
    case GradeEnchantResult::kSucceeded:
      return ro::core::Tr("Le grade de l'arme monte !");
    case GradeEnchantResult::kFailed:
      return ro::core::Tr("L'amélioration a échoué.");
    case GradeEnchantResult::kDowngraded:
      return ro::core::Tr("L'amélioration a échoué : le raffinement baisse.");
    case GradeEnchantResult::kBroken:
      return ro::core::Tr("L'amélioration a échoué : l'arme est détruite.");
    case GradeEnchantResult::kProtected:
      return ro::core::Tr("L'amélioration a échoué, mais l'arme est protégée.");
  }
  return ro::core::Tr("L'amélioration a échoué.");
}

std::string GradeLetter(std::uint8_t grade) {
  switch (grade) {
    case 0:
      return ro::core::Tr("aucun");
    case 1:
      return "D";
    case 2:
      return "C";
    case 3:
      return "B";
    case 4:
      return "A";
    default:
      return "?";
  }
}

ro::ui::TradeItemView GradeEnchantViewComposer::HeldOf(const GradeEnchantScene& scene,
                                                       const InventoryItem& item) {
  ro::ui::TradeItemView view;
  view.item_id = item.item_id;
  view.icon = scene.looks.icon_of(item.item_id);
  view.damaged = item.damaged;
  view.name = texts_.emplace_back(scene.looks.name_of_held(item));
  return view;
}

ro::ui::TradeItemView GradeEnchantViewComposer::CatalogOf(const GradeEnchantScene& scene,
                                                          std::uint32_t item_id) {
  ro::ui::TradeItemView view;
  view.item_id = item_id;
  view.icon = scene.looks.icon_of(item_id);
  view.name = texts_.emplace_back(scene.looks.name_of_id(item_id));
  return view;
}

void GradeEnchantViewComposer::ComposeChoice(const GradeEnchantScene& scene) {
  view_.chosen = {};
  view_.step = {};
  view_.offer_ready = false;
  view_.offer_refusal = {};
  view_.chance_percent = 0;
  view_.blessing = {};
  view_.blessing_owned = 0;
  view_.blessing_per_step = 0;
  view_.blessing_most_steps = 0;
  view_.blessing_bonus_percent = 0;
  const std::optional<std::uint16_t> slot = scene.grade.chosen_slot();
  const InventoryItem* item = slot.has_value() ? scene.inventory.Find(*slot) : nullptr;
  if (item == nullptr) {
    return;
  }
  view_.chosen = HeldOf(scene, *item);
  view_.step = texts_.emplace_back(
      Printed(ro::core::Tr("Grade : %s, puis %s"), GradeLetter(item->grade).c_str(),
              GradeLetter(static_cast<std::uint8_t>(item->grade + 1)).c_str()));
  // UNE PIÈCE QUE LE SERVEUR N'ADMET PLUS ne recevra pas de proposition.
  if (const GradeEnchantRefusal refusal = ro::sim::JudgeGradePiece(*item);
      refusal != GradeEnchantRefusal::kNone) {
    view_.step = {};
    view_.offer_ready = true;
    view_.offer_refusal = texts_.emplace_back(GradeEnchantRefusalText(refusal));
    return;
  }
  const std::optional<GradeEnchantOffer>& offer = scene.grade.offer();
  if (!offer.has_value()) {
    return;
  }
  view_.offer_ready = true;
  if (!offer->supported) {
    view_.offer_refusal =
        texts_.emplace_back(GradeEnchantRefusalText(GradeEnchantRefusal::kLevelUnsupported));
    return;
  }
  if (offer->chance_percent == 0) {
    view_.offer_refusal =
        texts_.emplace_back(GradeEnchantRefusalText(GradeEnchantRefusal::kRefineTooLow));
    return;
  }
  view_.chance_percent = offer->chance_percent;
  const ro::sim::GradeEnchantBlessing& blessing = offer->blessing;
  if (blessing.item_id != 0 && blessing.per_step > 0) {
    view_.blessing = CatalogOf(scene, blessing.item_id);
    view_.blessing_owned =
        static_cast<int>(ro::sim::CountedMaterial(scene.inventory, blessing.item_id));
    view_.blessing_per_step = static_cast<int>(blessing.per_step);
    view_.blessing_most_steps = static_cast<int>(blessing.most_steps);
    view_.blessing_bonus_percent = static_cast<int>(blessing.bonus_percent);
  }
  for (const ro::sim::GradeEnchantStake& stake : offer->stakes) {
    ro::ui::GradeEnchantStakeView row;
    row.item = CatalogOf(scene, stake.item_id);
    row.owned = static_cast<int>(ro::sim::CountedMaterial(scene.inventory, stake.item_id));
    row.needed = static_cast<int>(stake.amount);
    row.zeny = stake.zeny;
    row.risk = texts_.emplace_back(RiskText(stake));
    row.risky = stake.breaks || stake.downgrades;
    stakes_.push_back(row);
  }
}

void GradeEnchantViewComposer::ComposeTexts(const GradeEnchantScene& scene) {
  view_.instruction = texts_.emplace_back(ro::core::Tr(
      "Choisissez une arme : son grade monte d'un cran, contre un joyau et des zenys."));
  view_.notice = texts_.emplace_back(scene.notice);
  view_.notice_good = scene.notice_good;

  std::string confirmation;
  const std::optional<GradeEnchantOffer>& offer = scene.grade.offer();
  if (offer.has_value() && scene.attempt.stake < offer->stakes.size()) {
    const ro::sim::GradeEnchantStake& stake = offer->stakes[scene.attempt.stake];
    confirmation = Printed(
        ro::core::Tr("Améliorer %s ? Chance de réussite : %d %%."),
        std::string(view_.chosen.name).c_str(),
        static_cast<int>(ro::sim::GradeEnchantChance(*offer, scene.attempt.blessing_steps)));
    if (stake.breaks) {
      confirmation += "\n";
      confirmation += ro::core::Tr("En cas d'échec, l'arme est détruite.");
    }
    confirmation += "\n";
    confirmation += ro::core::Tr("Une réussite ramène l'arme à +0.");
  }
  view_.confirmation = texts_.emplace_back(std::move(confirmation));

  blockers_.clear();
  const ro::sim::GradeEnchantCheck check =
      ro::sim::CheckGradeEnchantRequest(scene.grade, scene.attempt, scene.inventory, scene.zeny);
  if (!check.passed() && check.refusal != GradeEnchantRefusal::kWaiting) {
    const std::string item =
        check.item_id != 0 ? scene.looks.name_of_id(check.item_id) : std::string();
    blockers_.push_back(texts_.emplace_back(GradeEnchantRefusalText(check.refusal, item)));
  }
  view_.blockers = blockers_;
}

const ro::ui::GradeEnchantView& GradeEnchantViewComposer::Compose(const GradeEnchantScene& scene) {
  const std::span<const InventoryItem> held = scene.inventory.items();
  const std::size_t stakes =
      scene.grade.offer().has_value() ? scene.grade.offer()->stakes.size() : 0;
  texts_.clear();
  texts_.reserve(kTextsPerRow * (held.size() + stakes) + kSceneTexts);
  candidates_.clear();
  stakes_.clear();

  for (const InventoryItem& item : held) {
    const GradeEnchantRefusal refusal = ro::sim::JudgeGradePiece(item);
    if (refusal == GradeEnchantRefusal::kNotGradable) {
      continue;
    }
    ro::ui::GradeEnchantPieceView row;
    row.client_slot = item.client_slot;
    row.item = HeldOf(scene, item);
    row.refusal = texts_.emplace_back(GradeEnchantRefusalText(refusal));
    row.chosen = scene.grade.chosen_slot() == item.client_slot;
    candidates_.push_back(row);
  }
  ComposeChoice(scene);
  ComposeTexts(scene);
  view_.candidates = candidates_;
  view_.stakes = stakes_;
  view_.choice_seq = scene.grade.choice_seq();
  view_.waiting = scene.grade.waiting();
  return view_;
}

}  // namespace moonclient

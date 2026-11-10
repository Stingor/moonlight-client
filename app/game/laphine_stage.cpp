// Le Laphine, dans la boucle. Voir l'en-tête.

#include "game/laphine_stage.h"

#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/laphine.h"
#include "ro/sim/world.h"

#include "items/laphine_book.h"

namespace moonclient {

void LaphineStage::StartSession() {
  laphine_.CloseSilently();
  result_read_ = laphine_.result_seq();
  notice_.clear();
  hovered_ = {};
  described_.reset();
  view_ = nullptr;
}

std::optional<TradeItemFocus> LaphineStage::TakeDescribed() noexcept {
  return std::exchange(described_, std::nullopt);
}

bool LaphineStage::YieldItemUse(std::uint32_t item_id, const LaphineBook* book,
                                ro::net::Connection& link) {
  if (!laphine_.open() || book == nullptr || !book->Triggers(item_id)) {
    return true;
  }
  if (!laphine_.GiveWayToTrigger()) {
    return false;
  }
  ro::net::SayLaphineFarewells(link, laphine_);
  return true;
}

const ro::sim::LaphineRecipe* LaphineStage::RecipeOf(const TradeContext& context) const {
  return context.laphine_book == nullptr
             ? nullptr
             : context.laphine_book->Find(laphine_.kind(), laphine_.trigger_item_id());
}

void LaphineStage::Make(const ro::ui::LaphineWindowAction& answer, const TradeContext& context) {
  // 🔴 CE QUI PART EST CE QUE LE JOUEUR A CONFIRMÉ : si le dépôt a changé depuis,
  // rien ne part.
  if (answer.piles_seq != laphine_.piles_seq()) {
    return;
  }
  const ro::sim::LaphineRecipe* recipe = RecipeOf(context);
  if (recipe == nullptr) {
    return;
  }
  notice_.clear();
  // ⚠ REJUGÉ ICI : la fenêtre a montré un verdict d'une image de retard.
  if (auto sent =
          ro::net::RequestLaphineMake(context.link, laphine_, *recipe, context.world.inventory());
      !sent) {
    ro::core::LogError("laphine : {}", ro::core::Describe(sent.error()));
  }
}

void LaphineStage::Follow(ro::ui::LaphineWindowState& state,
                          const ro::ui::LaphineWindowAction& answer, const TradeContext& context) {
  const ro::sim::Inventory& bag = context.world.inventory();
  hovered_ = {};
  if (answer.hovered_slot != 0) {
    if (const ro::sim::InventoryItem* item = bag.Find(answer.hovered_slot); item != nullptr) {
      hovered_ = TradeItemFocus{.item_id = item->item_id, .bag_slot = answer.hovered_slot};
    }
  } else if (answer.hovered_id != 0) {
    hovered_ = TradeItemFocus{.item_id = answer.hovered_id};
  }
  if (answer.describe_slot != 0) {
    if (const ro::sim::InventoryItem* item = bag.Find(answer.describe_slot); item != nullptr) {
      described_ = TradeItemFocus{.item_id = item->item_id,
                                  .bag_slot = answer.describe_slot,
                                  .gesture = answer.item_gesture};
    }
  } else if (answer.describe_id != 0) {
    described_ = TradeItemFocus{.item_id = answer.describe_id, .gesture = answer.item_gesture};
  }
  if (!laphine_.open()) {
    return;
  }
  // 🔴 UNE FENÊTRE BAISSÉE EST UNE FERMETURE, quel que soit le geste. ⚠ MAIS
  // SEULEMENT UNE FENÊTRE QUI MONTRAIT DÉJÀ CETTE OUVERTURE : à l'image où le
  // Laphine s'ouvre, elle est encore baissée.
  const bool shown = state.opening_seen == laphine_.opening_seq();
  if (answer.close || (!state.open && shown)) {
    laphine_.Close();
    return;
  }
  if (!shown) {
    return;
  }
  if (answer.deposit_slot != 0) {
    if (const ro::sim::LaphineRecipe* recipe = RecipeOf(context); recipe != nullptr) {
      const ro::sim::InventoryItem* item = bag.Find(answer.deposit_slot);
      // ⚠ LE DÉPÔT PLEIN : déposer encore remplace, pour l'amélioration qui n'en
      // tient qu'une ; la synthèse s'arrête à ce qu'elle demande.
      if (item != nullptr && ro::sim::JudgeLaphineItem(*recipe, laphine_.kind(), *item) ==
                                 ro::sim::LaphineRefusal::kNone) {
        if (laphine_.piles().size() >= recipe->needed_piles &&
            laphine_.kind() == ro::sim::LaphineKind::kUpgrade) {
          static_cast<void>(laphine_.Withdraw(laphine_.piles().front()));
        }
        if (laphine_.piles().size() < recipe->needed_piles) {
          notice_.clear();
          static_cast<void>(laphine_.Deposit(answer.deposit_slot));
        }
      }
    }
  }
  if (answer.withdraw_slot != 0) {
    static_cast<void>(laphine_.Withdraw(answer.withdraw_slot));
  }
  if (answer.make) {
    Make(answer, context);
  }
}

void LaphineStage::ReadResult(const TradeContext& context) {
  if (laphine_.result_seq() != result_read_) {
    result_read_ = laphine_.result_seq();
    notice_ = LaphineResultText(laphine_.kind(), laphine_.result());
  }
  if (const std::optional<std::uint16_t> upgraded = laphine_.TakeUpgraded(); upgraded.has_value()) {
    if (const ro::sim::InventoryItem* item = context.world.inventory().Find(*upgraded);
        item != nullptr) {
      described_ = TradeItemFocus{.item_id = item->item_id, .bag_slot = *upgraded};
    }
  }
}

void LaphineStage::Advance(ro::ui::LaphineWindowState& state,
                           const ro::ui::LaphineWindowAction& answer, const TradeContext& context) {
  Follow(state, answer, context);
  ReadResult(context);

  // 🔴 UNE RECETTE INCONNUE NE LAISSE PAS LE PERSONNAGE FIGÉ : la fenêtre se referme,
  // et la fermeture part aussitôt.
  const ro::sim::LaphineRecipe* recipe = laphine_.open() ? RecipeOf(context) : nullptr;
  if (laphine_.open() && recipe == nullptr) {
    ro::core::LogWarning("laphine : aucune recette pour l'objet {}", laphine_.trigger_item_id());
    Say(context, ro::core::Tr("Le client ne connaît pas encore cette recette du Laphine."),
        kTradeFailedColor);
    laphine_.Close();
  }
  ro::net::SayLaphineFarewells(context.link, laphine_);

  view_ = nullptr;
  if (!laphine_.open()) {
    state.open = false;
    state.asking = false;
    return;
  }
  if (laphine_.opening_seq() != state.opening_seen) {
    ro::ui::OpenLaphineOn(state, laphine_.opening_seq());
    notice_.clear();
  }
  const LaphineLooks looks{
      .name_of_held =
          [&context](const ro::sim::InventoryItem& item) {
            return context.items.NameOf(item, context.creators);
          },
      .name_of_id = [&context](std::uint32_t item_id) { return context.items.NameOf(item_id); },
      .icon_of = [&context](std::uint32_t item_id) { return context.items.IconOf(item_id); }};
  view_ = &composer_.Compose(LaphineScene{.laphine = laphine_,
                                          .recipe = *recipe,
                                          .inventory = context.world.inventory(),
                                          .looks = looks,
                                          .notice = notice_});
}

}  // namespace moonclient

// La réforme, dans la boucle. Voir l'en-tête.

#include "game/item_reform_stage.h"

#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/item_reform.h"
#include "ro/sim/world.h"

#include "items/item_reform_book.h"

namespace moonclient {

void ItemReformStage::StartSession() {
  reform_.CloseSilently();
  result_read_ = reform_.result_seq();
  notice_.clear();
  hovered_ = {};
  described_.reset();
  view_ = nullptr;
}

std::optional<TradeItemFocus> ItemReformStage::TakeDescribed() noexcept {
  return std::exchange(described_, std::nullopt);
}

const ro::sim::ItemReformTrigger* ItemReformStage::TriggerOf(const TradeContext& context) const {
  return context.item_reform_book == nullptr
             ? nullptr
             : context.item_reform_book->Find(reform_.trigger_item_id());
}

void ItemReformStage::Reform(const ro::ui::ItemReformWindowAction& answer,
                             const TradeContext& context) {
  // 🔴 CE QUI PART EST CE QUE LE JOUEUR A CONFIRMÉ : si le choix a changé depuis,
  // rien ne part.
  if (answer.choice_seq != reform_.choice_seq()) {
    return;
  }
  const ro::sim::ItemReformTrigger* trigger = TriggerOf(context);
  if (trigger == nullptr) {
    return;
  }
  notice_.clear();
  // ⚠ REJUGÉ ICI : la fenêtre a montré un verdict d'une image de retard.
  if (auto sent =
          ro::net::RequestItemReform(context.link, reform_, *trigger, context.world.inventory());
      !sent) {
    ro::core::LogError("item_reform : {}", ro::core::Describe(sent.error()));
  }
}

void ItemReformStage::Follow(ro::ui::ItemReformWindowState& state,
                             const ro::ui::ItemReformWindowAction& answer,
                             const TradeContext& context) {
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
  if (!reform_.open()) {
    return;
  }
  // 🔴 UNE FENÊTRE BAISSÉE EST UNE FERMETURE, quel que soit le geste. ⚠ MAIS
  // SEULEMENT UNE FENÊTRE QUI MONTRAIT DÉJÀ CETTE OUVERTURE : à l'image où la
  // réforme s'ouvre, elle est encore baissée.
  const bool shown = state.opening_seen == reform_.opening_seq();
  if (answer.close || (!state.open && shown)) {
    reform_.Close();
    return;
  }
  if (!shown) {
    return;
  }
  if (answer.choose_slot != 0) {
    notice_.clear();
    static_cast<void>(reform_.Choose(answer.choose_slot));
  }
  if (answer.reform) {
    Reform(answer, context);
  }
}

void ItemReformStage::ReadResult(const TradeContext& context) {
  if (reform_.result_seq() != result_read_) {
    result_read_ = reform_.result_seq();
    notice_ = ItemReformResultText(reform_.result());
  }
  if (const std::optional<std::uint16_t> reformed = reform_.TakeReformed(); reformed.has_value()) {
    if (const ro::sim::InventoryItem* item = context.world.inventory().Find(*reformed);
        item != nullptr) {
      described_ = TradeItemFocus{.item_id = item->item_id, .bag_slot = *reformed};
    }
  }
}

void ItemReformStage::Advance(ro::ui::ItemReformWindowState& state,
                              const ro::ui::ItemReformWindowAction& answer,
                              const TradeContext& context) {
  Follow(state, answer, context);
  ReadResult(context);

  // 🔴 UN DÉCLENCHEUR INCONNU NE LAISSE PAS LE PERSONNAGE FIGÉ : la fenêtre se
  // referme, et la fermeture part aussitôt.
  const ro::sim::ItemReformTrigger* trigger = reform_.open() ? TriggerOf(context) : nullptr;
  if (reform_.open() && trigger == nullptr) {
    ro::core::LogWarning("item_reform : aucune recette pour l'objet {}", reform_.trigger_item_id());
    Say(context, ro::core::Tr("Le client ne connaît pas encore cette réforme."), kTradeFailedColor);
    reform_.Close();
  }
  ro::net::SayItemReformFarewell(context.link, reform_);

  view_ = nullptr;
  if (!reform_.open()) {
    state.open = false;
    state.asking = false;
    return;
  }
  if (reform_.opening_seq() != state.opening_seen) {
    ro::ui::OpenItemReformOn(state, reform_.opening_seq());
    notice_.clear();
  }
  const ItemReformLooks looks{
      .name_of_held =
          [&context](const ro::sim::InventoryItem& item) {
            return context.items.NameOf(item, context.creators);
          },
      .name_of_id = [&context](std::uint32_t item_id) { return context.items.NameOf(item_id); },
      .icon_of = [&context](std::uint32_t item_id) { return context.items.IconOf(item_id); }};
  view_ = &composer_.Compose(ItemReformScene{.reform = reform_,
                                             .trigger = *trigger,
                                             .inventory = context.world.inventory(),
                                             .looks = looks,
                                             .notice = notice_});
}

}  // namespace moonclient

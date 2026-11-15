// L'enchantement, dans la boucle. Voir l'en-tête.

#include "game/item_enchant_stage.h"

#include <limits>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/item_enchant.h"
#include "ro/sim/world.h"

#include "items/item_enchant_book.h"

namespace moonclient {
namespace {

[[nodiscard]] std::uint8_t CardSlotsOf(const TradeContext& context, std::uint32_t item_id) {
  const int slots = context.items.CardSlotsOf(item_id);
  return static_cast<std::uint8_t>(slots < 0 ? 0 : slots);
}

}  // namespace

void ItemEnchantStage::StartSession() {
  enchant_.CloseSilently();
  result_read_ = enchant_.result_seq();
  notice_.clear();
  notice_good_ = false;
  concluded_ = false;
  shown_group_ = 0;
  hovered_ = {};
  described_.reset();
  view_ = nullptr;
}

std::optional<TradeItemFocus> ItemEnchantStage::TakeDescribed() noexcept {
  return std::exchange(described_, std::nullopt);
}

const ro::sim::ItemEnchantGroup* ItemEnchantStage::GroupOf(const TradeContext& context) const {
  const std::uint64_t group_id = enchant_.open() ? enchant_.group_id() : shown_group_;
  return context.item_enchant_book == nullptr ? nullptr : context.item_enchant_book->Find(group_id);
}

void ItemEnchantStage::Enchant(const ro::ui::ItemEnchantWindowState& state,
                               const ro::ui::ItemEnchantWindowAction& answer,
                               const TradeContext& context) {
  // 🔴 CE QUI PART EST CE QUE LE JOUEUR A CONFIRMÉ : si le choix a changé depuis,
  // rien ne part.
  if (answer.choice_seq != enchant_.choice_seq() || !enchant_.chosen_slot().has_value()) {
    return;
  }
  const ro::sim::ItemEnchantGroup* group = GroupOf(context);
  const ro::sim::InventoryItem* item = context.world.inventory().Find(*enchant_.chosen_slot());
  if (group == nullptr || item == nullptr) {
    return;
  }
  const std::int64_t zeny = context.world.player_status().zeny;
  const std::int64_t purse =
      zeny == ro::sim::kUnknownStat ? std::numeric_limits<std::int64_t>::max() : zeny;
  notice_.clear();
  // ⚠ REJUGÉ ICI : la fenêtre a montré un verdict d'une image de retard.
  if (auto sent = ro::net::RequestItemEnchant(
          context.link, enchant_, *group, ItemEnchantRequestOf(state), context.world.inventory(),
          CardSlotsOf(context, item->item_id), purse);
      !sent) {
    ro::core::LogError("item_enchant : {}", ro::core::Describe(sent.error()));
  }
}

void ItemEnchantStage::Follow(ro::ui::ItemEnchantWindowState& state,
                              const ro::ui::ItemEnchantWindowAction& answer,
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
  // LA FENÊTRE D'UNE SÉANCE CLOSE se ferme sans rien dire au serveur, qui l'a déjà
  // oubliée.
  if (concluded_ && !enchant_.open()) {
    if (answer.close || !state.open) {
      concluded_ = false;
    }
    return;
  }
  if (!enchant_.open()) {
    return;
  }
  // 🔴 UNE FENÊTRE BAISSÉE EST UNE FERMETURE, quel que soit le geste. ⚠ MAIS
  // SEULEMENT UNE FENÊTRE QUI MONTRAIT DÉJÀ CETTE OUVERTURE : à l'image où la
  // séance s'ouvre, elle est encore baissée.
  const bool shown = state.opening_seen == enchant_.opening_seq();
  if (answer.close || (!state.open && shown)) {
    enchant_.Close();
    return;
  }
  if (!shown) {
    return;
  }
  if (answer.choose_slot != 0) {
    notice_.clear();
    static_cast<void>(enchant_.Choose(answer.choose_slot));
  }
  if (answer.enchant) {
    Enchant(state, answer, context);
  }
}

void ItemEnchantStage::ReadResult(const TradeContext& context) {
  if (enchant_.result_seq() == result_read_) {
    return;
  }
  result_read_ = enchant_.result_seq();
  const ro::sim::ItemEnchant::Result& result = enchant_.result();
  const std::uint32_t named = result.enchant_item_id;
  notice_ = ItemEnchantResultText(result, named != 0 ? context.items.NameOf(named) : std::string());
  notice_good_ = result.succeeded();
  Say(context, notice_, notice_good_ ? kTradeSucceededColor : kTradeFailedColor);
  // LA SÉANCE EST CLOSE, LA FENÊTRE RESTE pour montrer la pièce telle qu'elle est.
  concluded_ = true;
  shown_group_ = enchant_.group_id();
}

void ItemEnchantStage::Advance(ro::ui::ItemEnchantWindowState& state,
                               const ro::ui::ItemEnchantWindowAction& answer,
                               const TradeContext& context) {
  Follow(state, answer, context);
  ReadResult(context);
  // 🔴 UN GROUPE INCONNU NE LAISSE PAS LA SÉANCE OUVERTE : la fenêtre se referme, et
  // la fermeture part aussitôt.
  if (enchant_.open() && GroupOf(context) == nullptr) {
    ro::core::LogWarning("item_enchant : aucun groupe {} dans la table", enchant_.group_id());
    Say(context, ro::core::Tr("Le client ne connaît pas encore cet enchantement."),
        kTradeFailedColor);
    enchant_.Close();
  }
  // LA PIÈCE CHOISIE A QUITTÉ LE SAC — vendue, rangée : le choix tombe.
  if (const std::optional<std::uint16_t> chosen = enchant_.chosen_slot();
      chosen.has_value() && !enchant_.waiting() &&
      context.world.inventory().Find(*chosen) == nullptr) {
    enchant_.Forget();
  }
  ro::net::SayItemEnchantFarewell(context.link, enchant_);

  view_ = nullptr;
  if (enchant_.open() && enchant_.opening_seq() != state.opening_seen) {
    ro::ui::OpenItemEnchantOn(state, enchant_.opening_seq());
    notice_.clear();
    notice_good_ = false;
    concluded_ = false;
  }
  if (!showing()) {
    state.open = false;
    state.asking = false;
    return;
  }
  const ro::sim::ItemEnchantGroup* group = GroupOf(context);
  if (group == nullptr) {
    concluded_ = false;
    state.open = false;
    state.asking = false;
    return;
  }
  const ItemEnchantLooks looks{
      .name_of_held =
          [&context](const ro::sim::InventoryItem& item) {
            return context.items.NameOf(item, context.creators);
          },
      .name_of_id = [&context](std::uint32_t item_id) { return context.items.NameOf(item_id); },
      .icon_of = [&context](std::uint32_t item_id) { return context.items.IconOf(item_id); },
      .card_slots_of = [&context](std::uint32_t item_id) { return CardSlotsOf(context, item_id); }};
  const std::int64_t zeny = context.world.player_status().zeny;
  // UNE SÉANCE CLOSE MONTRE ENCORE LA PIÈCE QU'ELLE A TOUCHÉE.
  const std::optional<std::uint16_t> chosen =
      enchant_.open() ? enchant_.chosen_slot()
                      : std::optional<std::uint16_t>(enchant_.result().client_slot);
  view_ = &composer_.Compose(ItemEnchantScene{.enchant = enchant_,
                                              .group = *group,
                                              .inventory = context.world.inventory(),
                                              .looks = looks,
                                              .state = state,
                                              .chosen_slot = chosen,
                                              .zeny = zeny,
                                              .zeny_known = zeny != ro::sim::kUnknownStat,
                                              .notice = notice_,
                                              .notice_good = notice_good_,
                                              .concluded = concluded_ && !enchant_.open()});
}

}  // namespace moonclient

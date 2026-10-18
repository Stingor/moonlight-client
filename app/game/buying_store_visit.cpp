// La vente à l'échoppe d'un acheteur, dans la boucle. Voir `buying_store_stage.h`.

#include <algorithm>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/net/buying_store.h"
#include "ro/sim/buying_store_checks.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"
#include "ro/vfs/trade_words.h"

#include "game/buying_store_rules.h"
#include "game/buying_store_stage.h"

namespace moonclient {
namespace {

using ro::vfs::TradeWord;

std::vector<ro::sim::BuyingStoreSaleLine> SaleLinesOf(
    std::span<const ro::ui::BuyingStoreSaleCartLine> cart) {
  std::vector<ro::sim::BuyingStoreSaleLine> lines;
  lines.reserve(cart.size());
  for (const ro::ui::BuyingStoreSaleCartLine& line : cart) {
    lines.push_back(ro::sim::BuyingStoreSaleLine{
        .client_slot = line.client_slot,
        .item_id = line.item_id,
        .amount =
            static_cast<std::uint16_t>(std::clamp(line.amount, 0, kBuyingStoreMostPerSaleLine))});
  }
  return lines;
}

std::int64_t KnownZeny(const ro::sim::PlayerStatus& status) {
  return status.zeny == ro::sim::kUnknownStat ? 0 : status.zeny;
}

// L'enseigne d'achat que porte cet acteur, s'il en porte une.
const ro::sim::Signboard* BuyingBoardOf(const ro::sim::Actor* actor) {
  if (actor == nullptr || !actor->signboard.has_value() ||
      actor->signboard->kind != ro::sim::SignboardKind::kBuyingStore) {
    return nullptr;
  }
  return &*actor->signboard;
}

}  // namespace

void BuyingStoreStage::FollowSell(ro::ui::BuyingStoreSellState& state,
                                  const ro::ui::BuyingStoreSellAction& answer,
                                  const TradeContext& context) {
  if (answer.hovered_id != 0) {
    hovered_ = TradeItemFocus{.item_id = answer.hovered_id, .bag_slot = answer.hovered_slot};
  }
  if (answer.describe_id != 0) {
    described_ = TradeItemFocus{.item_id = answer.describe_id,
                                .bag_slot = answer.describe_slot,
                                .gesture = answer.item_gesture};
  }
  if (!store_.visiting()) {
    return;
  }
  const ro::sim::VisitedBuyingStore& visited = store_.visited();
  // ⚠ SEULEMENT UNE FENÊTRE QUI MONTRAIT DÉJÀ CETTE ÉCHOPPE.
  const bool shown =
      state.buyer_seen == visited.buyer_account_id && state.store_seen == visited.store_id;
  if (!shown) {
    return;
  }
  // Le serveur n'a rien réservé : quitter l'échoppe ne se dit pas.
  if (answer.close || !state.open) {
    store_.LeaveVisit();
    refresh_asked_ = false;
    return;
  }
  if (answer.sell.empty()) {
    return;
  }
  // ⚠ REJUGÉ ICI : la confirmation a pu rester ouverte pendant qu'un achat
  // d'un autre vendeur changeait ce que l'acheteur veut encore.
  const std::vector<ro::sim::BuyingStoreSaleLine> lines = SaleLinesOf(answer.sell);
  const std::span<const ro::sim::InventoryItem> bag = context.world.inventory().items();
  const std::vector<ro::sim::BuyingStoreSaleProblem> problems =
      ro::sim::CheckBuyingStoreSale(visited, lines, bag, KnownZeny(context.world.player_status()));
  if (!problems.empty()) {
    Say(context, SaleFaultText(problems.front().fault), kBuyingStoreFailedColor);
    return;
  }
  // LES NOMS SE COMPOSENT MAINTENANT : quand la confirmation arrivera, le
  // serveur aura déjà retiré la pile du sac.
  sold_names_.clear();
  for (const ro::sim::BuyingStoreSaleLine& line : lines) {
    const ro::sim::InventoryItem* item = context.world.inventory().Find(line.client_slot);
    sold_names_.emplace_back(line.client_slot, item != nullptr
                                                   ? context.items.NameOf(*item, context.creators)
                                                   : context.items.NameOf(line.item_id));
  }
  if (auto sent = ro::net::RequestBuyingStoreSale(context.link, store_, lines); !sent) {
    ro::core::LogError("échoppe d'achat : {}", ro::core::Describe(sent.error()));
    return;
  }
  sale_sent_ = true;
}

void BuyingStoreStage::WatchBuyer(const TradeContext& context) {
  if (!store_.visiting()) {
    buyer_had_board_ = false;
    return;
  }
  // 🔴 UNE ÉCHOPPE FERMÉE NE LE DIT PAS AU VISITEUR : son enseigne disparaît,
  // c'est tout. Une vente vers elle échouerait avec le code fourre-tout ; la
  // fenêtre se ferme donc avec l'enseigne.
  const ro::sim::Actor* buyer = context.world.Find(store_.visited().buyer_account_id);
  if (BuyingBoardOf(buyer) != nullptr) {
    buyer_had_board_ = true;
  } else if (buyer_had_board_) {
    buyer_had_board_ = false;
    store_.LeaveVisit();
    refresh_asked_ = false;
  }
}

void BuyingStoreStage::OpenSellWindow(ro::ui::BuyingStoreSellState& state) {
  if (!store_.visiting()) {
    state.open = false;
    return;
  }
  if (store_.visit_seq() == visit_seen_) {
    return;
  }
  visit_seen_ = store_.visit_seq();
  // LA RÉPONSE À UN RAFRAÎCHISSEMENT, fenêtre refermée entre-temps : elle ne
  // rouvre rien.
  if (refresh_asked_ && !state.open) {
    refresh_asked_ = false;
    store_.LeaveVisit();
    return;
  }
  refresh_asked_ = false;
  const ro::sim::VisitedBuyingStore& visited = store_.visited();
  if (ro::ui::OpenBuyingStoreSellOn(state, visited.buyer_account_id, visited.store_id)) {
    buyer_had_board_ = false;
    sale_outcome_.clear();
  }
}

void BuyingStoreStage::AskRefresh(const TradeContext& context) {
  if (!store_.visiting()) {
    return;
  }
  if (auto asked = ro::net::RequestBuyingStoreList(context.link, store_.visited().buyer_account_id);
      !asked) {
    ro::core::LogError("échoppe d'achat : {}", ro::core::Describe(asked.error()));
    return;
  }
  refresh_asked_ = true;
}

void BuyingStoreStage::SaySaleOutcomes(ro::ui::BuyingStoreSellState& state,
                                       const TradeContext& context) {
  if (store_.sale_seq() != sale_seq_said_) {
    sale_seq_said_ = store_.sale_seq();
    const std::span<const ro::sim::BuyingStoreSale> sales = store_.sales();
    // Une autre échoppe a vidé la liste : tout ce qui y est est neuf.
    if (sales.size() < sales_said_) {
      sales_said_ = 0;
    }
    bool sold = false;
    for (std::size_t rank = sales_said_; rank < sales.size(); ++rank) {
      const ro::sim::BuyingStoreSale& sale = sales[rank];
      const auto named = std::ranges::find_if(
          sold_names_, [&sale](const auto& entry) { return entry.first == sale.client_slot; });
      const std::string name = named != sold_names_.end() ? named->second
                               : sale.item_id != 0        ? context.items.NameOf(sale.item_id)
                                                          : std::string("?");
      std::string line = SoldLineText(context.words, name, sale);
      sale_outcome_ = line;
      sale_outcome_done_ = true;
      Say(context, std::move(line), kBuyingStoreSoldColor);
      sold = true;
    }
    sales_said_ = sales.size();
    // LA VENTE EST FINIE quand la dernière ligne est confirmée : la vente
    // préparée a été servie, et la liste se redemande pour dire ce que
    // l'acheteur veut encore — d'autres vendeurs ont pu passer.
    if (sold && sale_sent_ && !store_.sale_pending()) {
      sale_sent_ = false;
      sold_names_.clear();
      state.cart.clear();
      state.typed.clear();
      AskRefresh(context);
    }
  }

  if (store_.sale_refusal_seq() != sale_refusal_said_) {
    sale_refusal_said_ = store_.sale_refusal_seq();
    const std::uint16_t code = store_.sale_refusal();
    std::string text;
    if (const std::optional<TradeWord> word = WordOfSaleRefusal(code); word.has_value()) {
      // Le dépassement nomme l'item par son identifiant, sans raffinement.
      text = *word == TradeWord::kBuyingStoreOverCount
                 ? ro::vfs::TradeTextWith(context.words, *word,
                                          context.items.NameOf(store_.sale_refusal_item()))
                 : PrintedWord(context.words, *word, 0);
    }
    if (text.empty()) {
      text = ro::core::Printed(ro::core::Tr("Le serveur a refusé la vente (code %d)."),
                               static_cast<int>(code));
    }
    sale_outcome_ = text;
    sale_outcome_done_ = false;
    Say(context, std::move(text), kBuyingStoreFailedColor);
    // 🔴 LA FENÊTRE RESTE, LA VENTE PRÉPARÉE AUSSI, ET LA LISTE SE REDEMANDE :
    // le client d'origine fermait tout, et le joueur devait recommencer sans
    // savoir ce qui avait changé. Le serveur a pu rabattre la limite de
    // l'acheteur sur ce qu'il possède : la liste fraîche le dira, et le verdict
    // retiendra la vente qui échouerait encore.
    sale_sent_ = false;
    sold_names_.clear();
    AskRefresh(context);
  }
}

void BuyingStoreStage::ComposeSell(const ro::ui::BuyingStoreSellState& state,
                                   const TradeContext& context) {
  const ro::sim::World& world = context.world;
  const ro::sim::VisitedBuyingStore& visited = store_.visited();
  const std::span<const ro::sim::InventoryItem> bag = world.inventory().items();

  const ro::sim::Actor* buyer = world.Find(visited.buyer_account_id);
  buyer_ = buyer != nullptr ? buyer->name : std::string();
  const ro::sim::Signboard* board = BuyingBoardOf(buyer);
  store_title_ = board != nullptr ? board->title : std::string();
  sell_view_.buyer = buyer_;
  sell_view_.store_title = store_title_;

  wanted_names_.clear();
  wanted_.clear();
  for (const ro::sim::BuyingStoreLine& line : visited.lines) {
    wanted_names_.push_back(context.items.NameOf(line.item_id));
    ro::ui::BuyingStoreWantedView shown;
    shown.item.item_id = line.item_id;
    shown.item.icon = context.items.IconOf(line.item_id);
    shown.wanted = line.wanted;
    shown.price = line.price;
    wanted_.push_back(shown);
  }
  for (std::size_t index = 0; index < wanted_.size(); ++index) {
    wanted_[index].item.name = wanted_names_[index];
  }
  sell_view_.wanted = wanted_;

  const std::vector<BuyingStoreSaleCandidate> candidates = BuyingStoreSaleCandidates(visited, bag);
  offer_names_.clear();
  offer_bars_.clear();
  offers_.clear();
  for (const BuyingStoreSaleCandidate& candidate : candidates) {
    const ro::sim::InventoryItem* item = world.inventory().Find(candidate.client_slot);
    offer_names_.push_back(item != nullptr ? context.items.NameOf(*item, context.creators)
                                           : context.items.NameOf(candidate.item_id));
    offer_bars_.push_back(candidate.barred.has_value() ? SaleFaultText(*candidate.barred) : "");
    ro::ui::BuyingStoreOfferView shown;
    shown.item.item_id = candidate.item_id;
    shown.item.icon = context.items.IconOf(candidate.item_id);
    shown.item.damaged = item != nullptr && item->damaged;
    shown.client_slot = candidate.client_slot;
    shown.owned = candidate.owned;
    shown.price = candidate.unit_price;
    offers_.push_back(shown);
  }
  for (std::size_t index = 0; index < offers_.size(); ++index) {
    offers_[index].item.name = offer_names_[index];
    offers_[index].barred = offer_bars_[index];
  }
  sell_view_.offers = offers_;
  sell_view_.buyer_funds = visited.zeny_limit;

  const std::vector<ro::sim::BuyingStoreSaleLine> lines = SaleLinesOf(state.cart);
  sell_view_.cart_total = ro::sim::BuyingStoreSaleTotal(visited, lines);
  sale_blocker_texts_.clear();
  if (!lines.empty()) {
    for (const ro::sim::BuyingStoreSaleProblem& problem :
         ro::sim::CheckBuyingStoreSale(visited, lines, bag, KnownZeny(world.player_status()))) {
      std::string text = SaleFaultText(problem.fault);
      if (std::ranges::find(sale_blocker_texts_, text) == sale_blocker_texts_.end()) {
        sale_blocker_texts_.push_back(std::move(text));
      }
    }
  }
  sale_blockers_.assign(sale_blocker_texts_.begin(), sale_blocker_texts_.end());
  sell_view_.blockers = sale_blockers_;
  sell_view_.awaiting = store_.sale_pending();
  sell_view_.outcome = sale_outcome_;
  sell_view_.outcome_done = sale_outcome_done_;
}

}  // namespace moonclient

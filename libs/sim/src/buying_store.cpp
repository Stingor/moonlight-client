#include "ro/sim/buying_store.h"

#include <algorithm>
#include <utility>

namespace ro::sim {

// ══ MON ÉCHOPPE ═════════════════════════════════════════════════════════════

void BuyingStore::TellPreparation(std::uint8_t slots) {
  if (phase_ == BuyingStorePhase::kOpen) {
    return;
  }
  slots_ = static_cast<std::uint8_t>(std::min<std::size_t>(slots, kBuyingStoreMostSlots));
  phase_ = BuyingStorePhase::kPreparing;
  requested_title_.clear();
  requested_zeny_limit_ = 0;
  ++preparation_seq_;
}

void BuyingStore::CancelPreparation() noexcept {
  if (phase_ == BuyingStorePhase::kPreparing) {
    phase_ = BuyingStorePhase::kClosed;
    slots_ = 0;
  }
}

bool BuyingStore::TellRequested(std::string title, std::int64_t zeny_limit) noexcept {
  if (phase_ != BuyingStorePhase::kPreparing) {
    return false;
  }
  requested_title_ = std::move(title);
  requested_zeny_limit_ = zeny_limit;
  phase_ = BuyingStorePhase::kRequested;
  return true;
}

void BuyingStore::TellOpenRefused(std::uint16_t result, std::uint32_t weight) noexcept {
  if (request_abandoned_) {
    request_abandoned_ = false;
    return;
  }
  if (phase_ == BuyingStorePhase::kOpen) {
    return;
  }
  phase_ = BuyingStorePhase::kClosed;
  slots_ = 0;
  open_refusal_ = result;
  open_refusal_weight_ = weight;
  ++open_refusal_seq_;
}

void BuyingStore::TellOpened(std::uint32_t owner_account_id, std::int64_t zeny_limit,
                             std::span<const BuyingStoreLine> lines) {
  if (request_abandoned_) {
    request_abandoned_ = false;
    return;
  }
  phase_ = BuyingStorePhase::kOpen;
  owner_account_id_ = owner_account_id;
  title_ = std::move(requested_title_);
  requested_title_.clear();
  zeny_limit_ = zeny_limit;
  opening_zeny_limit_ = zeny_limit;
  lines_.assign(lines.begin(), lines.end());
  journal_.clear();
  spent_zeny_ = 0;
  ++journal_seq_;
  ++opening_seq_;
}

void BuyingStore::TellPurchase(const BuyingStorePurchase& purchase, std::int64_t zeny_limit_left) {
  const auto line = std::ranges::find(lines_, purchase.item_id, &BuyingStoreLine::item_id);
  if (line != lines_.end()) {
    line->wanted =
        static_cast<std::uint16_t>(line->wanted - std::min(line->wanted, purchase.amount));
  }
  zeny_limit_ = zeny_limit_left;
  journal_.push_back(purchase);
  spent_zeny_ += purchase.zeny;
  ++journal_seq_;
}

void BuyingStore::ClearJournal() noexcept {
  journal_.clear();
  spent_zeny_ = 0;
  ++journal_seq_;
}

void BuyingStore::TellEnded(std::uint16_t result) noexcept {
  ForgetMyStore();
  end_result_ = result;
  ++end_seq_;
}

void BuyingStore::Close() noexcept {
  switch (phase_) {
    case BuyingStorePhase::kClosed:
      return;
    case BuyingStorePhase::kPreparing:
      CancelPreparation();
      return;
    case BuyingStorePhase::kRequested:
      request_abandoned_ = true;
      break;
    case BuyingStorePhase::kOpen:
      break;
  }
  ForgetMyStore();
  farewell_due_ = true;
}

bool BuyingStore::TakeFarewell() noexcept {
  const bool due = farewell_due_;
  farewell_due_ = false;
  return due;
}

void BuyingStore::ForgetMyStore() noexcept {
  phase_ = BuyingStorePhase::kClosed;
  slots_ = 0;
  requested_title_.clear();
  requested_zeny_limit_ = 0;
  lines_.clear();
  zeny_limit_ = 0;
}

// ══ L'ÉCHOPPE VISITÉE ═══════════════════════════════════════════════════════

void BuyingStore::TellVisit(VisitedBuyingStore store) {
  const bool same_store = visiting_ && visited_.buyer_account_id == store.buyer_account_id &&
                          visited_.store_id == store.store_id;
  if (!same_store) {
    sales_.clear();
    earned_zeny_ = 0;
    ++sale_seq_;
  }
  visited_ = std::move(store);
  visiting_ = true;
  ++visit_seq_;
}

void BuyingStore::LeaveVisit() noexcept {
  visiting_ = false;
  visited_ = VisitedBuyingStore();
}

bool BuyingStore::TellSaleRequested(std::span<const BuyingStoreSaleLine> lines) {
  if (!visiting_ || lines.empty() || !pending_sale_.empty()) {
    return false;
  }
  pending_sale_.assign(lines.begin(), lines.end());
  return true;
}

void BuyingStore::TellSold(std::uint16_t client_slot, std::uint16_t amount,
                           std::uint32_t unit_price) {
  BuyingStoreSale sale;
  sale.client_slot = client_slot;
  sale.amount = amount;
  sale.unit_price = unit_price;
  sale.zeny = static_cast<std::int64_t>(amount) * unit_price;

  const auto pending =
      std::ranges::find(pending_sale_, client_slot, &BuyingStoreSaleLine::client_slot);
  if (pending != pending_sale_.end()) {
    sale.item_id = pending->item_id;
    pending_sale_.erase(pending);
  }

  if (visiting_ && sale.item_id != 0) {
    const auto line = std::ranges::find(visited_.lines, sale.item_id, &BuyingStoreLine::item_id);
    if (line != visited_.lines.end()) {
      line->wanted = static_cast<std::uint16_t>(line->wanted - std::min(line->wanted, amount));
    }
    visited_.zeny_limit = std::max<std::int64_t>(visited_.zeny_limit - sale.zeny, 0);
  }

  sales_.push_back(sale);
  earned_zeny_ += sale.zeny;
  ++sale_seq_;
}

void BuyingStore::TellSaleRefused(std::uint16_t result, std::uint32_t item_id) noexcept {
  pending_sale_.clear();
  sale_refusal_ = result;
  sale_refusal_item_ = item_id;
  ++sale_refusal_seq_;
}

// ══ LES CHANGEMENTS DE CARTE ════════════════════════════════════════════════

void BuyingStore::CloseForMapChange() noexcept {
  CancelPreparation();
  LeaveVisit();
  pending_sale_.clear();
}

void BuyingStore::CloseSilently() noexcept {
  ForgetMyStore();
  farewell_due_ = false;
  request_abandoned_ = false;
  LeaveVisit();
  pending_sale_.clear();
}

}  // namespace ro::sim

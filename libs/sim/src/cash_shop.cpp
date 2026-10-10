#include "ro/sim/cash_shop.h"

namespace ro::sim {

void CashShop::TellOpening(std::uint32_t vote_points, std::uint32_t event_points,
                           std::uint32_t tab) {
  if (open_) {
    Close();
    return;
  }
  vote_points_ = vote_points;
  event_points_ = event_points;
  opening_tab_ = tab;
  open_ = true;
  ++opening_seq_;
  last_catalogue_tab_ = kCashShopTabCount;
}

void CashShop::Close() noexcept {
  open_ = false;
  farewell_due_ = true;
}

void CashShop::CloseForMapChange() noexcept {
  if (open_) {
    Close();
  }
}

void CashShop::CloseSilently() noexcept {
  open_ = false;
  farewell_due_ = false;
}

bool CashShop::TakeFarewell() noexcept {
  const bool due = farewell_due_;
  farewell_due_ = false;
  return due;
}

void CashShop::TellCatalogue(std::uint16_t tab, std::span<const CashShopItem> items) {
  if (tab >= kCashShopTabCount) {
    return;
  }
  std::vector<CashShopItem>& listed = tabs_[tab];
  if (last_catalogue_tab_ != tab) {
    listed.clear();
    last_catalogue_tab_ = tab;
  }
  listed.insert(listed.end(), items.begin(), items.end());
  ++catalogue_seq_;
}

std::span<const CashShopItem> CashShop::tab(std::size_t tab_index) const noexcept {
  if (tab_index >= kCashShopTabCount) {
    return {};
  }
  return tabs_[tab_index];
}

void CashShop::TellPurchase(std::uint16_t result, std::uint32_t vote_points,
                            std::uint32_t event_points) {
  purchase_result_ = result;
  vote_points_ = vote_points;
  event_points_ = event_points;
  ++purchase_seq_;
}

}  // namespace ro::sim

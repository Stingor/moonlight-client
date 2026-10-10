#include "ro/sim/vending.h"

#include <algorithm>
#include <utility>

#include "ro/core/text.h"

namespace ro::sim {

// ── LA PRÉPARATION ─────────────────────────────────────────────────────────────

std::size_t Vending::permitted_lines() const noexcept {
  return std::min<std::size_t>(offered_lines_, kVendingMostLines);
}

void Vending::TellPreparation(std::uint16_t offered_lines) {
  preparing_ = true;
  offered_lines_ = offered_lines;
  opening_pending_ = false;
  pending_title_.clear();
  ++preparation_seq_;
}

void Vending::NoteOpeningSent(std::string title_utf8) {
  opening_pending_ = true;
  pending_title_ = std::move(title_utf8);
}

void Vending::CancelPreparation() noexcept {
  preparing_ = false;
  opening_pending_ = false;
  farewell_due_ = true;
}

bool Vending::TakeFarewell() noexcept {
  const bool due = farewell_due_;
  farewell_due_ = false;
  return due;
}

void Vending::TellOpeningResult(std::uint8_t result) {
  opening_result_ = result;
  ++opening_result_seq_;
  opening_pending_ = false;
  // Réussie, l'échoppe s'est déjà ouverte par sa liste ; échouée, le serveur a
  // remis la préparation à zéro. Dans les deux cas elle est finie.
  preparing_ = false;
}

// ── MON ÉCHOPPE ────────────────────────────────────────────────────────────────

void Vending::TellShopOpened(std::uint32_t owner_aid, std::vector<VendingLine> lines) {
  shop_title_ = opening_pending_ ? std::move(pending_title_) : std::string();
  pending_title_.clear();
  preparing_ = false;
  opening_pending_ = false;
  shop_open_ = true;
  shop_owner_aid_ = owner_aid;
  shop_lines_ = std::move(lines);
  ++shop_opening_seq_;
  ++shop_seq_;
  ClearJournal();
}

void Vending::TellSale(std::uint16_t cart_slot, std::uint16_t amount, std::uint32_t buyer_char_id,
                       std::uint32_t sold_at_unix_s, std::int32_t zeny) {
  VendingSale sale;
  sale.item.client_slot = cart_slot;
  sale.zeny = zeny;
  sale.buyer_char_id = buyer_char_id;
  sale.sold_at_unix_s = sold_at_unix_s;

  const auto line = std::ranges::find_if(shop_lines_, [cart_slot](const VendingLine& listed) {
    return listed.item.client_slot == cart_slot;
  });
  if (line != shop_lines_.end()) {
    sale.item = line->item;
    const int left = std::max(0, static_cast<int>(line->item.amount) - static_cast<int>(amount));
    line->item.amount = static_cast<std::int16_t>(left);
    if (left == 0) {
      shop_lines_.erase(line);
    }
    ++shop_seq_;
  }
  sale.item.amount = static_cast<std::int16_t>(amount);

  journal_.push_back(std::move(sale));
  earned_zeny_ += zeny;
  ++journal_seq_;
}

void Vending::CloseShop() noexcept {
  if (!shop_open_) {
    return;
  }
  shop_open_ = false;
  shop_lines_.clear();
  ++shop_seq_;
}

void Vending::ClearJournal() noexcept {
  journal_.clear();
  earned_zeny_ = 0;
  ++journal_seq_;
}

// ── L'ÉCHOPPE VISITÉE ──────────────────────────────────────────────────────────

void Vending::TellVisit(std::uint32_t vendor_aid, std::uint32_t vender_id,
                        std::vector<VendingLine> offers) {
  if (stale_refresh_aid_ != 0 && stale_refresh_aid_ == vendor_aid) {
    stale_refresh_aid_ = 0;
    return;
  }
  const bool after_purchase = purchase_pending_ && visiting_ && vendor_aid_ == vendor_aid;
  purchase_pending_ = false;
  if (after_purchase && offers.empty()) {
    EndVisit(VendingVisitEnd::kEmptied);
    return;
  }
  if (!visiting_ || vendor_aid_ != vendor_aid) {
    ++visit_opening_seq_;
  }
  visiting_ = true;
  visit_end_ = VendingVisitEnd::kNone;
  vendor_aid_ = vendor_aid;
  vender_id_ = vender_id;
  offers_ = std::move(offers);
  ++visit_seq_;
}

void Vending::NotePurchaseSent() noexcept { purchase_pending_ = true; }

void Vending::TellPurchaseRefused(std::uint16_t shop_slot, std::uint16_t amount,
                                  std::uint8_t result) {
  purchase_result_ = result;
  purchase_result_slot_ = shop_slot;
  purchase_result_amount_ = amount;
  ++purchase_result_seq_;
}

void Vending::EndVisit(VendingVisitEnd why) noexcept {
  if (!visiting_) {
    purchase_pending_ = false;
    return;
  }
  if (purchase_pending_) {
    stale_refresh_aid_ = vendor_aid_;
  }
  visiting_ = false;
  purchase_pending_ = false;
  visit_end_ = why;
  offers_.clear();
  ++visit_seq_;
}

void Vending::CloseVisit() noexcept { EndVisit(VendingVisitEnd::kClosed); }

void Vending::TellVendorGone(std::uint32_t vendor_aid) noexcept {
  if (visiting_ && vendor_aid_ == vendor_aid) {
    EndVisit(VendingVisitEnd::kVendorGone);
  }
}

// ── LES CHANGEMENTS DE CARTE ET DE SERVEUR ─────────────────────────────────────

void Vending::CloseForMapChange() noexcept {
  if (preparing_) {
    CancelPreparation();
  }
  EndVisit(VendingVisitEnd::kLeftMap);
}

void Vending::CloseSilently() noexcept {
  preparing_ = false;
  opening_pending_ = false;
  farewell_due_ = false;
  CloseShop();
  EndVisit(VendingVisitEnd::kLeftMap);
  // L'autre serveur ne répondra pas à une liste demandée à celui-ci.
  stale_refresh_aid_ = 0;
}

// ── LES CONTRÔLES AVANT ENVOI ──────────────────────────────────────────────────

std::string FitVendingTitle(std::string_view title_utf8) {
  if (core::WireFromUtf8Text(title_utf8).size() <= kVendingTitleMostTextBytes) {
    return std::string(title_utf8);
  }
  // On avance caractère par caractère, et l'on garde le dernier début qui
  // tient : la forme du fil (1252 ou UTF-8) dépend du texte entier, pas d'un
  // compte d'octets fait d'avance.
  std::size_t fits = 0;
  std::size_t at = 0;
  while (at < title_utf8.size()) {
    std::size_t next = at;
    static_cast<void>(core::NextCodePoint(title_utf8, next));
    if (core::WireFromUtf8Text(title_utf8.substr(0, next)).size() > kVendingTitleMostTextBytes) {
      break;
    }
    fits = next;
    at = next;
  }
  return std::string(title_utf8.substr(0, fits));
}

std::int64_t VendingSetupTakings(std::span<const VendingSetupLine> lines) noexcept {
  std::int64_t takings = 0;
  for (const VendingSetupLine& line : lines) {
    takings +=
        static_cast<std::int64_t>(std::min(line.price_zeny, kVendingMostPriceZeny)) * line.amount;
  }
  return takings;
}

std::vector<VendingSetupRefusal> CheckVendingSetup(std::string_view title_utf8,
                                                   std::span<const VendingSetupLine> lines,
                                                   std::size_t permitted_lines,
                                                   std::span<const InventoryItem> cart,
                                                   std::int64_t zeny) {
  std::vector<VendingSetupRefusal> refusals;
  const auto refuse = [&refusals](VendingSetupFault fault, std::size_t line) {
    refusals.push_back(VendingSetupRefusal{.fault = fault, .line = line});
  };

  if (title_utf8.empty()) {
    refuse(VendingSetupFault::kNoTitle, kVendingWholeRequest);
  }
  if (lines.empty()) {
    refuse(VendingSetupFault::kNoLines, kVendingWholeRequest);
  }
  if (lines.size() > std::min(permitted_lines, kVendingMostLines)) {
    refuse(VendingSetupFault::kTooManyLines, kVendingWholeRequest);
  }

  for (std::size_t rank = 0; rank < lines.size(); ++rank) {
    const VendingSetupLine& line = lines[rank];
    const auto earlier = lines.first(rank);
    if (std::ranges::any_of(earlier, [&line](const VendingSetupLine& other) {
          return other.cart_slot == line.cart_slot;
        })) {
      refuse(VendingSetupFault::kDuplicateSlot, rank);
    }
    if (line.price_zeny == 0) {
      refuse(VendingSetupFault::kZeroPrice, rank);
    }
    if (line.price_zeny > kVendingMostPriceZeny) {
      refuse(VendingSetupFault::kPriceAboveMost, rank);
    }
    if (line.amount == 0) {
      refuse(VendingSetupFault::kZeroAmount, rank);
    }
    const auto stack = std::ranges::find_if(cart, [&line](const InventoryItem& carted) {
      return carted.client_slot == line.cart_slot && carted.item_id != 0;
    });
    if (stack == cart.end()) {
      refuse(VendingSetupFault::kNotInCart, rank);
      continue;
    }
    if (static_cast<int>(line.amount) > static_cast<int>(stack->amount)) {
      refuse(VendingSetupFault::kAmountAboveStack, rank);
    }
    if (!stack->identified) {
      refuse(VendingSetupFault::kUnidentified, rank);
    }
    if (stack->damaged) {
      refuse(VendingSetupFault::kDamaged, rank);
    }
    if (stack->rented()) {
      refuse(VendingSetupFault::kRented, rank);
    }
  }

  if (zeny + VendingSetupTakings(lines) > kVendingMostZeny) {
    refuse(VendingSetupFault::kTakingsOverflow, kVendingWholeRequest);
  }
  return refusals;
}

std::vector<VendingBasketLine> MergeVendingBasket(std::span<const VendingBasketLine> basket) {
  std::vector<VendingBasketLine> merged;
  for (const VendingBasketLine& line : basket) {
    const auto same = std::ranges::find_if(merged, [&line](const VendingBasketLine& kept) {
      return kept.shop_slot == line.shop_slot;
    });
    if (same == merged.end()) {
      merged.push_back(line);
      continue;
    }
    // Saturée plutôt que repliée : une somme qui déborde doit rester trop
    // grande pour le stock, pas redevenir petite.
    const std::uint32_t sum = std::uint32_t{same->amount} + line.amount;
    same->amount = static_cast<std::uint16_t>(std::min<std::uint32_t>(sum, UINT16_MAX));
  }
  return merged;
}

namespace {

const VendingLine* FindOffer(std::span<const VendingLine> offers, std::uint16_t shop_slot) {
  const auto offer = std::ranges::find_if(offers, [shop_slot](const VendingLine& listed) {
    return listed.item.client_slot == shop_slot;
  });
  return offer == offers.end() ? nullptr : &*offer;
}

}  // namespace

std::int64_t VendingBasketCost(std::span<const VendingBasketLine> basket,
                               std::span<const VendingLine> offers) noexcept {
  std::int64_t cost = 0;
  for (const VendingBasketLine& line : basket) {
    if (const VendingLine* offer = FindOffer(offers, line.shop_slot)) {
      cost += static_cast<std::int64_t>(offer->price_zeny) * line.amount;
    }
  }
  return cost;
}

std::vector<VendingBasketRefusal> CheckVendingBasket(std::span<const VendingBasketLine> basket,
                                                     std::span<const VendingLine> offers,
                                                     std::int64_t zeny) {
  std::vector<VendingBasketRefusal> refusals;
  const auto refuse = [&refusals](VendingBasketFault fault, std::size_t line) {
    refusals.push_back(VendingBasketRefusal{.fault = fault, .line = line});
  };

  const std::vector<VendingBasketLine> merged = MergeVendingBasket(basket);
  if (merged.empty()) {
    refuse(VendingBasketFault::kEmpty, kVendingWholeRequest);
  }
  if (merged.size() > kVendingMostLines) {
    refuse(VendingBasketFault::kTooManyLines, kVendingWholeRequest);
  }
  for (std::size_t rank = 0; rank < merged.size(); ++rank) {
    const VendingBasketLine& line = merged[rank];
    if (line.amount == 0) {
      refuse(VendingBasketFault::kZeroAmount, rank);
    }
    const VendingLine* offer = FindOffer(offers, line.shop_slot);
    if (offer == nullptr) {
      refuse(VendingBasketFault::kUnknownOffer, rank);
      continue;
    }
    if (static_cast<int>(line.amount) > static_cast<int>(offer->item.amount)) {
      refuse(VendingBasketFault::kAmountAboveStock, rank);
    }
  }
  if (VendingBasketCost(merged, offers) > zeny) {
    refuse(VendingBasketFault::kTotalAboveZeny, kVendingWholeRequest);
  }
  return refusals;
}

}  // namespace ro::sim

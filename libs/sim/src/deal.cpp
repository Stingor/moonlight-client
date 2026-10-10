// L'échange entre joueurs. Voir l'en-tête.

#include "ro/sim/deal.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

#include "ro/sim/npc_shop_cart.h"
#include "ro/sim/pet_egg.h"

namespace ro::sim {
namespace {

// Deux piles du partenaire sont-elles le même item, donc une seule ligne ?
bool SameStack(const InventoryItem& left, const InventoryItem& right) noexcept {
  if (left.item_id != right.item_id || left.refine_level != right.refine_level ||
      left.grade != right.grade || left.identified != right.identified ||
      left.damaged != right.damaged || left.cards != right.cards) {
    return false;
  }
  for (std::size_t rank = 0; rank < kItemOptionSlots; ++rank) {
    const ItemOption& a = left.options[rank];
    const ItemOption& b = right.options[rank];
    if (a.id != b.id || a.value != b.value || a.param != b.param) {
      return false;
    }
  }
  return true;
}

std::int16_t SaturatedSum(std::int16_t held, std::int16_t added) noexcept {
  const int sum = static_cast<int>(held) + static_cast<int>(added);
  return static_cast<std::int16_t>(std::min(sum, int{std::numeric_limits<std::int16_t>::max()}));
}

}  // namespace

void Deal::Reset() {
  my_offer_.clear();
  partner_offer_.clear();
  pending_adds_.clear();
  silent_adds_.clear();
  my_zeny_ = 0;
  partner_zeny_ = 0;
  partner_zeny_changed_after_my_lock_ = false;
  lock_sent_ = false;
  my_locked_ = false;
  partner_locked_ = false;
  execute_sent_ = false;
  partner_revision_ = 0;
  partner_revision_at_my_lock_ = 0;
}

void Deal::Close() {
  Reset();
  phase_ = DealPhase::kNone;
  partner_ = DealPartner();
  answer_sent_ = false;
}

void Deal::Tell(DealNewsKind kind, std::uint8_t code, const InventoryItem* item) {
  DealNews news;
  news.seq = ++news_seq_;
  news.kind = kind;
  news.code = code;
  if (item != nullptr) {
    news.item = *item;
  }
  if (news_.size() == kDealNewsKept) {
    news_.erase(news_.begin());
  }
  news_.push_back(std::move(news));
}

// ── LA PROPOSITION ─────────────────────────────────────────────────────────

void Deal::NoteProposalSent(DealPartner partner, std::uint32_t now_ms) {
  Close();
  ForgetReturns();
  phase_ = DealPhase::kProposing;
  partner_ = std::move(partner);
  proposal_sent_ms_ = now_ms;
}

void Deal::TellProposal(std::string name, std::uint32_t account_id, std::uint16_t base_level) {
  Close();
  ForgetReturns();
  phase_ = DealPhase::kProposed;
  partner_ =
      DealPartner{.account_id = account_id, .name = std::move(name), .base_level = base_level};
  ++proposal_seq_;
}

void Deal::NoteAnswerSent(bool accept) {
  if (phase_ != DealPhase::kProposed) {
    return;
  }
  if (!accept) {
    Close();
    return;
  }
  answer_sent_ = true;
}

void Deal::TellAnswer(std::uint8_t code, std::uint32_t partner_aid, std::uint16_t partner_level) {
  if (code == kDealAnswerAccepted) {
    if (phase_ != DealPhase::kProposing && phase_ != DealPhase::kProposed) {
      return;
    }
    Reset();
    ForgetReturns();
    phase_ = DealPhase::kOpen;
    if (partner_aid != 0) {
      partner_.account_id = partner_aid;
    }
    partner_.base_level = partner_level;
    answer_sent_ = false;
    ++opening_seq_;
    return;
  }
  // Mon propre refus, que le serveur renvoie : déjà fermé, rien à dire.
  if (phase_ == DealPhase::kNone || phase_ == DealPhase::kOpen) {
    return;
  }
  Tell(DealNewsKind::kAnswered, code);
  Close();
}

// ── MON OFFRE ──────────────────────────────────────────────────────────────

std::int32_t Deal::InFlightOf(std::uint16_t client_slot) const noexcept {
  std::int32_t total = 0;
  for (const DealPendingAdd& pending : pending_adds_) {
    if (pending.item.client_slot == client_slot) {
      total += pending.amount;
    }
  }
  return total;
}

bool Deal::Engages(std::uint16_t client_slot) const noexcept {
  const auto same_slot = [client_slot](const auto& entry) {
    return entry.item.client_slot == client_slot;
  };
  return std::ranges::any_of(my_offer_, same_slot) ||
         std::ranges::any_of(pending_adds_, same_slot) ||
         std::ranges::any_of(silent_adds_, same_slot);
}

bool Deal::HasStake() const noexcept {
  return !my_offer_.empty() || !pending_adds_.empty() || !silent_adds_.empty() || my_zeny_ > 0;
}

std::size_t Deal::DistinctOffered() const noexcept {
  std::vector<std::uint16_t> slots;
  slots.reserve(my_offer_.size() + pending_adds_.size() + silent_adds_.size());
  for (const DealLine& line : my_offer_) {
    slots.push_back(line.item.client_slot);
  }
  for (const DealPendingAdd& pending : pending_adds_) {
    slots.push_back(pending.item.client_slot);
  }
  for (const DealPendingAdd& pending : silent_adds_) {
    slots.push_back(pending.item.client_slot);
  }
  std::ranges::sort(slots);
  return static_cast<std::size_t>(
      std::ranges::distance(slots.begin(), std::ranges::unique(slots).begin()));
}

std::int32_t Deal::MostAddable(const InventoryItem& bag_item) const noexcept {
  // ⚠ LE SAC EST DÉJÀ AMPUTÉ DE CE QUI EST ACCUSÉ : seul ce qui est en route se
  // retranche encore. Le serveur borne de même, sur sa pile entière.
  const std::int32_t left =
      static_cast<std::int32_t>(bag_item.amount) - InFlightOf(bag_item.client_slot);
  return std::clamp(left, std::int32_t{0}, kDealMostItemAmount);
}

DealAddFault Deal::CheckAdd(const InventoryItem& bag_item, std::int32_t amount) const noexcept {
  if (phase_ != DealPhase::kOpen) {
    return DealAddFault::kNotOpen;
  }
  if (lock_sent_ || my_locked_) {
    return DealAddFault::kLocked;
  }
  if (bag_item.equipped()) {
    return DealAddFault::kEquipped;
  }
  if (bag_item.rented()) {
    return DealAddFault::kRented;
  }
  if (IsPetEgg(bag_item.item_id) && bag_item.damaged) {
    return DealAddFault::kHatchedEgg;
  }
  const std::int32_t most = MostAddable(bag_item);
  if (most < 1) {
    return DealAddFault::kAllOffered;
  }
  if (amount < 1 || amount > most) {
    return DealAddFault::kBadAmount;
  }
  if (!Engages(bag_item.client_slot) && DistinctOffered() >= kDealMostItems) {
    return DealAddFault::kOfferFull;
  }
  return DealAddFault::kNone;
}

void Deal::NoteAddSent(const InventoryItem& bag_item, std::int16_t amount, std::uint32_t now_ms) {
  pending_adds_.push_back(DealPendingAdd{.item = bag_item, .amount = amount, .sent_ms = now_ms});
}

std::optional<DealTaken> Deal::TellAddResult(std::uint16_t client_slot, std::uint8_t code) {
  if (client_slot == kDealZenySlot || phase_ != DealPhase::kOpen) {
    return std::nullopt;
  }
  // L'ACCUSÉ SUIT L'ORDRE D'ENVOI : le premier dépôt de cette case est le sien,
  // fût-il déjà tenu pour muet.
  const auto same_slot = [client_slot](const DealPendingAdd& pending) {
    return pending.item.client_slot == client_slot;
  };
  DealPendingAdd answered;
  if (const auto found = std::ranges::find_if(pending_adds_, same_slot);
      found != pending_adds_.end()) {
    answered = std::move(*found);
    pending_adds_.erase(found);
  } else if (const auto late = std::ranges::find_if(silent_adds_, same_slot);
             late != silent_adds_.end()) {
    answered = std::move(*late);
    silent_adds_.erase(late);
  } else {
    return std::nullopt;
  }

  if (code != kDealAddSucceeded) {
    Tell(DealNewsKind::kAddRefused, code, &answered.item);
    return std::nullopt;
  }
  const auto line = std::ranges::find_if(my_offer_, [client_slot](const DealLine& offered) {
    return offered.item.client_slot == client_slot;
  });
  if (line != my_offer_.end()) {
    line->item.amount = SaturatedSum(line->item.amount, answered.amount);
  } else {
    DealLine offered;
    offered.item = answered.item;
    offered.item.amount = answered.amount;
    my_offer_.push_back(std::move(offered));
  }
  return DealTaken{.client_slot = client_slot, .amount = answered.amount};
}

std::size_t Deal::ExpireSilentAdds(std::uint32_t now_ms) {
  std::size_t expired = 0;
  for (auto pending = pending_adds_.begin(); pending != pending_adds_.end();) {
    if (now_ms - pending->sent_ms < kDealAddSilenceMs) {
      ++pending;
      continue;
    }
    Tell(DealNewsKind::kAddRefused, kDealAddSilent, &pending->item);
    silent_adds_.push_back(std::move(*pending));
    pending = pending_adds_.erase(pending);
    ++expired;
  }
  return expired;
}

DealZenyFault Deal::CheckZeny(std::int64_t amount, std::int64_t purse) const noexcept {
  if (phase_ != DealPhase::kOpen) {
    return DealZenyFault::kNotOpen;
  }
  if (lock_sent_ || my_locked_) {
    return DealZenyFault::kLocked;
  }
  if (amount < 0) {
    return DealZenyFault::kNegative;
  }
  if (amount > kDealMostZeny) {
    return DealZenyFault::kAboveMost;
  }
  // Une bourse que le serveur n'a pas dite ne couvre rien.
  if (amount > std::max<std::int64_t>(purse, 0)) {
    return DealZenyFault::kAbovePurse;
  }
  return DealZenyFault::kNone;
}

void Deal::NoteZenySent(std::int64_t amount) noexcept { my_zeny_ = amount; }

// ── LE PARTENAIRE ──────────────────────────────────────────────────────────

void Deal::NotePartnerChange() {
  ++partner_revision_;
  if (lock_sent_) {
    Tell(DealNewsKind::kPartnerChangedAfterMyLock);
  }
}

void Deal::TellPartnerItem(InventoryItem item) {
  if (phase_ != DealPhase::kOpen || item.amount <= 0) {
    return;
  }
  // Le partenaire n'a pas de cases chez moi : la case ne désigne rien.
  item.client_slot = 0;
  DealLine* merged = nullptr;
  if (ItemTypeStacks(item.type)) {
    const auto same = std::ranges::find_if(
        partner_offer_, [&item](const DealLine& line) { return SameStack(line.item, item); });
    if (same != partner_offer_.end()) {
      same->item.amount = SaturatedSum(same->item.amount, item.amount);
      merged = &*same;
    }
  }
  if (merged == nullptr) {
    partner_offer_.push_back(DealLine{.item = std::move(item)});
    merged = &partner_offer_.back();
  }
  if (lock_sent_) {
    merged->changed_after_my_lock = true;
  }
  NotePartnerChange();
}

void Deal::TellPartnerZeny(std::int64_t amount) {
  if (phase_ != DealPhase::kOpen || amount == partner_zeny_) {
    return;
  }
  partner_zeny_ = amount;
  if (lock_sent_) {
    partner_zeny_changed_after_my_lock_ = true;
  }
  NotePartnerChange();
}

bool Deal::partner_changed_after_my_lock() const noexcept {
  return lock_sent_ && partner_revision_ != partner_revision_at_my_lock_;
}

// ── LES VERROUS ET LA FIN ──────────────────────────────────────────────────

void Deal::NoteLockSent() noexcept {
  if (lock_sent_) {
    return;
  }
  lock_sent_ = true;
  partner_revision_at_my_lock_ = partner_revision_;
}

void Deal::TellLocked(bool partner) {
  if (phase_ != DealPhase::kOpen) {
    return;
  }
  if (partner) {
    if (!partner_locked_) {
      partner_locked_ = true;
      Tell(DealNewsKind::kPartnerLocked);
    }
    return;
  }
  NoteLockSent();
  my_locked_ = true;
}

DealExecuteFault Deal::CheckExecute(std::uint32_t confirmed_partner_revision,
                                    std::int64_t purse) const noexcept {
  if (phase_ != DealPhase::kOpen) {
    return DealExecuteFault::kNotOpen;
  }
  if (!my_locked_ || !partner_locked_) {
    return DealExecuteFault::kNotBothLocked;
  }
  if (execute_sent_) {
    return DealExecuteFault::kAlreadySent;
  }
  if (!pending_adds_.empty()) {
    return DealExecuteFault::kAddInFlight;
  }
  if (confirmed_partner_revision != partner_revision_) {
    return DealExecuteFault::kPartnerChanged;
  }
  if (my_zeny_ > purse) {
    return DealExecuteFault::kZenyMelted;
  }
  return DealExecuteFault::kNone;
}

void Deal::NoteExecuteSent() noexcept { execute_sent_ = true; }

void Deal::NoteCancelSent() {
  if (phase_ == DealPhase::kOpen) {
    Tell(DealNewsKind::kCancelled);
    AwaitReturns();
  }
  Close();
}

void Deal::TellCancelled() {
  if (phase_ == DealPhase::kOpen) {
    Tell(execute_sent_ ? DealNewsKind::kRefusedAtExecution : DealNewsKind::kCancelled);
    AwaitReturns();
  }
  // ⚠ MÊME FERMÉ : après MON annulation, c'est celle du serveur qui clôt le
  // renvoi des dépôts.
  SettleReturns();
  if (phase_ != DealPhase::kNone) {
    Close();
  }
}

void Deal::TellExecuted(std::uint8_t result) {
  // Conclu, l'échange n'a rien rendu : ce qui ressemblait à un dépôt est un gain.
  ForgetReturns();
  if (phase_ != DealPhase::kOpen) {
    return;
  }
  Tell(result == 0 ? DealNewsKind::kExecuted : DealNewsKind::kRefusedAtExecution, result);
  Close();
}

// ── LES DÉPÔTS RENDUS ──────────────────────────────────────────────────────

void Deal::AwaitReturns() {
  awaited_returns_.clear();
  for (const DealLine& line : my_offer_) {
    awaited_returns_.push_back(line.item);
  }
  awaited_zeny_ = my_zeny_ > 0;
}

void Deal::ForgetReturns() noexcept {
  awaited_returns_.clear();
  awaited_zeny_ = false;
  claimed_slots_.clear();
}

void Deal::SettleReturns() {
  if (!claimed_slots_.empty() || awaited_zeny_) {
    Tell(DealNewsKind::kStakeReturned);
  }
  ForgetReturns();
}

bool Deal::ClaimReturn(const InventoryItem& picked) {
  if (std::ranges::find(claimed_slots_, picked.client_slot) != claimed_slots_.end()) {
    return false;
  }
  const auto same_deposit = [&picked](const InventoryItem& deposit) {
    return deposit.client_slot == picked.client_slot && deposit.amount == picked.amount &&
           SameStack(deposit, picked);
  };
  bool found = false;
  if (phase_ == DealPhase::kOpen) {
    found = std::ranges::any_of(
        my_offer_, [&same_deposit](const DealLine& line) { return same_deposit(line.item); });
  } else {
    found = std::ranges::any_of(awaited_returns_, same_deposit);
  }
  if (found) {
    claimed_slots_.push_back(picked.client_slot);
  }
  return found;
}

void Deal::CloseForMapChange() {
  if (phase_ == DealPhase::kOpen) {
    Tell(DealNewsKind::kCancelled);
  }
  Close();
}

// ── CE QUE LES AUTRES GESTES CONSULTENT ────────────────────────────────────

bool DealReachable(int my_x, int my_y, int partner_x, int partner_y) noexcept {
  return std::abs(my_x - partner_x) <= kDealReachCells &&
         std::abs(my_y - partner_y) <= kDealReachCells;
}

bool DealEngages(const Deal& deal, std::uint16_t client_slot) noexcept {
  return deal.open() && deal.Engages(client_slot);
}

bool DealForbidsSkills(const Deal& deal) noexcept { return deal.open() && deal.HasStake(); }

bool DealForbidsZenySpending(const Deal& deal) noexcept {
  return deal.open() && deal.my_zeny() > 0;
}

}  // namespace ro::sim

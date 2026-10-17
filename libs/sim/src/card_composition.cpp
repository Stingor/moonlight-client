#include "ro/sim/card_composition.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace ro::sim {
namespace {

// RELIT UN OBJET RETENU DANS LE SAC. Vrai s'il y est sous la même identité —
// copié alors dans `kept` —, ou s'il n'y est plus et que la copie retenue
// vaut toujours. Faux : la place désigne un autre objet.
[[nodiscard]] bool Reread(const Inventory& bag, InventoryItem& kept) {
  const InventoryItem* now = bag.Find(kept.client_slot);
  if (now == nullptr) {
    return true;
  }
  if (now->item_id != kept.item_id) {
    return false;
  }
  kept = *now;
  return true;
}

}  // namespace

void CardComposition::AskInsert(std::uint16_t card_slot, std::uint16_t equip_slot,
                                const Inventory& bag) {
  const InventoryItem* card = bag.Find(card_slot);
  const InventoryItem* equip = bag.Find(equip_slot);
  if (card == nullptr || equip == nullptr) {
    return;
  }
  asks_.push_back(CardInsertAsk{.card = *card, .equip = *equip});
  while (asks_.size() > kMostCardInsertAsks) {
    asks_.pop_front();
  }
}

void CardComposition::TellVerdict(CardInsertVerdict verdict, std::uint16_t equip_slot,
                                  std::uint16_t card_slot, const Inventory& bag) {
  verdict_ = verdict;
  ++verdict_seq_;
  CardInsertReport report{.verdict = verdict};
  const auto answered = std::ranges::find_if(asks_, [&](const CardInsertAsk& ask) {
    return ask.card.client_slot == card_slot && ask.equip.client_slot == equip_slot;
  });
  if (answered != asks_.end()) {
    CardInsertAsk ask = *answered;
    asks_.erase(asks_.begin(), answered + 1);
    if (Reread(bag, ask.card) && Reread(bag, ask.equip)) {
      report.matched = true;
      report.card = ask.card;
      report.equip = ask.equip;
    }
  }
  reports_.push_back(std::move(report));
  while (reports_.size() > kMostCardInsertAsks) {
    reports_.pop_front();
  }
}

std::vector<CardInsertReport> CardComposition::TakeReports() {
  std::vector<CardInsertReport> taken(std::make_move_iterator(reports_.begin()),
                                      std::make_move_iterator(reports_.end()));
  reports_.clear();
  return taken;
}

void CardComposition::Offer(std::vector<std::uint16_t> equip_slots) {
  offer_.card_slot = asked_card_slot_;
  offer_.equip_slots = std::move(equip_slots);
  open_ = true;
  ++offer_seq_;
}

void CardComposition::AskCompatible(std::uint16_t equip_slot) {
  compatible_ = CompatibleCards{};
  compatible_.equip_slot = equip_slot;
}

bool CardComposition::TellCompatible(std::uint16_t equip_slot,
                                     std::vector<std::uint16_t> card_slots) {
  if (compatible_.equip_slot == 0 || equip_slot != compatible_.equip_slot) {
    return false;
  }
  compatible_.answered = true;
  compatible_.card_slots = std::move(card_slots);
  return true;
}

}  // namespace ro::sim

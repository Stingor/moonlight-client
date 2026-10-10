#include "ro/sim/card_composition.h"

#include <utility>

namespace ro::sim {

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

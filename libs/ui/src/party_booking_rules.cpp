// Les règles de la fenêtre de recherche de groupe, sans dessin. Voir
// `ro/ui/party_booking_window.h`.

#include <algorithm>
#include <cstddef>

#include "ro/ui/party_booking_window.h"

namespace ro::ui {

std::size_t PartyBookingSlotsUsed(const PartyBookingSlots& slots) {
  return static_cast<std::size_t>(std::count_if(slots.begin(), slots.end(), [](std::int16_t code) {
    return code != kPartyBookingEmptySlot;
  }));
}

bool TogglePartyBookingSlot(PartyBookingSlots& slots, std::int16_t code) {
  const auto found = std::find(slots.begin(), slots.end(), code);
  if (found != slots.end()) {
    // Décocher tasse les suivantes vers le début.
    std::copy(found + 1, slots.end(), found);
    slots.back() = kPartyBookingEmptySlot;
    return true;
  }
  const std::size_t used = PartyBookingSlotsUsed(slots);
  if (used == slots.size()) {
    return false;
  }
  slots[used] = code;
  return true;
}

std::int16_t PartyBookingChosenPlace(const PartyBookingView& view, int region, int place) {
  if (region < 0 || static_cast<std::size_t>(region) >= view.regions.size()) {
    return kPartyBookingEverywhere;
  }
  const PartyBookingRegionView& chosen = view.regions[static_cast<std::size_t>(region)];
  if (place < 0 || static_cast<std::size_t>(place) >= chosen.places.size()) {
    return kPartyBookingEverywhere;
  }
  return chosen.places[static_cast<std::size_t>(place)].map_code;
}

PartyBookingAction PartyBookingSearchAsked(const PartyBookingWindowState& state,
                                           const PartyBookingView& view) {
  PartyBookingAction action;
  action.verb = PartyBookingVerb::kSearch;
  action.level = static_cast<std::int16_t>(std::clamp(state.search_level, 0, view.most_level));
  switch (state.filter) {
    case PartyBookingFilter::kNone:
      break;
    case PartyBookingFilter::kPlace:
      action.map_code = PartyBookingChosenPlace(view, state.search_region, state.search_place);
      break;
    case PartyBookingFilter::kJob:
      action.job = state.search_job;
      break;
  }
  return action;
}

PartyBookingAction PartyBookingRegisterAsked(const PartyBookingWindowState& state,
                                             const PartyBookingView& view) {
  PartyBookingAction action;
  action.verb = PartyBookingVerb::kRegister;
  action.level = static_cast<std::int16_t>(
      std::clamp(state.register_level, view.least_level, view.most_level));
  action.map_code = PartyBookingChosenPlace(view, state.register_region, state.register_place);
  action.jobs = state.register_jobs;
  return action;
}

}  // namespace ro::ui

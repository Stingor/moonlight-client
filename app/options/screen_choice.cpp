#include "options/screen_choice.h"

#include <cstddef>
#include <cstdlib>
#include <string>

namespace moonclient {

int PickScreen(const ScreenChoice& saved, std::span<const ro::render::ScreenInfo> connected) {
  if (connected.empty()) {
    return -1;
  }
  int primary = 0;
  for (std::size_t rank = 0; rank < connected.size(); ++rank) {
    if (connected[rank].primary) {
      primary = static_cast<int>(rank);
      break;
    }
  }
  if (saved.name.empty()) {
    return primary;
  }
  // Le même nom, au rang le plus proche de celui qu'on a gardé ; à distance
  // égale, le premier. Le rang gardé lui-même est à distance nulle.
  int best = -1;
  int best_distance = 0;
  for (std::size_t rank = 0; rank < connected.size(); ++rank) {
    if (connected[rank].name != saved.name) {
      continue;
    }
    const int distance = std::abs(static_cast<int>(rank) - saved.rank);
    if (best < 0 || distance < best_distance) {
      best = static_cast<int>(rank);
      best_distance = distance;
    }
  }
  return best >= 0 ? best : primary;
}

std::string ScreenLabel(int rank, const ro::render::ScreenInfo& screen) {
  std::string label = std::to_string(rank + 1);
  if (!screen.name.empty()) {
    label += " — " + screen.name;
  }
  if (screen.desktop.width > 0 && screen.desktop.height > 0) {
    label += " (" + std::to_string(screen.desktop.width) + "×" +
             std::to_string(screen.desktop.height) + ")";
  }
  return label;
}

void OfferScreens(std::span<const ro::render::ScreenInfo> connected, ScreenOffer& into) {
  into.screens.assign(connected.begin(), connected.end());
  into.labels.clear();
  into.choices.clear();
  into.labels.reserve(into.screens.size());
  for (std::size_t rank = 0; rank < into.screens.size(); ++rank) {
    into.labels.push_back(ScreenLabel(static_cast<int>(rank), into.screens[rank]));
  }
  // ⚠ LES POINTEURS APRÈS LA BOUCLE, jamais dedans : le vecteur se réalloue en
  // grandissant, et une vue posée plus tôt pendrait dans le vide.
  into.choices.reserve(into.labels.size());
  for (const std::string& label : into.labels) {
    into.choices.push_back(label.c_str());
  }
}

ScreenChoice ScreenChoiceAt(const ScreenOffer& offer, int rank, const ScreenChoice& kept) {
  if (rank < 0 || rank >= static_cast<int>(offer.screens.size())) {
    return kept;
  }
  return ScreenChoice{.name = offer.screens[static_cast<std::size_t>(rank)].name, .rank = rank};
}

}  // namespace moonclient

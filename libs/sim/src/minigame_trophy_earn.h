#pragma once

#include <cstddef>
#include <vector>

#include "ro/sim/minigame_trophies.h"

namespace ro::sim {

// GAGNE `trophy` si `earned` et s'il ne l'était pas : l'ajoute alors à
// `fresh`. Un trophée gagné ne se perd jamais.
inline void EarnMinigameTrophy(MinigameRecord& record, MinigameTrophy trophy, bool earned,
                               std::vector<MinigameTrophy>& fresh) {
  bool& unlocked = record.unlocked[static_cast<std::size_t>(trophy)];
  if (earned && !unlocked) {
    unlocked = true;
    fresh.push_back(trophy);
  }
}

}  // namespace ro::sim

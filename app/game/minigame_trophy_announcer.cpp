// Les trophées des mini-jeux dits au serveur. Voir l'en-tête.

#include "game/minigame_trophy_announcer.h"

#include <cstdint>

#include "ro/core/log.h"
#include "ro/net/minigame_trophy.h"

namespace moonclient {

void MinigameTrophyAnnouncer::Pump(ro::net::Connection& link, bool verified,
                                   std::uint32_t map_entries,
                                   const ro::sim::MinigameRecord& record) {
  if (!verified || map_entries != told_map_entries_) {
    told_ = {};
    told_map_entries_ = map_entries;
  }
  if (!verified) {
    return;
  }
  for (std::size_t rank = 0; rank < told_.size(); ++rank) {
    if (told_[rank] || !record.unlocked[rank]) {
      continue;
    }
    told_[rank] = true;
    if (auto sent = ro::net::SendMinigameTrophy(link, static_cast<std::uint8_t>(rank)); !sent) {
      ro::core::LogWarning("trophée de mini-jeu non dit au serveur : {}", sent.error().detail);
    }
  }
}

}  // namespace moonclient

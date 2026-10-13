// Les bulles d'annonce à montrer. Voir l'en-tête.

#include "ro/sim/broadcast_balloon.h"

#include <utility>

namespace ro::sim {

void BalloonNews::Post(BalloonNotice notice) {
  if (notice.lines.empty()) {
    return;
  }
  if (notices_.size() >= kBalloonNewsKept) {
    notices_.erase(notices_.begin());
  }
  notices_.push_back(std::move(notice));
}

std::vector<BalloonNotice> BalloonNews::Take() { return std::exchange(notices_, {}); }

}  // namespace ro::sim

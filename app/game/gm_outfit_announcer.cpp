// L'habit du GM dit au serveur. Voir l'en-tête.

#include "game/gm_outfit_announcer.h"

#include "ro/core/log.h"
#include "ro/net/gm_outfit.h"

namespace moonclient {

void GmOutfitAnnouncer::Pump(ro::net::Connection& link, bool verified, bool admin, bool worn) {
  if (!verified) {
    Rearm();
    return;
  }
  if (!admin || (told_ && worn == last_worn_)) {
    return;
  }
  told_ = true;
  last_worn_ = worn;
  if (auto sent = ro::net::SendGmOutfit(link, worn); !sent) {
    ro::core::LogWarning("habit du GM non dit au serveur : {}", sent.error().detail);
    return;
  }
  ro::core::LogInfo("habit du GM dit au serveur : {}", worn ? "porté" : "quitté");
}

}  // namespace moonclient

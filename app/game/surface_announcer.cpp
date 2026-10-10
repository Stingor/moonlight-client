// L'annonce des surfaces de ce client au serveur. Voir l'en-tête.

#include "game/surface_announcer.h"

#include "ro/core/log.h"

namespace moonclient {

void SurfaceAnnouncer::Pump(ro::net::Connection& link, bool verified, std::uint32_t caps) {
  // Sans reconnaissance, le serveur n'écoute pas : la prochaine vérification
  // devra annoncer de nouveau, quel que soit le masque.
  if (!verified) {
    Rearm();
    return;
  }
  if (announced_ && caps == last_caps_) {
    return;
  }
  announced_ = true;
  last_caps_ = caps;
  if (auto sent = ro::net::AnnounceUiCaps(link, caps); !sent) {
    ro::core::LogWarning("surfaces non annoncées : {}", sent.error().detail);
    return;
  }
  ro::core::LogInfo("surfaces annoncées au serveur : {:#010x}", caps);
}

}  // namespace moonclient

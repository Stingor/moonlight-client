#pragma once

#include <string>

namespace moonclient {

// UN CORRESPONDANT RÉCENT, tel qu'il se garde d'une partie à l'autre : son nom,
// et le sens du DERNIER chuchotement échangé avec lui.
//
// ⚠ SANS DÉPENDANCE, et c'est voulu : le fichier d'historique l'écrit sans rien
// savoir de l'interface, et la barre le montre sans rien savoir du fichier.
struct WhisperPeer {
  std::string name;
  // Il nous a chuchoté en dernier ; faux : c'est nous qui lui avons chuchoté.
  bool incoming = false;

  friend bool operator==(const WhisperPeer&, const WhisperPeer&) = default;
};

}  // namespace moonclient

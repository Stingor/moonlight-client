#pragma once

// UNE TRAME ENCODÉE PART SUR LA LIAISON, ou l'erreur de son encodage remonte et
// rien ne part.
//
// ⚠ DEUX FORMES D'`Encode` COEXISTENT, et elles ne se généralisent pas : celle
// des paquets engendrés rend les octets, celle des paquets écrits à la main rend
// un `Result` parce qu'elle CONTRÔLE d'abord que leur disposition correspond
// encore à ce que le serveur déclare lire. Ce contrôle est tout l'intérêt d'un
// paquet manuel : sans lui, un changement de version enverrait des octets bien
// formés au mauvais endroit. C'est ce `Result` qui passe par ici.
//
// En-tête PRIVÉ de `libs/net/src`, commun aux guichets qui encodent une
// demande avant de l'envoyer.

#include <cstdint>
#include <expected>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"

namespace ro::net {

inline core::Result<void> SendEncoded(Connection& link,
                                      const core::Result<std::vector<std::uint8_t>>& frame) {
  if (!frame) {
    return std::unexpected(frame.error());
  }
  link.Send(*frame);
  return {};
}

}  // namespace ro::net

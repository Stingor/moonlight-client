#pragma once

// UNE TRAME ENCODÉE PART SUR LA LIAISON, ou l'erreur de son encodage remonte et
// rien ne part.
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

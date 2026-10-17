#pragma once

// LA DISPOSITION D'UNE DEMANDE ÉCRITE À LA MAIN, confrontée à ce que le serveur
// déclare lire.
//
// En-tête PRIVÉ de `libs/proto/src`, commun aux fichiers de paquets. 🔴 AUCUNE
// COPIE LOCALE : une recopie diverge à la première correction.

#include <cstddef>
#include <string_view>

#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"

namespace ro::proto {

// Une demande de taille FIXE : le gestionnaire, les décalages et la longueur
// que la table du serveur déclare doivent tous concorder.
template <typename Request, typename Offsets>
[[nodiscard]] bool FixedLayoutHolds(std::string_view handler, const Offsets& offsets) {
  const auto length = PacketLength(Request::kOpcode);
  return MatchesServerLayout(Request::kOpcode, handler, offsets) && length.has_value() &&
         static_cast<std::size_t>(*length) == Request::kWireSize;
}

// Une demande de taille VARIABLE : la table doit la déclarer comme telle.
template <typename Request, typename Offsets>
[[nodiscard]] bool VariableLayoutHolds(std::string_view handler, const Offsets& offsets) {
  const auto length = PacketLength(Request::kOpcode);
  return MatchesServerLayout(Request::kOpcode, handler, offsets) && length.has_value() &&
         *length == kVariableLength;
}

}  // namespace ro::proto

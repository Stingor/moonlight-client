#pragma once

// UNE DEMANDE RÉDUITE À SON OPCODE : aucun champ, donc rien à écrire que les
// deux octets qui la nomment.
//
// En-tête PRIVÉ de `libs/proto/src`, commun aux fichiers de paquets. 🔴 AUCUNE
// COPIE LOCALE : une recopie diverge à la première correction.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/byte_writer.h"
#include "ro/core/error.h"

#include "layout_holds.h"

namespace ro::proto {

// La trame d'une demande sans champ. La confronter au serveur reste à
// l'appelant.
template <typename Request>
[[nodiscard]] std::vector<std::uint8_t> OpcodeOnlyFrame() {
  std::vector<std::uint8_t> frame;
  frame.reserve(Request::kWireSize);
  core::ByteWriter writer(frame);
  writer.U16(Request::kOpcode);
  return frame;
}

// Aucun champ, donc aucun décalage à recouper : seuls le gestionnaire et la
// longueur peuvent mentir. `what` nomme la demande en tête du message d'échec.
template <typename Request>
[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodeOpcodeOnly(std::string_view handler,
                                                                       std::string_view what) {
  if (!FixedLayoutHolds<Request>(handler, std::array<std::uint16_t, 0>{})) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion,
                      std::string(what) + " ne correspond plus à ce que le serveur déclare lire");
  }
  return OpcodeOnlyFrame<Request>();
}

}  // namespace ro::proto

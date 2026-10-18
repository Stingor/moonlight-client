#include "ro/proto/stop_attack_packet.h"

#include <array>
#include <string_view>

#include "ro/core/byte_writer.h"

#include "layout_holds.h"

namespace ro::proto {
namespace {

// ⚠ AUCUN CHAMP : le serveur enregistre ce paquet sous son gestionnaire et sa
// longueur seuls. Le recoupement porte donc sur les deux.
constexpr std::string_view kStopAttackHandler = "clif_parse_StopAttack";
constexpr std::array<std::uint16_t, 0> kStopAttackOffsets = {};

}  // namespace

core::Result<std::vector<std::uint8_t>> EncodeStopAttack() {
  if (!FixedLayoutHolds<StopAttackRequest>(kStopAttackHandler, kStopAttackOffsets)) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion,
                      "la disposition écrite pour l'arrêt d'une attaque ne correspond plus à ce "
                      "que le serveur déclare lire");
  }
  std::vector<std::uint8_t> frame;
  core::ByteWriter writer(frame);
  writer.U16(StopAttackRequest::kOpcode);
  return frame;
}

}  // namespace ro::proto

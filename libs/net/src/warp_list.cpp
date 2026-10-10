// La liste de destinations, et la réponse. Voir l'en-tête.

#include "ro/net/warp_list.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/card_composition.h"
#include "ro/sim/world.h"

namespace ro::net {
namespace {

// L'offset de la compétence, après l'opcode et la longueur.
constexpr std::size_t kSkillOffset = 4;

[[nodiscard]] FrameOutcome ApplyWarpList(const Frame& frame, sim::World& world) {
  if (frame.bytes.size() < kWarpListHeaderSize) {
    return FrameOutcome::kUndecodable;
  }
  core::ByteReader reader(frame.bytes);
  if (!reader.Seek(kSkillOffset)) {
    return FrameOutcome::kUndecodable;
  }
  const std::uint16_t skill_id = reader.U16();
  // ⚠ LE COMPTE EST UNE DIVISION ENTIÈRE : des octets de trop après la dernière
  // entrée complète ne refusent pas la liste, ils sont ignorés.
  const std::size_t count = reader.remaining() / kWarpListEntrySize;
  std::vector<std::string> destinations;
  destinations.reserve(count);
  for (std::size_t rank = 0; rank < count; ++rank) {
    const std::string_view field = reader.FixedString(kWarpListEntrySize);
    const std::string_view kept = field.substr(0, std::min(field.size(), kWarpListKeptNameSize));
    // Une entrée vide ne fait pas de ligne.
    if (!kept.empty()) {
      destinations.emplace_back(kept);
    }
  }
  world.warp_menu().Offer(skill_id, std::move(destinations), world.map_entries());
  return FrameOutcome::kApplied;
}

}  // namespace

FrameOutcome ApplyWarpListFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcWarplist::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  return ApplyWarpList(frame, world);
}

core::Result<void> RequestWarpPoint(Connection& link, std::uint16_t skill_id,
                                    std::string_view destination) {
  proto::CzSelectWarppoint request;
  request.skill_id = static_cast<std::int16_t>(skill_id);
  request.mapname = std::string(destination);
  link.Send(proto::Encode(request));
  return {};
}

}  // namespace ro::net

// Les règles du serveur. Voir l'en-tête.

#include "ro/net/server_rules.h"

#include <cstddef>

#include "ro/proto/map_packets.h"
#include "ro/sim/session_settings.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// Où la trame porte sa propre longueur : juste après l'opcode.
constexpr std::size_t kLengthOffset = 2;
constexpr unsigned kBitsPerByte = 8;

// La longueur que la trame annonce, petit-boutiste.
[[nodiscard]] std::size_t AnnouncedLength(const Frame& frame) {
  if (frame.bytes.size() < kLengthOffset + 2) {
    return 0;
  }
  return static_cast<std::size_t>(frame.bytes[kLengthOffset]) |
         (static_cast<std::size_t>(frame.bytes[kLengthOffset + 1]) << kBitsPerByte);
}

void Keep(sim::SessionSettings& session, const proto::ZcMoonlightServerRulesEntry& rule) {
  switch (static_cast<ServerRuleKey>(rule.key)) {
    case ServerRuleKey::kViewCells:
      if (rule.value > 0) {
        session.view_cells = rule.value;
      }
      return;
    case ServerRuleKey::kHomunculusMaxLevel:
      if (rule.value > 0) {
        session.homunculus_max_level = rule.value;
      }
      return;
    case ServerRuleKey::kHomunculusSMaxLevel:
      if (rule.value > 0) {
        session.homunculus_s_max_level = rule.value;
      }
      return;
    case ServerRuleKey::kHomunculusTeleportHoldMs:
      if (rule.value >= 0) {
        session.homunculus_teleport_hold_ms = static_cast<std::uint32_t>(rule.value);
      }
      return;
  }
  // Une clé que ce client ne connaît pas : le serveur en a ajouté une.
}

}  // namespace

FrameOutcome ApplyServerRulesFrame(const Frame& frame, sim::World& world) {
  if (frame.opcode != proto::ZcMoonlightServerRules::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  if (AnnouncedLength(frame) != frame.bytes.size()) {
    return FrameOutcome::kUndecodable;
  }
  return Decoded<proto::ZcMoonlightServerRules>(frame, [&world](const auto& packet) {
    for (const proto::ZcMoonlightServerRulesEntry& rule : packet.rules) {
      Keep(world.session(), rule);
    }
    return FrameOutcome::kApplied;
  });
}

}  // namespace ro::net

// Le bond de Body Relocation. Voir l'en-tête.

#include "ro/net/snap.h"

#include <array>

#include "ro/proto/snap_packets.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// LE GLISSEMENT DU BOND : quinze millisecondes par case, dix fois plus vite
// qu'une marche ordinaire.
constexpr std::uint16_t kSnapGlideMsPerCell = 15;

// LA TRAÎNÉE QU'UN JOUEUR LAISSE EN BONDISSANT : l'effet 166, blanc, qui vit
// cent tics d'effet de seize millisecondes.
constexpr std::uint32_t kSnapTrailEffect = 166;
constexpr std::array<float, 3> kSnapTrailColor = {250.0F, 250.0F, 250.0F};
constexpr std::uint32_t kSnapTrailLifeTicks = 100;
constexpr std::uint32_t kEffectTickMs = 16;

}  // namespace

FrameOutcome ApplySnapFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  if (frame.opcode != proto::SnapNotice::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  return Decoded<proto::SnapNotice>(frame, [&world, now_ms](const auto& packet) {
    const sim::Actor* actor = world.Find(packet.actor_id);
    if (actor == nullptr) {
      return FrameOutcome::kIgnored;
    }
    // LA TRAÎNÉE D'ABORD, pour un joueur seulement : elle sème ses copies
    // pendant que le corps glisse.
    if (actor->kind == sim::ActorKind::kPlayer) {
      sim::PlayedEffect trail;
      trail.id = kSnapTrailEffect;
      trail.since_ms = now_ms;
      trail.told_color = kSnapTrailColor;
      trail.ends_ms = now_ms + kSnapTrailLifeTicks * kEffectTickMs;
      world.RecordPlayedEffect(packet.actor_id, trail);
    }
    // PUIS LE GLISSEMENT vers la case d'arrivée, à la vitesse du bond. Le
    // regard reste celui qu'il montrait : le bond ne dit pas de direction.
    world.StartGliding(
        packet.actor_id,
        sim::CellPosition{static_cast<std::uint16_t>(packet.x),
                          static_cast<std::uint16_t>(packet.y), sim::FacingAt(*actor, now_ms)},
        kSnapGlideMsPerCell, now_ms);
    return FrameOutcome::kApplied;
  });
}

}  // namespace ro::net

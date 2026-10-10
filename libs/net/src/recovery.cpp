// La vie et la magie rendues au joueur. Voir l'en-tête pour ce que le client
// en fait, et pourquoi l'ajout à la jauge de magie n'est pas une redite.

#include "ro/net/recovery.h"

#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

#include "frame_apply.h"

namespace ro::net {
namespace {

// Ajoute le montant à la vie du joueur. Une vie encore inconnue le reste : il
// n'y a rien à quoi ajouter, et le serveur la redira.
void AddToHealth(sim::World& world, std::int32_t amount) {
  const sim::Actor* const self = world.Find(world.player_id());
  if (self == nullptr || self->health == sim::kUnknownHealth) {
    return;
  }
  static_cast<void>(world.SetHealth(world.player_id(), self->health + amount, self->max_health));
}

// Ajoute le montant à la magie du joueur, sous la même réserve.
void AddToMagic(sim::World& world, std::int32_t amount) {
  const std::int64_t magic = world.player_status().sp;
  if (magic == sim::kUnknownStat) {
    return;
  }
  static_cast<void>(world.SetPlayerParameter(static_cast<std::uint16_t>(sim::ServerParameter::kSp),
                                             magic + amount));
}

}  // namespace

FrameOutcome ApplyRecoveryFrame(const Frame& frame, sim::World& world, std::uint32_t now_ms) {
  if (frame.opcode != proto::ZcRecovery::kOpcode) {
    return FrameOutcome::kUnhandled;
  }
  return Decoded<proto::ZcRecovery>(frame, [&world, now_ms](const auto& packet) {
    const auto restored = static_cast<sim::ServerParameter>(packet.type);
    std::uint32_t effect = 0;
    if (restored == sim::ServerParameter::kHp) {
      AddToHealth(world, packet.amount);
      world.damage_meter().SetSelf(world.player_id());
      world.damage_meter().NoteRecovery(packet.amount, now_ms);
      effect = kHpRecoveryEffect;
    } else if (restored == sim::ServerParameter::kSp) {
      AddToMagic(world, packet.amount);
      effect = kSpRecoveryEffect;
    } else {
      return FrameOutcome::kIgnored;
    }
    // Le montant voyage avec l'effet : c'est lui que le chiffre écrira.
    return world.RecordEffect(world.player_id(), effect, now_ms, packet.amount)
               ? FrameOutcome::kApplied
               : FrameOutcome::kIgnored;
  });
}

}  // namespace ro::net

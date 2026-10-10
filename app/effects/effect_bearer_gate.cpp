#include "effects/effect_bearer_gate.h"

#include "ro/sim/map_mode.h"
#include "ro/sim/world.h"

#include "world/actor_visibility.h"

namespace moonclient {
namespace {

// LE TYPE D'OBJET D'UN PERSONNAGE JOUEUR dans les paquets d'apparition. Le
// client ne bâtit qu'à lui l'acteur dont la porte lit les options ; un joueur
// déguisé, annoncé d'un autre type, se bâtit comme un PNJ.
constexpr std::uint8_t kPlayerObjectType = 0;

}  // namespace

BearerGate BearerGateOf(std::uint32_t bearer_id, const ro::sim::World& world,
                        const ro::sim::PartyRoster* party) {
  if (bearer_id == 0) {
    return BearerGate::kShown;
  }
  const ro::sim::Actor* actor = world.Find(bearer_id);
  if (actor == nullptr || !IsHidden(*actor, world)) {
    return BearerGate::kShown;
  }
  if (bearer_id == world.player_id()) {
    return BearerGate::kExcepted;
  }
  if (actor->kind != ro::sim::ActorKind::kPlayer || actor->object_type != kPlayerObjectType) {
    return BearerGate::kShown;
  }
  if (party != nullptr && ro::sim::PartySeesConcealedEffects(world.map_type()) &&
      party->HasOnlineMember(bearer_id)) {
    return BearerGate::kExcepted;
  }
  return BearerGate::kConcealed;
}

bool IsConcealedBoss(const ro::sim::Actor& actor, const ro::sim::World& world) {
  return actor.boss != ro::sim::BossRank::kNone && IsHidden(actor, world);
}

}  // namespace moonclient

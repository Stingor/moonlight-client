#include "world/actor_visibility.h"

#include "ro/sim/world.h"

namespace moonclient {

bool HasSprite(ro::sim::ActorKind kind) {
  return kind == ro::sim::ActorKind::kPlayer || kind == ro::sim::ActorKind::kNonPlayer ||
         kind == ro::sim::ActorKind::kMonster || kind == ro::sim::ActorKind::kItem;
}

Concealment ConcealmentOf(const ro::sim::Actor& actor, const ro::sim::World& world) {
  // Camouflé ou invisible, l'acteur perd son dessin ENTIER, ombre comprise, et
  // cela vaut pour le joueur lui-même.
  if ((actor.option & (ro::sim::kOptionCloak | ro::sim::kOptionInvisible)) != 0) {
    return Concealment::kNothing;
  }
  if ((actor.option & ro::sim::kOptionHide) == 0) {
    return Concealment::kVisible;
  }
  // Caché, il perd son corps ; les autres perdent aussi son ombre, lui non.
  return actor.id == world.player_id() ? Concealment::kShadowOnly : Concealment::kNothing;
}

bool IsHidden(const ro::sim::Actor& actor, const ro::sim::World& world) {
  return ConcealmentOf(actor, world) != Concealment::kVisible;
}

void KeepConcealedSelf(Concealment concealment, std::uint32_t actor_id,
                       const ro::render::ComposedActor& shadow, const glm::vec3& ground_position,
                       std::vector<ro::render::DrawnActor>& drawn,
                       std::vector<std::uint32_t>& ids) {
  drawn.push_back(ro::render::DrawnActor{
      nullptr, concealment == Concealment::kShadowOnly ? &shadow : nullptr, ground_position});
  ids.push_back(actor_id);
}

bool IsWarp(const ro::sim::Actor& actor) {
  return actor.kind == ro::sim::ActorKind::kNonPlayer && actor.look.job == ro::sim::kPortalJob;
}

const ro::sim::Actor* ClickTakerUnderMouse(const ro::sim::Actor* under_mouse) {
  return under_mouse != nullptr && IsWarp(*under_mouse) ? nullptr : under_mouse;
}

void KeepWarp(std::uint32_t actor_id, const glm::vec3& ground_position,
              std::vector<ro::render::DrawnActor>& drawn, std::vector<std::uint32_t>& ids) {
  ro::render::DrawnActor warp{nullptr, nullptr, ground_position};
  warp.ground_pick_radius = kWarpPickRadius;
  drawn.push_back(warp);
  ids.push_back(actor_id);
}

}  // namespace moonclient

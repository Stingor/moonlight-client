#include "world/actor_visibility.h"

#include "ro/sim/world.h"

namespace moonclient {
namespace {

// Les reculs de désignation, en unités du monde — les marges que le client
// ajoute à la profondeur des rectangles cliquables, par genre.
constexpr float kNonPlayerPickBias = 100.0F;
constexpr float kMonsterPickBias = 200.0F;
// UN OBJET AU SOL CÈDE LE PIXEL À UN MONSTRE, et le prend à tous les autres :
// qui chasse sur un tas de butin vise le monstre qui y marche, et qui ramasse
// trouve l'objet sous ses pieds ou sous son pet. Le client d'origine ne lui
// donne aucun recul, et le butin y détourne le curseur des monstres.
constexpr float kGroundItemPickBias = 250.0F;
// UN FAMILIER CÈDE LE PIXEL À UN MONSTRE, et le prend encore à son maître : qui
// chasse vise le monstre derrière son pet, et qui veut son pet le trouve
// par-dessus lui-même. L'écart de cent avec le monstre dépasse ce qui sépare en
// profondeur deux acteurs voisins à l'écran.
constexpr float kCompanionPickBias = 300.0F;
constexpr float kPlayerPickBias = 400.0F;
// UN MONSTRE QUI AGONISE RECULE AU-DELÀ DE TOUT ce que la caméra peut voir :
// il ne prend le pixel que si personne d'autre ne le tient. Un monstre vivant
// posé sur la case d'un mort reste donc désignable, quel que soit le plus
// proche de l'œil.
constexpr float kCorpsePickBias = 1.0e6F;

}  // namespace

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

void KeepPointPick(std::uint32_t actor_id, const glm::vec3& ground_position, float pick_bias,
                   std::vector<ro::render::DrawnActor>& drawn, std::vector<std::uint32_t>& ids) {
  ro::render::DrawnActor bodiless{nullptr, nullptr, ground_position};
  bodiless.point_pick = true;
  bodiless.pick_bias = pick_bias;
  drawn.push_back(bodiless);
  ids.push_back(actor_id);
}

float PickBiasOf(const ro::sim::Actor& actor) {
  // ⚠ L'AGONIE D'UN MONSTRE SEUL — son cadavre, lui, ne se désigne plus : un
  // joueur mort doit rester désignable, on le vise pour le relever.
  if (actor.kind == ro::sim::ActorKind::kMonster && actor.posture == ro::sim::Posture::kDead) {
    return kCorpsePickBias;
  }
  if (actor.familiar != ro::sim::Familiar::kNone) {
    return kCompanionPickBias;
  }
  switch (actor.kind) {
    case ro::sim::ActorKind::kPlayer:
      return kPlayerPickBias;
    case ro::sim::ActorKind::kMonster:
      return kMonsterPickBias;
    case ro::sim::ActorKind::kNonPlayer:
      return kNonPlayerPickBias;
    case ro::sim::ActorKind::kItem:
      return kGroundItemPickBias;
    default:
      return 0.0F;
  }
}

}  // namespace moonclient

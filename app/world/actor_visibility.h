#pragma once

#include <cstdint>
#include <vector>

#include <glm/vec3.hpp>

#include "ro/render/actor_sprite.h"
#include "ro/render/effect_warp_ring_seed.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/actor.h"
#include "ro/sim/world_fwd.h"

// Ce qui se dessine d'un acteur : a-t-il des sprites, et que laissent voir de
// lui ses options de dissimulation.
namespace moonclient {

// Un personnage ou un objet au sol se dessine par ses sprites ; une zone de
// compétence, non.
[[nodiscard]] bool HasSprite(ro::sim::ActorKind kind);

// Ce qu'on voit encore d'un acteur que ses options cachent.
enum class Concealment : std::uint8_t {
  kVisible,
  // Le corps a disparu, son ombre reste au sol.
  kShadowOnly,
  // Ni corps ni ombre.
  kNothing,
};

// CACHÉ (Hiding), LE CORPS DISPARAÎT POUR TOUS, le joueur caché compris ; son
// ombre ne s'efface que pour les AUTRES — qui se cache voit encore la sienne à
// ses pieds. Camouflé ou invisible, l'acteur disparaît tout entier, pour
// lui-même aussi.
[[nodiscard]] Concealment ConcealmentOf(const ro::sim::Actor& actor, const ro::sim::World& world);

// Vrai pour un acteur dont le CORPS ne se dessine pas : voir `ConcealmentOf`.
[[nodiscard]] bool IsHidden(const ro::sim::Actor& actor, const ro::sim::World& world);

// UN AUTRE ACTEUR DISSIMULÉ, à la place qu'il occupe sur le sol. Il n'a ni corps
// ni rang dans la scène dessinée, donc ni nom, ni jauge, ni clic : seul ce qui
// doit le montrer malgré tout s'y rapporte.
struct ConcealedPlace {
  std::uint32_t actor_id = 0;
  glm::vec3 ground_position{0.0F};
};

// LE JOUEUR DISSIMULÉ GARDE SA PLACE DANS LA SCÈNE, SANS CORPS : un rang sans
// sprite, ajouté à `drawn` et `ids`, où sa jauge et son nom s'accrochent encore
// et que rien ne peut cliquer. Caché, il laisse en plus son ombre au sol, portée
// par ce rang comme celle d'un corps : l'eau la couvre de même.
void KeepConcealedSelf(Concealment concealment, std::uint32_t actor_id,
                       const ro::render::ComposedActor& shadow, const glm::vec3& ground_position,
                       std::vector<ro::render::DrawnActor>& drawn, std::vector<std::uint32_t>& ids);

// UN WARP : un PNJ de la classe des portails. Un joueur ou un monstre qui
// porterait ce numéro n'en est pas un.
[[nodiscard]] bool IsWarp(const ro::sim::Actor& actor);

// L'ACTEUR QUI PREND LE CLIC GAUCHE, parmi celui que la souris désigne.
//
// 🔴 UN WARP NE LE PREND PAS : il le laisse DESCENDRE AU SOL, et c'est la case
// sous la souris — pas celle du warp — que le personnage va chercher, retenue
// par son geste et soumise au plancher comme tout clic au sol. Le survol ne lui
// prête que sa forme de curseur.
[[nodiscard]] const ro::sim::Actor* ClickTakerUnderMouse(const ro::sim::Actor* under_mouse);

// LE RAYON PAR LEQUEL UN WARP SE DÉSIGNE : celui dont repart le plus grand de
// ses anneaux. Le joueur vise ce qu'il voit.
inline constexpr float kWarpPickRadius = ro::render::kWarpRingRewindRadius;

// UN WARP PREND SA PLACE DANS LA SCÈNE SANS RIEN Y PEINDRE : un rang sans corps,
// ajouté à `drawn` et `ids`, qui se désigne par le disque de ses anneaux (voir
// `ro::render::PickActor`). C'est ce qui le rend survolable et cliquable ; ses
// anneaux, eux, sont un effet, dessiné ailleurs.
//
// ⚠ À APPELER AVANT DE CHERCHER UN CORPS : un warp n'en a pas, et l'acteur sans
// corps sort de la scène.
void KeepWarp(std::uint32_t actor_id, const glm::vec3& ground_position,
              std::vector<ro::render::DrawnActor>& drawn, std::vector<std::uint32_t>& ids);

// UN ACTEUR SANS CORPS QUI SE CLIQUE prend sa place dans la scène sans rien y
// peindre : un rang ajouté à `drawn` et `ids`, désigné par le point de ses pieds
// (voir `ro::render::DrawnActor::point_pick`), avec ce recul. Ni nom, ni jauge,
// ni ombre ne s'y accrochent.
void KeepPointPick(std::uint32_t actor_id, const glm::vec3& ground_position, float pick_bias,
                   std::vector<ro::render::DrawnActor>& drawn, std::vector<std::uint32_t>& ids);

}  // namespace moonclient

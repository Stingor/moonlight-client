// LES FAMILLES DONT LA PEINTURE FAIT NAÎTRE UN AUTRE EFFET, et l'oubli de la
// troupe.
//
// Trois familles de nœuds demandent un effet à chaque peinture : la lueur en
// arc du sous-type 109, l'émetteur en spirale du 151 et le semeur qui marche du
// 120. Elles naissent ici avec ce qu'il leur faut pour le demander — l'acteur
// désigné, la cible —, et `Drawn` recueille leurs demandes. Voir
// `ro/render/effect_spawn_request.h`.
//
// ⚠ `Forget` VIT ICI parce que l'en-tête de la troupe est au plafond de huit
// cents lignes, et qu'il doit oublier ces demandes avec le reste.

#include <cstdint>
#include <optional>
#include <string>

#include "ro/render/effect_arcing_light_seed.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_spiral_emitter_seed.h"
#include "ro/render/effect_walking_sower_seed.h"

#include "effects/effect_node_view.h"

namespace moonclient {

void EffectNodesInPlay::Forget() {
  pool_.Clear();
  steps_.Forget();
  clocks_.clear();
  screen_nodes_.clear();
  screen_quad_lots_.clear();
  screen_quads_.clear();
  composed_.clear();
  placements_.clear();
  drawn_.clear();
  sprite_batches_.clear();
  ring_quads_.clear();
  world_quads_.clear();
  ground_cell_lots_.clear();
  ground_cells_.clear();
  // Les bruits de semis et les effets demandés pas encore emportés
  // appartenaient à la troupe oubliée.
  sowing_sounds_.clear();
  spawn_requests_.clear();
  // ⚠ LE BATTEMENT AUSSI, et pour la raison qui l'a fait naître : le premier
  // passage ne rattrape rien. Le garder ferait vieillir la troupe suivante de
  // tout le temps écoulé depuis l'essai précédent.
  beat_ms_ = 0;
  silent_frames_ = 0;
  sans_texture_ = 0;
  sans_quad_ = 0;
}

bool EffectNodesInPlay::PlayArcingLights(std::uint32_t effect_id, glm::vec3 ground_position,
                                         std::uint32_t designated_actor_id) {
  // UN NŒUD PAR SITE, au premier tic seulement : aux suivants le client ne fait
  // plus que recaler l'instance sur son porteur.
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectArcingLightPattern& pattern :
       ro::render::EffectArcingLightPatternsFor(effect_id)) {
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowArcingLightNode(pattern, ground_position,
                                             ro::render::EffectNodeLifeFor(effect_id), generator,
                                             designated_actor_id));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySpiralEmitters(std::uint32_t effect_id, glm::vec3 ground_position,
                                           std::uint32_t designated_actor_id) {
  // UN NŒUD PAR APPEL, au premier tic seulement — deux pour 931.
  //
  // ⚠ L'IMAGE N'EST PAS DEMANDÉE : le bâtisseur la charge et rien ne la peint.
  // Une installation où elle manque ne perd donc rien de ce que le nœud fait.
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectSpiralEmitterPattern& pattern :
       ro::render::EffectSpiralEmitterPatternsFor(effect_id)) {
    pool_.Add(ro::render::SowSpiralEmitterNode(pattern, ground_position,
                                               ro::render::EffectNodeLifeFor(effect_id),
                                               designated_actor_id, generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayWalkingSowers(std::uint32_t effect_id, glm::vec3 ground_position,
                                          const std::optional<glm::vec3>& target_position) {
  // UN NŒUD PAR SITE, au premier tic seulement. Sans cible, il marche au cap
  // zéro — voir `WalkingSowerClientHeading`.
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectWalkingSowerPattern& pattern :
       ro::render::EffectWalkingSowerPatternsFor(effect_id)) {
    pool_.Add(ro::render::SowWalkingSowerNode(pattern, ground_position, target_position,
                                              ro::render::EffectNodeLifeFor(effect_id), generator));
    sown = true;
  }
  return sown;
}

}  // namespace moonclient

// LES SEMIS EN VAGUES : des familles dont le bâtisseur, rappelé à chaque tic,
// repose un nœud à des âges fixes de l'effet.
//
// Chacune suit un horaire constant, le même pour tous les effets de la
// famille ; c'est ce qui les distingue des semis horlogés par patron
// d'`effect_node_sow_clocked.cpp`. L'horloge les rappelle, et `LastSowTick`
// borne leur dernier semis.

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ro/render/effect_expanding_ring_seed.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_palette_billboard_seed.h"
#include "ro/render/effect_rain_particle_seed.h"
#include "ro/render/effect_swirl_trail_seed.h"
#include "ro/render/effect_tetra_vortex_seed.h"

#include "effects/effect_node_view.h"

namespace moonclient {

bool EffectNodesInPlay::PlayPaletteBillboards(std::uint32_t effect_id, glm::vec3 ground_position,
                                              std::int32_t tick) {
  // ⚠ DEUX VAGUES, AUX TICS ZÉRO ET DIX : le bâtisseur est rappelé à chaque tic
  // et pose le même nœud à ces deux âges de l'effet. Sans la seconde, l'éclat
  // de gel n'a qu'une spirale au lieu de deux.
  if (!ro::render::EffectNodeScheduleSowsAt(ro::render::kPaletteBillboardSchedule, tick)) {
    return false;
  }
  // ⚠ UNE INSTANCE MORTE NE SÈME PLUS : un effet plus court que dix tics n'a
  // pas de seconde vague.
  const std::int32_t life = ro::render::EffectNodeLifeFor(effect_id);
  if (tick >= life) {
    return false;
  }
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectPaletteBillboardPattern& pattern :
       ro::render::EffectPaletteBillboardPatternsFor(effect_id)) {
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    // LE NŒUD RECOPIE LA VIE ENTIÈRE DE L'INSTANCE, qu'il naisse au tic zéro ou
    // au dixième ; c'est la mort de l'instance qui l'emporte — voir
    // `PooledEffectNode::instance_beats_left`.
    pool_.Add(ro::render::SowEffectPaletteBillboardNode(pattern, ground_position, life, generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayRainParticles(std::uint32_t effect_id, glm::vec3 ground_position,
                                          std::int32_t tick) {
  // ⚠ UN ÉMETTEUR : une bouffée tous les dix tics tant que l'effet vit et a
  // moins de mille tics. C'est ce retour régulier qui fait la pluie.
  if (!ro::render::EffectNodeScheduleSowsAt(ro::render::kRainParticleSchedule, tick)) {
    return false;
  }
  const std::int32_t life = ro::render::EffectNodeLifeFor(effect_id);
  if (tick >= life) {
    return false;
  }
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectRainParticlePattern& pattern :
       ro::render::EffectRainParticlePatternsFor(effect_id)) {
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    // La vie entière de l'instance, comme l'éclat de gel.
    pool_.Add(ro::render::SowEffectRainParticleNode(pattern, ground_position, life, generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlaySwirlTrails(std::uint32_t effect_id, glm::vec3 ground_position,
                                        std::int32_t tick) {
  // ⚠ CINQ SEMIS, AUX TICS ZÉRO, TROIS, SIX, NEUF ET DOUZE, chacun depuis la
  // place que le porteur occupe à ce tic : c'est pourquoi l'horloge les
  // rappelle, au lieu de les semer d'avance retardés.
  if (!ro::render::EffectNodeScheduleSowsAt(ro::render::kSwirlTrailSchedule, tick)) {
    return false;
  }
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectSwirlTrailPattern& pattern :
       ro::render::EffectSwirlTrailPatternsFor(effect_id)) {
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    // ⚠ LE NŒUD MEURT AVEC SON INSTANCE : le bâtisseur recopie la vie de
    // l'instance, et un nœud né au tic douze n'a plus que ce qu'il en reste.
    pool_.Add(ro::render::SowEffectSwirlTrailNode(
        pattern, ground_position, ro::render::EffectNodeLifeFor(effect_id) - tick, generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayExpandingRings(std::uint32_t effect_id, glm::vec3 ground_position,
                                           std::int32_t tick) {
  // ⚠ QUATRE SEMIS, UN PAR TIC DU ZÉRO AU TROISIÈME, chacun depuis la
  // place que le porteur occupe à ce tic, et chacun deux unités plus large que le
  // précédent : l'âge entre dans le rayon. Semer les quatre d'avance les
  // poserait tous au même point et à la même taille.
  if (!ro::render::EffectNodeScheduleSowsAt(ro::render::kExpandingRingSchedule, tick)) {
    return false;
  }
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (const ro::render::EffectExpandingRingPattern& pattern :
       ro::render::EffectExpandingRingPatternsFor(effect_id)) {
    if (TextureFor(std::string(pattern.texture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowEffectExpandingRingNode(
        pattern, ground_position, tick, ro::render::EffectNodeLifeFor(effect_id) - tick,
        generator));
    sown = true;
  }
  return sown;
}

bool EffectNodesInPlay::PlayTetraVortices(std::uint32_t effect_id, glm::vec3 ground_position,
                                          const std::optional<glm::vec3>& target_position,
                                          std::int32_t tick) {
  if (ro::render::EffectTetraVortexPatternsFor(effect_id).empty() ||
      !ro::render::EffectNodeScheduleSowsAt(ro::render::kTetraVortexSchedule, tick)) {
    return false;
  }
  // ⚠ LES HUIT IMAGES SONT EXIGÉES : le mouvement les parcourt toutes, et une
  // seule illisible ferait clignoter le tourbillon sans que rien ne le dise.
  for (const std::string_view image : ro::render::kTetraVortexFrames) {
    if (TextureFor(std::string(image)).empty()) {
      return false;
    }
  }
  // ⚠ LE POINT VISÉ EST CELUI DE LA NAISSANCE, que l'horloge garde : le client
  // ne réécrit le point visé qu'une fois, par la commande 14 qui suit la pose.
  std::optional<ro::render::PooledEffectNode> node = ro::render::SowTetraVortexNode(
      effect_id, ground_position, target_position, ro::render::EffectNodeLifeFor(effect_id));
  if (!node.has_value()) {
    return false;
  }
  pool_.Add(std::move(*node));
  return true;
}

}  // namespace moonclient

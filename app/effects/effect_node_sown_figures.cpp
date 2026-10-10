// LES FIGURES SEMÉES DE QUATRE SOUS-TYPES : les monstres du 153, le carré qui
// tourne à plat du 143, la note qui flotte du 156 et le voile teinté du 172.
//
// Une seule méthode pour les quatre, et c'est ce qu'elles ont en commun : un
// bâtisseur qui ne lit de son cas qu'un octet ou deux, et qui sème son nœud
// sur la place de l'instance. Les trois carrés naissent au premier tic ; les
// monstres, eux, naissent un par tic tant que leur style sème.

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

#include "ro/render/effect_bobbing_note.h"
#include "ro/render/effect_colored_veil.h"
#include "ro/render/effect_floor_spinner.h"
#include "ro/render/effect_glow_sprite_seed.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pool.h"

#include "effects/effect_node_view.h"

namespace moonclient {

bool EffectNodesInPlay::PlaySownFigures(std::uint32_t effect_id, glm::vec3 ground_position,
                                        std::int32_t effect_tick, float instance_scale,
                                        float instance_second) {
  bool sown = false;
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;

  // LES MONSTRES : un par tic d'âge tant que leur style sème. Le couple
  // .spr/.act se charge par le chemin commun des sprites de la troupe.
  for (const ro::render::EffectGlowSpriteNodePattern& pattern :
       ro::render::EffectGlowSpriteNodePatternsFor(effect_id)) {
    const ro::render::EffectGlowSpriteStyleSeed* seed =
        ro::render::EffectGlowSpriteStyleSeedFor(pattern.style);
    if (seed == nullptr || effect_tick >= ro::render::EffectGlowSpriteSowingTicks(pattern.style)) {
      continue;
    }
    const ro::render::LoadedActorSprite* charge = LoadedSprite(std::string(seed->sprite_base));
    if (charge == nullptr) {
      continue;
    }
    ro::render::PooledEffectNode node = ro::render::SowEffectGlowSpriteNode(
        pattern, ground_position, effect_tick, ro::render::EffectNodeLifeFor(effect_id),
        pool_.world(), generator);
    // 🔴 L'ANIMATION BOUCLE SUR LES IMAGES DE SON ACTION, que seul le `.act` dit :
    // laissé à un, le monstre resterait figé sur sa première image.
    const auto action = static_cast<std::size_t>(std::max(node.node.action, 0));
    const auto& actions = charge->animation.actions();
    node.node.texture_count =
        action < actions.size() ? std::max<std::size_t>(actions[action].frames.size(), 1) : 1;
    pool_.Add(std::move(node));
    sown = true;
  }
  if (effect_tick != 0) {
    return sown;
  }

  // LE CARRÉ QUI TOURNE À PLAT : un nœud par appel du cas. L'eau lit le rayon
  // de son objet de carte dans l'échelle d'instance, et son cap dans le second
  // paramètre du même objet.
  for (const ro::render::EffectFloorSpinnerPattern& pattern :
       ro::render::EffectFloorSpinnerPatternsFor(effect_id)) {
    const std::vector<std::string> images = ro::render::EffectFloorSpinnerTextures(pattern.byte);
    if (images.empty() || TextureFor(images.front()).empty()) {
      continue;
    }
    for (const std::string& image : images) {
      (void)TextureFor(image);
    }
    pool_.Add(ro::render::SowFloorSpinnerNode(pattern, ground_position, instance_scale,
                                              instance_second, generator));
    sown = true;
  }

  // LA NOTE QUI FLOTTE, dans la variante que `/mineffect` demande.
  for (const ro::render::EffectBobbingNotePattern& pattern :
       ro::render::EffectBobbingNotePatternsFor(effect_id)) {
    const std::string image(ro::render::EffectBobbingNoteTexture(pattern, minimal_effects_));
    if (TextureFor(image).empty()) {
      continue;
    }
    ro::render::PooledEffectNode node =
        ro::render::SowBobbingNoteNode(pattern, ground_position, minimal_effects_, generator);
    // LA GRAINE DES COINS QUI VIBRENT, propre à chaque note.
    node.node.wander_seed = generator;
    pool_.Add(std::move(node));
    sown = true;
  }

  // LE VOILE TEINTÉ.
  for (const ro::render::EffectColoredVeilPattern& pattern :
       ro::render::EffectColoredVeilPatternsFor(effect_id)) {
    if (TextureFor(std::string(ro::render::kColoredVeilTexture)).empty()) {
      continue;
    }
    pool_.Add(ro::render::SowColoredVeilNode(pattern, ground_position));
    sown = true;
  }
  return sown;
}

}  // namespace moonclient

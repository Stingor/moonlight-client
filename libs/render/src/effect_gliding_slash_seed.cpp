#include "ro/render/effect_gliding_slash_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_gliding_slash.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"

namespace ro::render {
namespace {

// LA CASE QUI PORTE LA VITESSE, et les deux marques.
constexpr std::size_t kSlotOfSpeed = 1;
constexpr std::size_t kSlotOfPaintMark = 1;

}  // namespace

std::span<const EffectGlidingSlashPattern> EffectGlidingSlashPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectGlidingSlashPatternTable(), effect_id,
                           &EffectGlidingSlashPattern::effect_id);
}

std::vector<std::string_view> EffectGlidingSlashTexturesOf(
    const EffectGlidingSlashPattern& pattern) {
  // ⚠ QUATRE IMAGES POUR UN SEUL OCTET, une pour tous les autres. C'est la
  // même comparaison que celle qui fait défiler l'index côté figure : les deux
  // doivent s'accorder, sans quoi l'index sortirait du jeu.
  if (pattern.builder == EffectGlidingSlashBuilder::kSonicWave &&
      pattern.mark == kGlidingSlashAnimatedMark) {
    return {kGlidingSlashWaveTextures.begin(), kGlidingSlashWaveTextures.end()};
  }
  return {kGlidingSlashPlainTexture};
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGlidingSlashTracks(
    const EffectGlidingSlashPattern& pattern, glm::vec3 anchor, std::uint32_t& generator) {
  const bool aimed = pattern.builder == EffectGlidingSlashBuilder::kSonicWave;
  // ⚠ LE SECOND BÂTISSEUR TIRE UN RETARD DE PLUS, une fois pour les quatre
  // pistes : deux taillades lancées ensemble ne partent pas au même tic.
  const std::int32_t shared_delay = aimed ? 0 : DrawBelow(generator, kGlidingSlashExtraDelaySpread);

  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.alpha = 0;
    // Un tic d'écart entre les quatre : elles se suivent.
    track.ticks = static_cast<std::int32_t>(rang) - kGlidingSlashDelayTicks - shared_delay;
    track.radius_at_anchor = kGlidingSlashRadius;
    // ⚠ NOTRE REPÈRE MONTE, CELUI DU CLIENT DESCEND : là où il retranche dix,
    // nous ajoutons, et le tirage qui l'écarte change de signe avec lui.
    track.anchor = {anchor.x,
                    anchor.y + kGlidingSlashBirthHeight -
                        static_cast<float>(DrawBelow(generator, kGlidingSlashHeightSpread)),
                    anchor.z};
    // ⚠ LE PREMIER MET L'AZIMUT À ZÉRO ET LE MOUVEMENT LE REMPLACE au premier
    // tic, en visant la cible. Le second le reçoit de son site et le garde.
    track.azimuth_degrees = aimed ? 0 : pattern.azimuth_degrees;
    track.value[kSlotOfSpeed] = kGlidingSlashSpeed;
    track.mark[0] = 0;
    track.mark[kSlotOfPaintMark] = aimed ? pattern.mark : 0;
  }
  return tracks;
}

PooledEffectNode SowEffectGlidingSlashNode(const EffectGlidingSlashPattern& pattern,
                                           glm::vec3 anchor, std::int32_t effect_life_ticks,
                                           std::uint32_t& generator) {
  // ⚠ DEUX FORMES POUR UNE SEULE FIGURE : rien dans l'état du nœud ne dit
  // lequel des deux sous-types vise sa cible, et le deviner ferait viser une
  // taillade sur deux à l'envers. Le patron le dit, et la forme le porte.
  PooledEffectNode pooled = PooledNodeAt(pattern.builder == EffectGlidingSlashBuilder::kSonicWave
                                             ? PooledNodeShape::kAimedGlidingSlash
                                             : PooledNodeShape::kGlidingSlash,
                                         anchor);
  pooled.node.life_ticks = effect_life_ticks;
  for (const std::string_view image : EffectGlidingSlashTexturesOf(pattern)) {
    pooled.resource_paths.emplace_back(image);
  }
  pooled.tracks = SeedEffectGlidingSlashTracks(pattern, anchor, generator);
  return pooled;
}

}  // namespace ro::render

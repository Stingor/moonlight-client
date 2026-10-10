#include "ro/render/effect_sphere_shell_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LA SEULE PISTE QUE LE BÂTISSEUR ALLUME.
constexpr std::size_t kLivingTrack = 0;
// La case que la coquille extérieure lit comme son enflure, semée à zéro.
constexpr std::size_t kSlotOfOuterGrowth = 0;

}  // namespace

std::span<const EffectSphereShellPattern> EffectSphereShellPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectSphereShellPattern> table = EffectSphereShellPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectSphereShellPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectSphereShellPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSphereShellTracks(
    const EffectSphereShellPattern& pattern) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ LES TROIS AUTRES RESTENT ÉTEINTES, et c'est le bâtisseur qui le veut :
  // il les couche une à une avant d'allumer la première.
  EffectTrack& track = tracks[kLivingTrack];
  track.active = true;
  track.ticks = 0;
  // ⚠ IL NAÎT INVISIBLE : l'alpha part de zéro et c'est le mouvement qui le
  // monte, de vingt par tic pendant cinq tics. Le semer plein ferait paraître
  // le bouclier d'un coup.
  track.alpha = 0;
  track.lift_key = pattern.lift_key;
  track.radius_at_anchor = kSphereShellRadiusAtAnchor;
  track.value[kSlotOfOuterGrowth] = 0.0F;
  track.mark[0] = pattern.mark;
  return tracks;
}

PooledEffectNode SowEffectSphereShellNode(const EffectSphereShellPattern& pattern, glm::vec3 anchor,
                                          std::int32_t effect_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kSphereShell;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  // ⚠ L'ORDRE DES DEUX IMAGES EST CELUI DU BÂTISSEUR, et la figure s'en sert
  // comme d'un index : celle du site d'abord, `guardK2.tga` ensuite. Les
  // échanger peindrait les deux coquilles intérieures avec l'image de la
  // troisième.
  pooled.resource_paths = {std::string(pattern.texture), std::string(pattern.second_texture)};
  pooled.trail_variant = kSphereShellNodeScale;
  pooled.tracks = SeedEffectSphereShellTracks(pattern);
  return pooled;
}

}  // namespace ro::render

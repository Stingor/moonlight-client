#include "ro/render/effect_grid_plane_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_grid_plane.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LE CÔTÉ D'UN CARREAU, que le bâtisseur écrit en dur. C'est aussi le pas des
// deux boucles de la figure, et c'est ce qui rend la nappe jointive.
constexpr float kCellSide = 30.0F;

}  // namespace

std::span<const EffectGridPlanePattern> EffectGridPlanePatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectGridPlanePattern> table = EffectGridPlanePatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectGridPlanePattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectGridPlanePattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGridPlaneTracks(
    const EffectGridPlanePattern& pattern) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ UNE SEULE PISTE VIVE : le bâtisseur éteint les quatre puis rallume la
  // première. Les allumer toutes empilerait quatre nappes au même endroit.
  EffectTrack& track = tracks.front();
  track.active = true;
  track.ticks = 0;
  track.radius_at_anchor = kCellSide;
  // L'ORIGINE PART À ZÉRO : la nappe est centrée sur son ancre.
  track.lift_key = 0.0F;
  // ⚠ L'ALPHA PART À ZÉRO : rien ne se voit tant que le mouvement n'a pas tourné.
  track.alpha = 0;
  // ⚠ L'OCTET DU SITE EST LA MARQUE, et DEUX lecteurs l'y prennent : la figure
  // pour sa teinte, le mouvement pour le rythme de son alpha.
  track.mark[kMarkOfGridPlaneTint] = pattern.builder_argument;
  return tracks;
}

PooledEffectNode SowEffectGridPlaneNode(const EffectGridPlanePattern& pattern, glm::vec3 anchor,
                                        std::int32_t effect_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kGridPlane;
  // ⚠ L'ANCRE DESCEND DE NEUF AVANT LE BÂTI, et c'est l'effet ENTIER qui descend
  // chez le client. Nous n'en posons qu'un nœud, si bien que la descente se voit
  // ici ; le jour où un effet du sous-type 72 en pose deux, il faudra la remonter
  // d'un cran.
  const glm::vec3 pose{anchor.x, anchor.y + kGridPlaneAnchorDrop, anchor.z};
  pooled.ground_position = pose;
  pooled.owner_position = pose;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.node.render_word = kGridPlaneRenderWord;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectGridPlaneTracks(pattern);
  return pooled;
}

}  // namespace ro::render

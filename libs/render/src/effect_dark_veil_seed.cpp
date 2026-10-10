#include "ro/render/effect_dark_veil_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LE RAYON DU CAS ZÉRO, qui ne suit pas la règle des dix autres. Le client
// l'écrit comme une constante à part, et c'est le seul voile de cette taille.
constexpr float kLoneRadius = 65.0F;

// LA RÈGLE DES DIX AUTRES : un rayon qui RÉTRÉCIT quand l'octet grandit. Cent
// trente pour un, quarante pour dix.
constexpr float kRadiusAtFirstStep = 140.0F;
constexpr float kRadiusPerStep = 10.0F;

// L'ALPHA DE DÉPART, et il vaut zéro : le voile vient, il ne tombe pas.
constexpr std::int16_t kVeilStartsInvisible = 0;

// La piste que le bâtisseur allume. Il éteint les quatre, puis rallume celle-ci.
constexpr std::size_t kLoneTrack = 0;

}  // namespace

std::span<const EffectDarkVeilPattern> EffectDarkVeilPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectDarkVeilPattern> table = EffectDarkVeilPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectDarkVeilPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectDarkVeilPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

EffectDarkVeilShape EffectDarkVeilShapeFor(std::uint8_t builder_argument) {
  EffectDarkVeilShape rendered;
  if (builder_argument == 0) {
    rendered.radius_at_anchor = kLoneRadius;
    rendered.shade_mark = 0;
    return rendered;
  }
  rendered.radius_at_anchor =
      kRadiusAtFirstStep - kRadiusPerStep * static_cast<float>(builder_argument);
  rendered.shade_mark = kDarkVeilPaler;
  return rendered;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectDarkVeilTracks(
    const EffectDarkVeilPattern& pattern) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  const EffectDarkVeilShape forme = EffectDarkVeilShapeFor(pattern.builder_argument);
  EffectTrack& track = tracks[kLoneTrack];
  track.active = true;
  track.ticks = 0;
  track.alpha = kVeilStartsInvisible;
  track.radius_at_anchor = forme.radius_at_anchor;
  track.mark[kMarkOfVeilShade] = forme.shade_mark;
  return tracks;
}

PooledEffectNode SowEffectDarkVeilNode(const EffectDarkVeilPattern& pattern, glm::vec3 anchor,
                                       std::int32_t effect_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kDarkVeil;
  // ⚠ LE VOILE SE POSE SUR SON PORTEUR, sans décalage. Le bâtisseur du client
  // élève bien son instance au premier tic, mais il la ramène sur le porteur
  // dès le tic suivant, et le nœud recopie cette place à chaque tic : le
  // décalage ne dure qu'une image. Le garder le faisait durer toute la vie
  // de l'effet — et, écrit dans le mauvais sens, sous le sol.
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  // LE VOILE SE PEINT PAR-DESSUS TOUT : ni le décor ni un acteur ne le cachent,
  // et son seau impose le mélange ordinaire.
  pooled.node.render_word = kEffectRenderWordOfConstructor | kEffectRenderWordOverEverything;
  pooled.resource_paths.reserve(pattern.textures.size());
  for (const std::string_view image : pattern.textures) {
    pooled.resource_paths.emplace_back(image);
  }
  pooled.tracks = SeedEffectDarkVeilTracks(pattern);
  return pooled;
}

}  // namespace ro::render

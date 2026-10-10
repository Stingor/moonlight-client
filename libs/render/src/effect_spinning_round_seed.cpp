#include "ro/render/effect_spinning_round_seed.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_spinning_round.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

[[nodiscard]] float DrawAngleDegrees(std::uint32_t& rng_state) {
  return static_cast<float>(NextEffectNodeDraw(rng_state) %
                            static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
}

}  // namespace

std::span<const EffectSpinningRoundPattern> EffectSpinningRoundPatternsFor(
    std::uint32_t effect_id) {
  const std::span<const EffectSpinningRoundPattern> table = EffectSpinningRoundPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectSpinningRoundPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectSpinningRoundPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpinningRoundTracks(
    const EffectSpinningRoundPattern& pattern, std::uint32_t& generator) {
  // ⚠ LE REPÈRE DU CLIENT DESCEND, LE NÔTRE AUSSI POUR CE CHAMP : le décalage
  // d'une particule se range dans le repère du client, et la figure le retourne
  // au dernier moment. Voir `ComposeSpinningRoundQuads`.
  const float height = -kSpinningRoundHeightPerByte * static_cast<float>(pattern.height_byte);
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    track.active = true;
    track.lift_key = kSpinningRoundUnusedKey;
    track.radius_at_anchor = kSpinningRoundSquareRadius;
    // ⚠ LES QUATRE PREMIERS TIRAGES SONT LES DEUX PAIRES DE TREMBLEMENT, et ils
    // viennent AVANT les quatre angles de ronde. Les intervertir donnerait une
    // autre ronde à graine égale.
    for (std::size_t rang = 0; rang < kSpinningRoundSlotOfFirstSpin; ++rang) {
      track.value[rang] = DrawAngleDegrees(generator);
    }
    // ⚠ ET LE CLIENT INTERCALE : il pose le décalage d'une particule, puis tire
    // son angle de ronde, et recommence. L'ordre des tirages est le même, mais
    // le dire ici garde la transcription lisible en regard du bâtisseur.
    for (std::size_t particle = 0; particle < kSpinningRoundParticles; ++particle) {
      SetEffectSpinningRoundOffset(track, particle, {0.0F, height, 0.0F});
      track.value[kSpinningRoundSlotOfFirstSpin + particle] = DrawAngleDegrees(generator);
    }
    track.mark[0] = 1;
    track.mark[kSpinningRoundMarkOfColor] = pattern.green ? kSpinningRoundGreenMark : 0;
  }
  return tracks;
}

PooledEffectNode SowEffectSpinningRoundNode(const EffectSpinningRoundPattern& pattern,
                                            glm::vec3 anchor, std::int32_t effect_life_ticks,
                                            std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kSpinningRound;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectSpinningRoundTracks(pattern, generator);
  return pooled;
}

}  // namespace ro::render

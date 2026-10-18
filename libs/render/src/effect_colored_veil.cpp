#include "ro/render/effect_colored_veil.h"

#include <algorithm>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

constexpr std::size_t kLeadingTrack = 0;
constexpr std::int16_t kOrdinaryBlend = 1;

[[nodiscard]] std::uint8_t Channel(float value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0.0F, kEffectNodeChannelMax));
}

}  // namespace

std::span<const EffectColoredVeilPattern> EffectColoredVeilPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectColoredVeilPatternTable(), effect_id,
                           &EffectColoredVeilPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedColoredVeilTracks(std::uint8_t byte) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& leader = tracks[kLeadingTrack];
  leader.active = true;
  leader.ticks = 0;
  leader.lift_key = 0.0F;
  leader.alpha = 0;
  leader.mark[kColoredVeilMarkOfRhythm] = byte;
  return tracks;
}

PooledEffectNode SowColoredVeilNode(const EffectColoredVeilPattern& pattern,
                                    glm::vec3 owner_position) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kColoredVeil, owner_position);
  pooled.node.life_ticks = std::max(pattern.life_ticks, 1);
  pooled.node.render_word = kColoredVeilRenderWord;
  pooled.node.frame_hold_ticks = 0;
  pooled.node.color = {static_cast<float>(pattern.color[0]), static_cast<float>(pattern.color[1]),
                       static_cast<float>(pattern.color[2])};
  pooled.resource_paths = {std::string(kColoredVeilTexture)};
  pooled.tracks = SeedColoredVeilTracks(pattern.byte);
  return pooled;
}

std::optional<EffectWorldQuad> ComposeColoredVeilQuad(const EffectTrack& track,
                                                      std::int16_t turn_degrees,
                                                      std::int16_t tilt_degrees,
                                                      const std::array<float, 3>& color,
                                                      glm::vec3 node_origin) {
  if (!track.active || track.alpha <= 0) {
    return std::nullopt;
  }
  // LE CARRÉ À PLAT, dans l'ordre des coins du client, puis redressé.
  std::array<glm::vec3, 4> corners = {{
      {-kColoredVeilHalfSize, kColoredVeilHeightDown, -kColoredVeilHalfSize},
      {kColoredVeilHalfSize, kColoredVeilHeightDown, -kColoredVeilHalfSize},
      {kColoredVeilHalfSize, kColoredVeilHeightDown, kColoredVeilHalfSize},
      {-kColoredVeilHalfSize, kColoredVeilHeightDown, kColoredVeilHalfSize},
  }};
  RotateEffectQuadCorners(corners, turn_degrees, tilt_degrees);

  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    // ⚠ LE CARRÉ EST DANS LE REPÈRE DU CLIENT, tourné par les angles de SA
    // caméra : il revient au nôtre par `S` entier.
    request.corners[corner] = EffectPointFromClient(corners[corner]);
  }
  request.absolute = false;
  request.alpha =
      static_cast<std::uint8_t>(std::clamp<std::int16_t>(track.alpha, 0, kEffectTrackAlphaMax));
  request.color = {Channel(color[0]), Channel(color[1]), Channel(color[2])};
  request.blend_mode = kOrdinaryBlend;
  request.texture = 0;
  return ComposeEffectParticleQuad(request, node_origin);
}

}  // namespace ro::render

#include "ro/render/effect_bobbing_note.h"

#include <algorithm>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_quad_rotation.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

constexpr std::size_t kLeadingTrack = 0;
constexpr std::size_t kTrackThatTakesTheTurn = 0;
constexpr std::size_t kTrackThatTakesTheTilt = 1;

constexpr std::int16_t kAddedBlend = 0;
constexpr std::int16_t kOrdinaryBlend = 1;

// L'ALPHA DES COPIES : la moitié entière de celui de la note.
constexpr std::int16_t kHaloAlphaDivisor = 2;

// LES QUATRE ÉCARTS DU HALO, dans l'ordre où le client les peint : vers les x
// croissants, décroissants, puis vers les profondeurs croissantes, décroissantes
// — dans SON repère, d'où ils passent par `S`.
constexpr std::array<glm::vec3, 4> kHaloOffsets = {{
    {kBobbingNoteHaloOffset, 0.0F, 0.0F},
    {-kBobbingNoteHaloOffset, 0.0F, 0.0F},
    {0.0F, 0.0F, kBobbingNoteHaloOffset},
    {0.0F, 0.0F, -kBobbingNoteHaloOffset},
}};

constexpr std::array<std::uint8_t, 3> kWhite = {255, 255, 255};

// LE COIN SUIVANT : quatre-vingt-cinq degrés plus un tirage, ramené dans le tour.
[[nodiscard]] std::int32_t NextCorner(std::int32_t previous, std::uint32_t& generator) {
  const auto draw =
      static_cast<std::int32_t>(NextEffectNodeDraw(generator) % kBobbingNoteCornerDraws);
  return (previous + draw + kBobbingNoteCornerStepDegrees) % kEffectTrackFullTurnDegrees;
}

// UN CARRÉ DE LA NOTE, ses coins déjà posés dans le monde.
[[nodiscard]] EffectWorldQuad NoteQuad(const std::array<glm::vec3, 4>& corners, glm::vec3 offset,
                                       std::int16_t alpha, std::array<std::uint8_t, 3> color,
                                       std::int16_t blend_mode, std::size_t texture) {
  EffectParticleQuadRequest request;
  for (std::size_t corner = 0; corner < corners.size(); ++corner) {
    request.corners[corner] = corners[corner] + offset;
  }
  request.absolute = true;
  request.alpha =
      static_cast<std::uint8_t>(std::clamp<std::int16_t>(alpha, 0, kEffectTrackAlphaMax));
  request.color = color;
  request.blend_mode = blend_mode;
  request.texture = texture;
  return ComposeEffectParticleQuad(request, glm::vec3{0.0F, 0.0F, 0.0F});
}

}  // namespace

std::span<const EffectBobbingNotePattern> EffectBobbingNotePatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectBobbingNotePatternTable(), effect_id,
                           &EffectBobbingNotePattern::effect_id);
}

std::string_view EffectBobbingNoteTexture(const EffectBobbingNotePattern& pattern,
                                          bool minimal_effects) {
  return minimal_effects && !pattern.minimal_texture.empty() ? pattern.minimal_texture
                                                             : pattern.texture;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedBobbingNoteTracks(std::uint8_t byte,
                                                                    glm::vec3 owner_position,
                                                                    std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> node_tracks;
  EffectTrack& leader = node_tracks[kLeadingTrack];
  leader.active = true;
  leader.ticks = 0;
  leader.radius_at_anchor = kBobbingNoteRadius;
  leader.value[kBobbingNoteSlotOfShiver] = 0.0F;
  leader.alpha = kBobbingNoteAlpha;
  // ⚠ L'ANCRE NAÎT SANS BALANCEMENT : le premier tic la réécrit.
  leader.anchor = owner_position + glm::vec3{0.0F, kBobbingNoteLift, 0.0F};
  leader.azimuth_degrees = static_cast<std::int16_t>(
      NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
  leader.degrees = kBobbingNoteFirstCornerDegrees;
  leader.mark[kBobbingNoteMarkOfImage] = 0;
  leader.mark[kBobbingNoteMarkOfByte] = byte;
  return node_tracks;
}

PooledEffectNode SowBobbingNoteNode(const EffectBobbingNotePattern& pattern,
                                    glm::vec3 owner_position, bool minimal_effects,
                                    std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kBobbingNote, owner_position);
  pooled.node.life_ticks = std::max(pattern.life_ticks, 1);
  pooled.node.render_word = kBobbingNoteRenderWord;
  pooled.node.frame_hold_ticks = 0;
  pooled.resource_paths = {std::string(EffectBobbingNoteTexture(pattern, minimal_effects))};
  pooled.tracks = SeedBobbingNoteTracks(pattern.byte, owner_position, generator);
  return pooled;
}

void AdvanceBobbingNoteTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              glm::vec3 node_position, std::int16_t billboard_turn_degrees,
                              std::int16_t billboard_tilt_degrees) {
  tracks[kTrackThatTakesTheTurn].style = billboard_turn_degrees;
  tracks[kTrackThatTakesTheTilt].style = billboard_tilt_degrees;
  EffectTrack& leader = tracks[kLeadingTrack];
  if (!leader.active) {
    return;
  }
  ++leader.ticks;
  float& frisson = leader.value[kBobbingNoteSlotOfShiver];
  frisson += kBobbingNoteShiverPerTick;
  if (frisson >= static_cast<float>(kEffectTrackFullTurnDegrees)) {
    frisson -= static_cast<float>(kEffectTrackFullTurnDegrees);
  }
  leader.azimuth_degrees = WrappedOnceToInt16(leader.azimuth_degrees + kBobbingNoteSwayPerTick);
  // ⚠ LE CLIENT AJOUTE le sinus à une ordonnée qui DESCEND : la note est au plus
  // bas quand le sinus vaut un.
  leader.anchor = node_position +
                  glm::vec3{0.0F,
                            kBobbingNoteLift -
                                (kBobbingNoteSwayAmplitude * SinOfDegrees(leader.azimuth_degrees)),
                            0.0F};
}

std::vector<EffectWorldQuad> ComposeBobbingNoteQuads(const EffectTrack& track,
                                                     std::int16_t turn_degrees,
                                                     std::int16_t tilt_degrees,
                                                     std::uint32_t jitter_seed) {
  std::vector<EffectWorldQuad> quads;
  if (!track.active || track.alpha <= 0) {
    return quads;
  }
  float frisson = track.value[kBobbingNoteSlotOfShiver];
  if (frisson >= static_cast<float>(kEffectTrackFullTurnDegrees) || frisson < 0.0F) {
    frisson = 0.0F;
  }
  const float radius = track.radius_at_anchor;
  const float etendue =
      (radius * SinOfDegrees(static_cast<std::int32_t>(frisson))) * kBobbingNoteShiverShare +
      radius;

  std::uint32_t rng_state = jitter_seed ^ static_cast<std::uint32_t>(track.ticks);
  std::array<std::int32_t, 4> angles{};
  angles[0] = track.degrees;
  for (std::size_t corner = 1; corner < angles.size(); ++corner) {
    angles[corner] = NextCorner(angles[corner - 1], rng_state);
  }
  std::array<glm::vec3, 4> poses;
  for (std::size_t corner = 0; corner < poses.size(); ++corner) {
    poses[corner] = {etendue * CosOfDegrees(angles[corner]), 0.0F,
                     etendue * SinOfDegrees(angles[corner])};
  }
  RotateEffectQuadCorners(poses, turn_degrees, tilt_degrees);
  // ⚠ L'ORDRE DE SORTIE N'EST PAS CELUI DES ANGLES : le client passe le deuxième
  // coin, le premier, le quatrième puis le troisième. C'est ce qui pose l'image
  // à l'endroit.
  constexpr std::array<std::size_t, 4> kEmissionOrder = {1, 0, 3, 2};
  std::array<glm::vec3, 4> quad_corners;
  for (std::size_t rang = 0; rang < quad_corners.size(); ++rang) {
    const glm::vec3& pose = poses[kEmissionOrder[rang]];
    // Le carré tourné par les angles de la caméra du client revient au nôtre
    // par `S` entier ; l'ancre y est déjà.
    quad_corners[rang] = track.anchor + EffectPointFromClient(pose);
  }

  const std::uint8_t octet = track.mark[kBobbingNoteMarkOfByte];
  const std::size_t image = track.mark[kBobbingNoteMarkOfImage];
  const std::int16_t blend = octet == kBobbingNoteByteOfAddedNote ? kAddedBlend : kOrdinaryBlend;
  const std::array<std::uint8_t, 3> halo =
      octet == kBobbingNoteByteOfBlueHalo ? kBobbingNoteBlueHalo : kWhite;
  const auto half_alpha = static_cast<std::int16_t>(track.alpha / kHaloAlphaDivisor);

  quads.reserve(1 + kHaloOffsets.size());
  quads.push_back(
      NoteQuad(quad_corners, glm::vec3{0.0F, 0.0F, 0.0F}, track.alpha, kWhite, blend, image));
  for (const glm::vec3& spread : kHaloOffsets) {
    quads.push_back(NoteQuad(quad_corners, EffectPointFromClient(spread), half_alpha, halo,
                             kAddedBlend, image));
  }
  return quads;
}

}  // namespace ro::render

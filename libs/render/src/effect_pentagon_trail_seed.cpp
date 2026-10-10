#include "ro/render/effect_pentagon_trail_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES TROIS FAMILLES D'OCTETS que le bâtisseur distingue, et rien entre les
// deux : il compare à des valeurs exactes, pas à des seuils.
[[nodiscard]] bool IsWideFamily(std::uint8_t octet) { return octet == 1 || octet == 11; }
[[nodiscard]] bool IsTightFamily(std::uint8_t octet) {
  return octet == 2 || octet == 12 || octet == 13;
}
// Le seul octet qui raccourcit la course de la traînée.
constexpr std::uint8_t kShortCourseArgument = 10;

// L'ALPHA que chaque famille pose.
constexpr std::int16_t kShortCourseAlpha = 25;
constexpr std::int16_t kTightAlpha = 30;
constexpr std::int16_t kInvisibleAlpha = 0;
constexpr std::int16_t kOrdinaryAlpha = 40;

// LA SUITE DES RAYONS, une par famille : le rang de la couche entre dedans.
constexpr float kLargeRadiusFirst = 4.0F;
constexpr float kLargeRadiusStep = 2.0F;
constexpr float kTightRadiusFirst = 1.0F;
constexpr float kTightRadiusStep = 0.5F;
constexpr float kOrdinaryRadiusFirst = 2.0F;
constexpr float kOrdinaryRadiusStep = 1.0F;

// LE RAYON DE LA COUCHE DE TÊTE, écrit en constante et non tiré de la suite.
constexpr float kLargeHeadRadius = 4.0F;
constexpr float kTightHeadRadius = 1.0F;
constexpr float kOrdinaryHeadRadius = 2.0F;

// DE COMBIEN LES COUCHES TOURNENT l'une par rapport à l'autre.
constexpr std::int16_t kAzimuthPerLayer = 25;

// LA COUCHE DE TÊTE : la dernière, et elle naît avec ce compteur déjà écoulé.
constexpr std::size_t kHeadLayer = kEffectTracksPerNode - 1;
constexpr std::int32_t kHeadStartsAtTick = 180;

// OÙ VA LE SECOND CENTRE, depuis la position de l'effet.
//
// ⚠ LE CLIENT RETRANCHE SUR LES TROIS AXES, et une soustraction ÉLÈVE dans son
// repère, qui descend : la traînée monte, et s'écarte en diagonale. Les deux
// courses sont écrites comme lui, et passent au nôtre avant de s'ajouter à la
// place de l'effet ; ajoutées telles quelles, elles la faisaient plonger de
// trois cents unités sous le sol.
constexpr glm::vec3 kLongCourse{-70.0F, -300.0F, -70.0F};
constexpr glm::vec3 kShortCourse{-15.0F, -60.0F, -15.0F};

}  // namespace

std::span<const EffectPentagonTrailPattern> EffectPentagonTrailPatternsFor(
    std::uint32_t effect_id) {
  const std::span<const EffectPentagonTrailPattern> table = EffectPentagonTrailPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectPentagonTrailPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectPentagonTrailPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectPentagonTrailTracks(
    const EffectPentagonTrailPattern& pattern, glm::vec3 origin) {
  const std::uint8_t octet = pattern.builder_argument;
  const bool large = IsWideFamily(octet);
  const bool tight = IsTightFamily(octet);
  const glm::vec3 far_point =
      origin + EffectPointFromClient(octet == kShortCourseArgument ? kShortCourse : kLongCourse);

  std::int16_t alpha = kOrdinaryAlpha;
  if (tight) {
    alpha = kTightAlpha;
  } else if (octet == kShortCourseArgument) {
    alpha = kShortCourseAlpha;
  } else if (octet == 11) {
    // ⚠ CELUI-CI NAÎT INVISIBLE, et son mouvement le fait monter puis
    // redescendre. Le semer visible le montrerait d'emblée à pleine force.
    alpha = kInvisibleAlpha;
  }

  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t layer = 0; layer < tracks.size(); ++layer) {
    EffectTrack& track = tracks[layer];
    const auto rang = static_cast<float>(layer);
    track.active = true;
    track.ticks = 0;
    track.alpha = alpha;
    track.radius_at_anchor = 0.0F;
    track.azimuth_degrees = static_cast<std::int16_t>(kAzimuthPerLayer * layer);
    track.mark[kMarkOfTrailShade] = octet;

    if (layer == kHeadLayer) {
      // ⚠ LA COUCHE DE TÊTE EST DÉJÀ LOIN : son compteur part à cent quatre-vingts.
      track.ticks = kHeadStartsAtTick;
      track.lift_key = large ? kLargeHeadRadius : (tight ? kTightHeadRadius : kOrdinaryHeadRadius);
    } else if (large) {
      track.lift_key = kLargeRadiusFirst + kLargeRadiusStep * rang;
    } else if (tight) {
      track.lift_key = kTightRadiusFirst + kTightRadiusStep * rang;
    } else {
      track.lift_key = kOrdinaryRadiusFirst + kOrdinaryRadiusStep * rang;
    }
    // ⚠ LA PREMIÈRE CASE GARDE LE RAYON DE REPOS, et le mouvement fait respirer
    // la clé autour d'elle. Les deux commencent donc égales, et divergent.
    track.value[0] = track.lift_key;

    track.tail[kTailSlotOfNearCenterX] = origin.x;
    track.tail[kTailSlotOfNearCenterY] = origin.y;
    track.tail[kTailSlotOfNearCenterZ] = origin.z;
    track.tail[kTailSlotOfFarCenterX] = far_point.x;
    track.tail[kTailSlotOfFarCenterY] = far_point.y;
    track.tail[kTailSlotOfFarCenterZ] = far_point.z;
  }
  return tracks;
}

PooledEffectNode SowEffectPentagonTrailNode(const EffectPentagonTrailPattern& pattern,
                                            glm::vec3 anchor, std::int32_t effect_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kPentagonTrail;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectPentagonTrailTracks(pattern, anchor);
  return pooled;
}

}  // namespace ro::render

#include "ro/render/effect_homing_gust_seed.h"

#include <cstddef>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_ground_gust.h"
#include "ro/render/effect_homing_gust.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"

namespace ro::render {
namespace {

// LES OCTETS QUE LE BÂTISSEUR DU 122 DISTINGUE.
constexpr std::uint8_t kOctetLeft = 0;
constexpr std::uint8_t kOctetRight = 1;
constexpr std::uint8_t kOctetEarlyLeft = 2;
constexpr std::uint8_t kOctetEarlyRight = 3;
constexpr std::uint8_t kOctetGreyLeft = 4;
constexpr std::uint8_t kOctetGreyRight = 5;

// Les deux rayons et les trois teintes qu'il en tire.
constexpr float kWideRadius = 25.0F;
constexpr float kNarrowRadius = 6.0F;
constexpr std::uint8_t kBlueColor = 0;
constexpr std::uint8_t kWhiteColor = 2;
constexpr std::uint8_t kGreyColor = 3;

// LES DEUX CÔTÉS, tels qu'ils se rangent dans la marque.
constexpr std::uint8_t kLeftSide = 0;
constexpr std::uint8_t kRightSide = 1;

// CE QUE LES DEUX BÂTISSEURS POSENT À L'IDENTIQUE, piste par piste.
//
// Allumée, compteur à zéro, alpha nul, l'ancre au nœud relevée de dix, cap nul
// (le premier tic utile l'écrase), inclinaison au rang, marques zéro et un à
// zéro.
void PlaceBase(EffectTrack& track, std::size_t rang, glm::vec3 anchor_point) {
  track.active = true;
  track.ticks = 0;
  track.alpha = 0;
  track.anchor = {anchor_point.x, anchor_point.y + kHomingGustLiftAboveNode, anchor_point.z};
  track.azimuth_degrees = 0;
  track.tilt_degrees = static_cast<std::int16_t>(rang);
  // ⚠ LA MARQUE ZÉRO N'EST LUE NI PAR LES DEUX MOUVEMENTS NI PAR LE
  // DESSINATEUR : elle est posée parce que le bâtisseur la pose.
  track.mark[0] = 0;
  track.mark[kHomingGustMarkOfState] = kHomingGustChasing;
}

}  // namespace

std::span<const EffectHomingGustPattern> EffectHomingGustPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectHomingGustPatternTable(), effect_id,
                           &EffectHomingGustPattern::effect_id);
}

HomingGustSteeredStyle HomingGustSteeredStyleOf(std::uint8_t octet) {
  HomingGustSteeredStyle style;
  // LE RAYON ET LA TEINTE.
  if (octet == kOctetGreyLeft || octet == kOctetGreyRight) {
    style.radius = kWideRadius;
    style.color = kGreyColor;
  } else if (octet <= kOctetRight) {
    style.radius = kWideRadius;
    style.color = kBlueColor;
  } else {
    style.radius = kNarrowRadius;
    style.color = kWhiteColor;
  }
  // LE CÔTÉ. ⚠ AU-DELÀ DE CINQ, RIEN N'EST ÉCRIT.
  if (octet == kOctetLeft || octet == kOctetEarlyLeft || octet == kOctetGreyLeft) {
    style.side = kLeftSide;
  } else if (octet == kOctetRight || octet == kOctetEarlyRight || octet == kOctetGreyRight) {
    style.side = kRightSide;
  }
  // L'EXTINCTION HÂTIVE, pour les deux octets du milieu.
  style.early_fade = (octet == kOctetEarlyLeft || octet == kOctetEarlyRight) ? 1 : 0;
  return style;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectHomingGustSteeredTracks(
    glm::vec3 anchor, std::uint8_t octet) {
  const HomingGustSteeredStyle style = HomingGustSteeredStyleOf(octet);
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    PlaceBase(track, rang, anchor);
    track.radius_at_anchor = style.radius;
    track.value[kGroundGustSlotOfSpeed] = kHomingGustSteeredSpeed;
    track.mark[kGroundGustMarkOfColor] = style.color;
    if (style.side.has_value()) {
      track.mark[kHomingGustMarkOfSide] = *style.side;
    }
    track.mark[kHomingGustMarkOfEarlyFade] = style.early_fade;
  }
  return tracks;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectHomingGustSpreadTracks(
    glm::vec3 anchor, std::uint8_t octet, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    PlaceBase(track, rang, anchor);
    // ⚠ LE RAYON D'ABORD, LA VITESSE ENSUITE : c'est l'ordre des deux `rand()`
    // du bâtisseur, et l'inverser échangerait les deux suites.
    track.radius_at_anchor =
        kHomingGustSpreadRadiusBase +
        static_cast<float>(DrawBelow(generator, kHomingGustSpreadRadiusDraws)) *
            kHomingGustSpreadRadiusStep;
    track.value[kGroundGustSlotOfSpeed] =
        kHomingGustSpreadSpeedBase +
        static_cast<float>(DrawBelow(generator, kHomingGustSpreadSpeedDraws)) *
            kHomingGustSpreadSpeedStep;
    // ⚠ LE CÔTÉ N'EST ÉCRIT QUE POUR ZÉRO ET UN ; le
    // site n'en passe pas d'autre.
    if (octet == kLeftSide || octet == kRightSide) {
      track.mark[kHomingGustMarkOfSide] = octet;
    }
    track.mark[kGroundGustMarkOfColor] = kHomingGustSpreadColor;
  }
  return tracks;
}

PooledEffectNode SowEffectHomingGustNode(const EffectHomingGustPattern& pattern, glm::vec3 anchor,
                                         const std::optional<glm::vec3>& target, std::uint8_t octet,
                                         std::int32_t effect_life_ticks, std::uint32_t& generator) {
  const bool dirige = pattern.kind == EffectHomingGustKind::kSteered;
  // LA PLACE DU NŒUD (+16..+24) : l'origine du premier cap, et le point où le
  // 122 rentre. ⚠ ELLE NE SUIT PAS LE PORTEUR : le mouvement ne la recopie pas.
  PooledEffectNode pooled = PooledNodeAt(
      dirige ? PooledNodeShape::kHomingGustSteered : PooledNodeShape::kHomingGustSpread, anchor);
  pooled.target_position = target;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = dirige ? SeedEffectHomingGustSteeredTracks(anchor, octet)
                         : SeedEffectHomingGustSpreadTracks(anchor, octet, generator);
  return pooled;
}

}  // namespace ro::render

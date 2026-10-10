#include "ro/render/effect_swarm_seed.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES DIX CASES DES DEUX GROUPES, dans l'ordre où le client les écrit : une
// référence de cap, une cible, un cap courant, une cible d'inclinaison, une
// inclinaison courante — et cela deux fois. Voir `AdvanceBillboardSwarmTracks`,
// qui les relit exactement ainsi.
constexpr std::size_t kGroups = 2;
constexpr std::size_t kSlotsPerGroup = 5;
constexpr std::size_t kSlotOfHeadingReference = 0;
constexpr std::size_t kSlotOfHeadingTarget = 1;
constexpr std::size_t kSlotOfHeading = 2;
constexpr std::size_t kSlotOfTiltTarget = 3;
constexpr std::size_t kSlotOfTilt = 4;
constexpr std::size_t kSlotOfFirstAlpha = 12;

// OÙ LA PREMIÈRE POSITION VIT : les trois cases de queue que la figure lit.
constexpr std::size_t kSlotOfTailX = 3;
constexpr std::size_t kSlotOfTailY = 4;
constexpr std::size_t kSlotOfTailZ = 5;

constexpr float kFullTurn = 360.0F;

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

// UN SEUL REPLI, comme le client : il compare une fois au-dessous de zéro et
// une fois au-dessus du tour, et n'y revient pas. Un modulo ramènerait dans le
// tour des valeurs que le client y laisse sortir.
[[nodiscard]] float WithinFullTurn(float degrees) {
  if (degrees < 0.0F) {
    degrees += kFullTurn;
  }
  if (degrees >= kFullTurn) {
    degrees -= kFullTurn;
  }
  return degrees;
}

}  // namespace

std::int16_t EffectSwarmHeadingDegrees(float toward_x, float toward_z) {
  return static_cast<std::int16_t>(HeadingOfVector(toward_x, toward_z));
}

std::span<const EffectSwarmPattern> EffectSwarmPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectSwarmPattern> table = EffectSwarmPatternTable();
  const auto first = std::ranges::lower_bound(table, effect_id, {}, &EffectSwarmPattern::effect_id);
  const auto last = std::ranges::upper_bound(table, effect_id, {}, &EffectSwarmPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSwarmTracks(
    const EffectSwarmPattern& pattern, glm::vec3 toward_target, std::uint32_t& generator) {
  const bool large = pattern.kind == EffectSwarmKind::kWide;
  // LE CAP EST CELUI DU CLIENT, comme tous ceux que le mouvement tire ensuite :
  // la profondeur du vecteur, prise dans notre monde, se nie avant d'y entrer.
  const std::int16_t cap = EffectSwarmHeadingDegrees(toward_target.x, -toward_target.z);

  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ LE LARGE ALLUME LES QUATRE, L'ÉTROIT LA PREMIÈRE SEULEMENT. Et chaque
  // piste du large TIRE POUR ELLE-MÊME : leur donner les mêmes nombres ferait
  // quatre essaims superposés, c'est-à-dire un seul quatre fois plus opaque.
  const std::size_t vivantes = large ? kEffectTracksPerNode : 1;
  for (std::size_t rang = 0; rang < vivantes; ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    track.lift_key =
        large
            ? kSwarmWideSpeedFloor +
                  kSwarmWideSpeedStep * static_cast<float>(DrawBelow(generator, kSwarmSpeedSpread))
            : kSwarmNarrowSpeedFloor + kSwarmNarrowSpeedStep * static_cast<float>(DrawBelow(
                                                                   generator, kSwarmSpeedSpread));
    const bool little = large && pattern.mark == kSwarmSmallRadiusMark;
    track.radius_at_anchor =
        little
            ? kSwarmSmallRadiusFloor + kSwarmRadiusStep * static_cast<float>(DrawBelow(
                                                              generator, kSwarmSmallRadiusSpread))
            : kSwarmRadiusFloor +
                  kSwarmRadiusStep * static_cast<float>(DrawBelow(generator, kSwarmRadiusSpread));

    // ⚠ LE CAP VERS LA CIBLE SE RANGE DANS L'INCLINAISON, et c'est l'ADR 0017
    // en pleine lumière : le champ s'appelle `tilt_degrees` parce qu'un anneau
    // y lit une inclinaison, et cet essaim y met la direction du sort. Tous ses
    // caps tirés s'en écartent.
    track.tilt_degrees = cap;

    // LES DEUX POSITIONS naissent au même point, au-dessus du nœud.
    track.tail[kSlotOfTailX] = 0.0F;
    track.tail[kSlotOfTailY] = kSwarmBirthHeight;
    track.tail[kSlotOfTailZ] = 0.0F;
    track.anchor = {0.0F, kSwarmBirthHeight, 0.0F};

    for (std::size_t groupe = 0; groupe < kGroups; ++groupe) {
      const std::size_t base = groupe * kSlotsPerGroup;
      track.value[base + kSlotOfHeadingReference] = static_cast<float>(cap);
      for (const std::size_t case_de_cap : {kSlotOfHeadingTarget, kSlotOfHeading}) {
        const std::int32_t drawn =
            large ? DrawBelow(generator, kSwarmWideHeadingSpread)
                  : DrawBelow(generator, kSwarmNarrowHeadingSpread) - kSwarmNarrowHeadingBias;
        track.value[base + case_de_cap] = static_cast<float>(cap + drawn);
      }
      track.value[base + kSlotOfTiltTarget] = kSwarmLevelDegrees;
      track.value[base + kSlotOfTilt] = kSwarmLevelDegrees;
      // ⚠ LES DEUX ALPHAS RESTENT À ZÉRO : c'est le mouvement qui les monte, de
      // vingt-cinq par tic. Les semer pleins ferait paraître l'essaim d'un coup.
      track.value[kSlotOfFirstAlpha + groupe] = 0.0F;
    }
    for (std::size_t rang_de_case = 0; rang_de_case < kGroups * kSlotsPerGroup; ++rang_de_case) {
      track.value[rang_de_case] = WithinFullTurn(track.value[rang_de_case]);
    }
    track.mark[0] = pattern.mark;
  }
  return tracks;
}

PooledEffectNode SowEffectSwarmNode(const EffectSwarmPattern& pattern, glm::vec3 anchor,
                                    glm::vec3 toward_target, std::int32_t effect_life_ticks,
                                    std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kBillboardSwarm;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectSwarmTracks(pattern, toward_target, generator);
  return pooled;
}

}  // namespace ro::render

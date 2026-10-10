#include "ro/render/effect_ground_curtain_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES DEUX CASES DE QUEUE QUI PORTENT LE BOUT LOINTAIN du segment au sol, et la
// case qui porte le style. Les mêmes que lit `ComposeGroundCurtainQuad`.
constexpr std::size_t kSlotOfFarX = 3;
constexpr std::size_t kSlotOfFarZ = 5;
constexpr std::size_t kSlotOfStyle = 0;

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

[[nodiscard]] bool EstUnTreillis(std::uint8_t octet) {
  return octet >= kGroundCurtainLatticeFirstByte && octet <= kGroundCurtainLatticeLastByte;
}

[[nodiscard]] bool IsCross(std::uint8_t octet) {
  return octet >= kGroundCurtainCrossFirstByte && octet <= kGroundCurtainCrossLastByte;
}

}  // namespace

std::span<const EffectGroundCurtainPattern> EffectGroundCurtainPatternsFor(
    std::uint32_t effect_id) {
  const std::span<const EffectGroundCurtainPattern> table = EffectGroundCurtainPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectGroundCurtainPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectGroundCurtainPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGroundCurtainTracks(
    const EffectGroundCurtainPattern& pattern, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  const std::uint8_t octet = pattern.arrangement;
  const bool spray = octet == kGroundCurtainScatterByte;
  const bool treillis = EstUnTreillis(octet);
  const bool cross = IsCross(octet);
  if (!spray && !treillis && !cross) {
    // ⚠ TOUT AUTRE OCTET NE SÈME RIEN, et le client non plus : il saute la
    // piste entière. Retomber sur la gerbe inventerait une figure.
    return tracks;
  }

  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    // ⚠ LA CROIX N'ALLUME QUE SES DEUX PREMIÈRES PISTES, mais le client écrit
    // quand même la géométrie des quatre. Ne rien écrire donnerait le même
    // écran et un état différent du sien.
    track.active = !cross || rang < kGroundCurtainCrossLiveTracks;
    track.ticks = 0;
    // L'ANGLE DE DÉPART EST TIRÉ AU SORT sur le tour entier : c'est lui que le
    // mouvement fait avancer, et c'est ce qui décale la respiration des pistes.
    track.degrees = static_cast<std::int16_t>(DrawBelow(generator, kEffectTrackFullTurnDegrees));

    if (spray) {
      track.alpha = kGroundCurtainScatterAlpha;
      // ⚠ LE PIED PROCHE EST AU CENTRE et le lointain n'importe où dans un
      // carré de quarante : la gerbe part du personnage, elle ne l'entoure pas.
      track.anchor = {0.0F, 0.0F, 0.0F};
      track.tail[kSlotOfFarX] =
          static_cast<float>(DrawBelow(generator, kGroundCurtainScatterSpread)) +
          kGroundCurtainScatterFloor;
      track.tail[kSlotOfFarZ] =
          static_cast<float>(DrawBelow(generator, kGroundCurtainScatterSpread)) +
          kGroundCurtainScatterFloor;
      // ⚠ ET LA GERBE NE POSE PAS DE STYLE : la case zéro garde zéro, et sa
      // tenture est donc BLANCHE. Lui donner l'octet du site la teindrait de la
      // couleur du premier style, que le client ne lui donne pas.
      continue;
    }

    track.value[kSlotOfStyle] = static_cast<float>(octet);
    if (treillis) {
      track.alpha = kGroundCurtainLatticeAlpha;
      const float shifted = static_cast<float>(DrawBelow(generator, kGroundCurtainLatticeSpread)) -
                            kGroundCurtainLatticeHalf;
      if (rang < kGroundCurtainLatticeTurnsAt) {
        // Un trait du bord lointain vers le bord proche, décalé au sort.
        track.tail[kSlotOfFarX] = shifted;
        track.tail[kSlotOfFarZ] = -kGroundCurtainLatticeHalf;
        track.anchor = {-shifted, 0.0F, kGroundCurtainLatticeHalf};
      } else {
        // Et un trait en travers, décalé au sort lui aussi.
        track.tail[kSlotOfFarX] = -kGroundCurtainLatticeHalf;
        track.tail[kSlotOfFarZ] = shifted;
        track.anchor = {kGroundCurtainLatticeHalf, 0.0F, -shifted};
      }
      continue;
    }

    track.alpha = kGroundCurtainCrossAlpha;
    if (rang < kGroundCurtainCrossTurnsAt) {
      // La branche de la croix qui suit l'axe des x, d'un bord à l'autre.
      track.tail[kSlotOfFarX] = kGroundCurtainCrossHalf;
      track.tail[kSlotOfFarZ] = 0.0F;
      track.anchor = {-kGroundCurtainCrossHalf, 0.0F, 0.0F};
    } else {
      // Et celle qui suit l'axe des z.
      track.tail[kSlotOfFarX] = 0.0F;
      track.tail[kSlotOfFarZ] = -kGroundCurtainCrossHalf;
      track.anchor = {0.0F, 0.0F, kGroundCurtainCrossHalf};
    }
  }
  return tracks;
}

PooledEffectNode SowEffectGroundCurtainNode(const EffectGroundCurtainPattern& pattern,
                                            glm::vec3 anchor, std::int32_t effect_life_ticks,
                                            std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kGroundCurtain;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectGroundCurtainTracks(pattern, generator);
  return pooled;
}

}  // namespace ro::render

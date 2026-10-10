#include "ro/render/effect_quad_burst_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_quad_burst.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES OCTETS QUE LE BÂTISSEUR COMPARE, un à un.
constexpr std::uint8_t kVariantFreezing = 0;
constexpr std::uint8_t kVariantBlue = 1;
constexpr std::uint8_t kVariantScaled = 2;
constexpr std::uint8_t kVariantGhost = 3;
constexpr std::uint8_t kVariantRising = 4;
constexpr std::uint8_t kVariantFadingFive = 5;
constexpr std::uint8_t kVariantScattering = 6;
constexpr std::uint8_t kVariantFadingSeven = 7;
constexpr std::uint8_t kVariantFadingEight = 8;

// COMBIEN DE PISTES CHAQUE OCTET ALLUME.
constexpr std::size_t kGhostLiveTracks = 1;
constexpr std::size_t kFadingLiveTracks = 2;

// LES TIRAGES DU SEMIS, dans l'ordre où le client les fait.
constexpr std::int32_t kHeightSpread = 100;
constexpr std::int32_t kAngleSpread = 360;
constexpr std::int32_t kGhostScatterSpread = 15;
constexpr std::int32_t kGhostScatterBias = 8;
constexpr std::int32_t kWideScatterSpread = 35;
constexpr std::int32_t kWideScatterBias = 17;
constexpr std::int32_t kScatteringStartSpread = 40;
constexpr float kScatteringStartFloor = -40.0F;

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& rng_state, std::int32_t exclusive_bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(rng_state) %
                                   static_cast<std::uint32_t>(exclusive_bound));
}

[[nodiscard]] bool IsFadingMark(std::uint8_t variant_value) {
  return variant_value == kVariantFadingFive || variant_value == kVariantFadingSeven ||
         variant_value == kVariantFadingEight;
}

// L'ÉCART LATÉRAL DE DÉPART d'une particule : nul pour presque tous les
// octets, tiré au sort pour le fantôme et le dispersé.
[[nodiscard]] float StartOffset(std::uint8_t variant_value, std::uint32_t& rng_state) {
  if (variant_value == kVariantGhost) {
    return static_cast<float>(DrawBelow(rng_state, kGhostScatterSpread) - kGhostScatterBias);
  }
  if (variant_value == kVariantScattering) {
    return static_cast<float>(DrawBelow(rng_state, kWideScatterSpread) - kWideScatterBias);
  }
  return 0.0F;
}

}  // namespace

std::span<const EffectQuadBurstPattern> EffectQuadBurstPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectQuadBurstPattern> table = EffectQuadBurstPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectQuadBurstPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectQuadBurstPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::string_view EffectQuadBurstTextureOf(std::uint8_t variant) {
  if (variant == kVariantGhost) {
    return kQuadBurstGhostTexture;
  }
  return variant >= kVariantBlue ? kQuadBurstWhiteTexture : kQuadBurstFreezingTexture;
}

std::uint8_t EffectQuadBurstMarkOf(std::uint8_t variant) {
  if (variant == kVariantBlue) {
    return kQuadBurstMarkBlue;
  }
  if (variant == kVariantGhost) {
    return kQuadBurstMarkGrey;
  }
  if (IsFadingMark(variant)) {
    return kQuadBurstMarkFading;
  }
  if (variant == kVariantRising) {
    return kQuadBurstMarkRising;
  }
  if (variant == kVariantScattering) {
    return kQuadBurstMarkScattering;
  }
  return kQuadBurstMarkWhite;
}

float EffectQuadBurstScaleOf(std::uint8_t variant) {
  return variant == kVariantScaled ? kQuadBurstScaleForSeven : 0.0F;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectQuadBurstTracks(
    const EffectQuadBurstPattern& pattern, std::uint32_t& generator) {
  const std::uint8_t variant_value = pattern.variant;
  const std::uint8_t mark = EffectQuadBurstMarkOf(variant_value);
  // ⚠ TROIS OCTETS N'ALLUMENT PAS LES QUATRE PISTES, et le client écrit quand
  // même l'état des quatre. Ne rien écrire donnerait le même écran et un état
  // différent du sien.
  const std::size_t vivantes =
      variant_value == kVariantGhost ? kGhostLiveTracks
      : (variant_value == kVariantFadingSeven || variant_value == kVariantFadingEight)
          ? kFadingLiveTracks
          : kEffectTracksPerNode;

  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = rang < vivantes;
    // ⚠ LE FANTÔME EST DEUX FOIS PLUS LENT ET PLUS GROS, et c'est le seul.
    track.lift_key = variant_value == kVariantGhost ? kQuadBurstGhostSpeed : kQuadBurstSpeed;
    if (variant_value == kVariantGhost) {
      track.radius_at_anchor = kQuadBurstGhostRadius;
    } else if (variant_value == kVariantFreezing || variant_value == kVariantScaled ||
               variant_value == kVariantFadingSeven) {
      track.radius_at_anchor = kQuadBurstSmallRadius;
    } else {
      track.radius_at_anchor = kQuadBurstRadius;
    }
    track.mark[0] = mark;

    // LES QUATRE PARTICULES, dans l'ordre du client : un écart, une hauteur,
    // puis QUATRE angles — et cela quatre fois.
    for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
      EffectQuadBurstOffset offset;
      offset.sideways = StartOffset(variant_value, generator);
      offset.forward =
          variant_value == kVariantGhost
              ? static_cast<float>(DrawBelow(generator, kGhostScatterSpread) - kGhostScatterBias)
              : 0.0F;
      offset.depth_below = static_cast<float>(DrawBelow(generator, kHeightSpread));
      SetEffectQuadBurstOffset(track, particle, offset);
      for (std::size_t angle = 0; angle < 4; ++angle) {
        track.value[4 * particle + angle] = static_cast<float>(DrawBelow(generator, kAngleSpread));
      }
    }

    // ⚠ ET TROIS OCTETS REPRENNENT LES QUATRE HAUTEURS APRÈS COUP. Le client
    // les tire d'abord au sort, puis les écrase — un tirage pour rien, mais
    // qui AVANCE LE GÉNÉRATEUR. Le sauter décalerait tous les angles suivants.
    if (variant_value == kVariantRising) {
      for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
        EffectQuadBurstOffset offset = EffectQuadBurstOffsetOf(track, particle);
        offset.depth_below = kQuadBurstRisingStarts[particle];
        SetEffectQuadBurstOffset(track, particle, offset);
      }
    } else if (variant_value == kVariantScattering) {
      for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
        EffectQuadBurstOffset offset = EffectQuadBurstOffsetOf(track, particle);
        offset.depth_below = kScatteringStartFloor -
                             static_cast<float>(DrawBelow(generator, kScatteringStartSpread));
        SetEffectQuadBurstOffset(track, particle, offset);
      }
    } else if (variant_value == kVariantFadingFive || variant_value == kVariantFadingSeven) {
      for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
        EffectQuadBurstOffset offset = EffectQuadBurstOffsetOf(track, particle);
        offset.depth_below = 0.0F;
        SetEffectQuadBurstOffset(track, particle, offset);
      }
    }
    if (mark == kQuadBurstMarkFading) {
      // ⚠ LA MARQUE QUINZE RANGE L'OCTET DU SITE DANS SA SIXIÈME MARQUE, et
      // c'est lui qui choisit sa teinte. Les quatre marques d'usure partent à
      // zéro.
      for (std::size_t particle = 0; particle < kQuadBurstParticles; ++particle) {
        track.mark[particle + 1] = 0;
      }
      track.mark[kQuadBurstParticles + 1] = variant_value;
    }
  }
  return tracks;
}

PooledEffectNode SowEffectQuadBurstNode(const EffectQuadBurstPattern& pattern, glm::vec3 anchor,
                                        std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kQuadBurst;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = kQuadBurstEndlessLifeTicks;
  pooled.resource_paths = {std::string(EffectQuadBurstTextureOf(pattern.variant))};
  pooled.trail_variant = static_cast<std::uint8_t>(EffectQuadBurstScaleOf(pattern.variant));
  pooled.tracks = SeedEffectQuadBurstTracks(pattern, generator);
  return pooled;
}

}  // namespace ro::render

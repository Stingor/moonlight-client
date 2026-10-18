#include "ro/render/effect_cloud_ring_seed.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_draw_below.h"
#include "effect_track_angles.h"

namespace ro::render {
namespace {

// CE QUE CHAQUE OCTET POSE PAR PISTE, et les deux qui TIRENT leur rayon.
//
// ⚠ LES QUATRE FORMULES SONT DES MESURES, pas un motif : deux comptent vers le
// haut, deux vers le bas, et leurs pentes ne se déduisent pas l'une de l'autre.
[[nodiscard]] float RankKey(std::uint8_t octet, std::size_t rang) {
  const auto r = static_cast<float>(rang);
  switch (octet) {
    case 1:
      return 2.0F * r + 3.0F;
    case 2:
      return 48.0F - 5.0F * r;
    case 3:
      return 28.0F - 5.0F * r;
    default:
      return 2.0F * r + 5.0F;
  }
}

// LE RAYON : tiré au sort pour les octets zéro et un, calculé pour les deux
// autres.
constexpr std::int32_t kRadiusSpread = 101;

[[nodiscard]] float RankRadius(std::uint8_t octet, std::size_t rang, std::uint32_t& rng_state) {
  const auto r = static_cast<float>(rang);
  if (octet == 1) {
    return static_cast<float>(DrawBelow(rng_state, kRadiusSpread)) * 0.01F + 2.5F;
  }
  if (octet == 0) {
    return static_cast<float>(DrawBelow(rng_state, kRadiusSpread)) * 0.03F + 4.5F;
  }
  return octet == 2 ? 14.0F - 2.0F * r : 6.0F - r;
}

}  // namespace

std::span<const EffectCloudRingPattern> EffectCloudRingPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectCloudRingPatternTable(), effect_id,
                           &EffectCloudRingPattern::effect_id);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectCloudRingTracks(
    const EffectCloudRingPattern& pattern, std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> node_tracks;
  // ⚠ LE NUAGE DE SOIN NE TIRE RIEN, et il ne touche donc PAS au générateur.
  // Le faire passer par les formules de l'octet lui donnerait deux rayons tirés
  // au sort et décalerait la suite du générateur pour tout ce qui vient après.
  if (pattern.builder == EffectCloudRingBuilder::kHealCloud) {
    for (std::size_t rang = 0; rang < node_tracks.size(); ++rang) {
      EffectTrack& track = node_tracks[rang];
      const auto r = static_cast<float>(rang);
      const auto row = static_cast<std::int16_t>(rang);
      track.active = true;
      track.alpha = 0;
      track.style = 0;
      track.degrees = static_cast<std::int16_t>(kHealCloudFirstSweepDegrees -
                                                (kHealCloudSweepFallPerRank * row));
      track.azimuth_degrees = static_cast<std::int16_t>(kHealCloudAzimuthPerRank * row);
      track.lift_key = kHealCloudLiftKey;
      track.radius_at_anchor = kHealCloudFirstRadius + (kHealCloudRadiusGrowthPerRank * r);
      track.tilt_degrees = kHealCloudTiltDegrees;
      track.value.fill(kHealCloudFirstSegmentLength - (kHealCloudSegmentFallPerRank * r));
      track.mark.fill(0);
    }
    return node_tracks;
  }
  // ⚠ LE NUAGE VIOLET ET ROUGE NON PLUS NE TIRE RIEN, et son STYLE d'entrée ne
  // choisit que la teinte : ses quatre pistes sont les mêmes pour le mur bleu et
  // pour le rouge. Passer son style en style de PISTE ferait mentir le
  // mouvement, qui n'attend là qu'un zéro, un ou deux.
  if (pattern.builder == EffectCloudRingBuilder::kVioletRedCloud) {
    for (std::size_t rang = 0; rang < node_tracks.size(); ++rang) {
      EffectTrack& track = node_tracks[rang];
      const auto r = static_cast<float>(rang);
      track.active = true;
      track.alpha = 0;
      track.style = kVioletRedCloudStyle;
      track.degrees = kCloudRingStartSweepDegrees;
      track.azimuth_degrees =
          static_cast<std::int16_t>(kCloudRingDegreesPerTrack * static_cast<std::int16_t>(rang));
      track.lift_key = kVioletRedCloudFirstLiftKey + (kVioletRedCloudLiftKeyPerRank * r);
      track.radius_at_anchor = kVioletRedCloudFirstRadius - (kVioletRedCloudRadiusFallPerRank * r);
      track.tilt_degrees = kVioletRedCloudTiltDegrees;
      track.value.fill(kVioletRedCloudSegmentLength);
      track.mark.fill(0);
    }
    return node_tracks;
  }
  for (std::size_t rang = 0; rang < node_tracks.size(); ++rang) {
    EffectTrack& track = node_tracks[rang];
    track.active = true;
    track.alpha = 0;
    track.degrees = kCloudRingStartSweepDegrees;
    track.azimuth_degrees =
        static_cast<std::int16_t>(kCloudRingDegreesPerTrack * static_cast<std::int16_t>(rang));
    // ⚠ L'OCTET CHOISIT LE STYLE, et le style tout le mouvement : un plafond de
    // balayage, un moment de chute et une croissance de rayon.
    track.style = pattern.style_byte < 2 ? kCloudRingSlowStyle : kCloudRingWideStyle;
    // ⚠ ET LA CLÉ EST UNE HAUTEUR pour ce dessinateur-ci. Voir
    // `ComposeCloudRingQuads`.
    track.lift_key = RankKey(pattern.style_byte, rang);
    // L'ORDRE DES TIRAGES : le rayon d'abord — quand il se tire —, l'inclinaison
    // ensuite. Les mélanger décalerait la suite du générateur.
    track.radius_at_anchor = RankRadius(pattern.style_byte, rang, generator);
    track.tilt_degrees =
        static_cast<std::int16_t>(DrawBelow(generator, kCloudRingTiltSpread) + kCloudRingTiltFloor);
    // LE RUBAN NAÎT DE LONGUEUR ÉGALE : les vingt et une cases à un.
    track.value.fill(kCloudRingSegmentLength);
    track.mark.fill(0);
  }
  return node_tracks;
}

PooledEffectNode SowEffectCloudRingNode(const EffectCloudRingPattern& pattern, glm::vec3 anchor,
                                        std::int32_t effect_life_ticks, std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kCloudRing, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.trail_variant = pattern.trail_variant;
  pooled.tracks = SeedEffectCloudRingTracks(pattern, generator);
  return pooled;
}

void AdvanceCloudRingTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    if (track.ticks <= 0) {
      continue;
    }

    // L'AZIMUT TOURNE, et reboucle au tour.
    TurnOnward(track.azimuth_degrees, kCloudRingSpinPerTick);

    // ⚠ LE BALAYAGE S'OUVRE DE TROIS PAR TIC, et c'est tout l'effet : l'anneau
    // se déroule au lieu de paraître entier.
    const bool large = track.style == kCloudRingWideStyle;
    track.degrees = static_cast<std::int16_t>(track.degrees + kCloudRingSweepPerTick);
    track.degrees = std::min<std::int16_t>(track.degrees,
                                           large ? kCloudRingWideSweepCap : kCloudRingSlowSweepCap);

    // ⚠ ET LE RAYON NE CROÎT QUE POUR DEUX STYLES. Le client saute son incrément
    // pour tout style qui n'est ni le large après douze tics, ni le lent après
    // vingt. Aux autres, l'anneau garde son rayon.
    bool radius_grows = false;
    if (large) {
      if (track.ticks > kCloudRingWideFallAfterTick) {
        track.alpha = std::max<std::int16_t>(track.alpha - kCloudRingWideFall, 0);
      }
      radius_grows = track.ticks > kCloudRingRiseUntilTick;
    } else if (track.style == kCloudRingSlowStyle && track.ticks > kCloudRingSlowFallAfterTick) {
      // ⚠ LA CHUTE DE L'ALPHA ET LA CROISSANCE DU RAYON VONT ENSEMBLE ici : le
      // client les met sur le même chemin. Les séparer ferait grandir un anneau
      // qui ne s'efface pas.
      track.alpha = std::max<std::int16_t>(track.alpha - kCloudRingSlowFall, 0);
      radius_grows = true;
    }
    if (radius_grows) {
      track.radius_at_anchor += kCloudRingRadiusPerTick;
    }

    if (track.ticks > kCloudRingLateFallAfterTick &&
        track.style != kCloudRingStyleWithoutLateFall) {
      track.alpha = std::max<std::int16_t>(track.alpha - kCloudRingLateFall, 0);
    }

    // LA MONTÉE, sous le douzième tic — et elle vient APRÈS les descentes, comme
    // chez le client.
    if (track.ticks < kCloudRingRiseUntilTick) {
      if (large) {
        track.alpha = static_cast<std::int16_t>(track.alpha + kCloudRingWideRise);
      } else {
        track.alpha =
            std::min<std::int16_t>(track.alpha + kCloudRingSlowRise, kCloudRingSlowRiseCap);
      }
    }
  }
}

}  // namespace ro::render

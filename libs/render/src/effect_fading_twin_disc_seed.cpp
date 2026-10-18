#include "ro/render/effect_fading_twin_disc_seed.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/render/effect_breathing_quad.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES OCTETS QUE LE BÂTISSEUR NOMME, testés un à un par égalité : ce ne sont
// pas des bornes.
constexpr std::uint8_t kOctetGreen = 1;
constexpr std::uint8_t kOctetLilac = 2;
constexpr std::uint8_t kOctetBlue = 3;

// LE STYLE QUE CHAQUE OCTET ÉCRIT — flottants 15,0 / 17,0 / 18,0, et 4,0 pour
// tout autre. La figure les
// lit comme trois teintes et un blanc : voir `EffectBreathingQuadStyleFor`.
constexpr std::int32_t kStyleOfOctetGreen = 15;
constexpr std::int32_t kStyleOfOctetLilac = 17;
constexpr std::int32_t kStyleOfOctetBlue = 18;
constexpr std::int32_t kStyleOfOtherOctets = 4;

// LE RAYON D'ANCRE : quinze, et vingt-quatre pour le seul octet trois.
constexpr float kOrdinaryRadius = 15.0F;
constexpr float kRadiusOfOctetBlue = 24.0F;

}  // namespace

std::span<const EffectFadingTwinDiscPattern> EffectFadingTwinDiscPatternsFor(
    std::uint32_t effect_id) {
  const std::span<const EffectFadingTwinDiscPattern> table = EffectFadingTwinDiscPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectFadingTwinDiscPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectFadingTwinDiscPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

EffectFadingTwinDiscSeed EffectFadingTwinDiscSeedFor(std::uint8_t builder_argument) {
  EffectFadingTwinDiscSeed seed;
  switch (builder_argument) {
    case kOctetGreen:
      seed.node_style = kStyleOfOctetGreen;
      break;
    case kOctetLilac:
      seed.node_style = kStyleOfOctetLilac;
      break;
    case kOctetBlue:
      seed.node_style = kStyleOfOctetBlue;
      break;
    default:
      seed.node_style = kStyleOfOtherOctets;
      break;
  }
  seed.radius_at_anchor = builder_argument == kOctetBlue ? kRadiusOfOctetBlue : kOrdinaryRadius;
  return seed;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectFadingTwinDiscTracks(
    const EffectFadingTwinDiscPattern& pattern) {
  const EffectFadingTwinDiscSeed seed = EffectFadingTwinDiscSeedFor(pattern.builder_argument);
  std::array<EffectTrack, kEffectTracksPerNode> node_tracks;
  for (std::size_t rang = 0; rang < kFadingTwinDiscLiveTracks; ++rang) {
    EffectTrack& track = node_tracks[rang];
    track.active = true;
    // ⚠ LES COMPTEURS SONT REMIS À ZÉRO NOMMÉMENT : c'est d'eux que le
    // mouvement date l'extinction.
    track.ticks = 0;
    track.alpha = kFadingTwinDiscAlphaAtBirth;
    // ⚠ LE MÊME RAYON POUR LES DEUX : la seconde piste COPIE celui de la première.
    track.radius_at_anchor = seed.radius_at_anchor;
    // ⚠ ZÉRO ET CENT QUATRE-VINGTS : les deux souffles sont en opposition de
    // phase, l'un au plus large quand l'autre est au plus serré.
    track.tilt_degrees = rang == 0 ? static_cast<std::int16_t>(0) : kFadingTwinDiscSecondTilt;
    track.value[kSlotOfBreathingHeight] = kFadingTwinDiscHeight;
  }
  return node_tracks;
}

std::int32_t FadingTwinDiscLifeTicks(const EffectFadingTwinDiscPattern& pattern,
                                     std::int32_t effect_life_ticks) {
  // ⚠ LE BÂTISSEUR COPIE LA VIE DE L'INSTANCE,
  // et un voisin appelé AVANT lui dans le même cas peut l'avoir relevée : c'est
  // la valeur relevée que le nœud emporte, pas la durée de la table.
  return std::max(effect_life_ticks, pattern.instance_life_floor_ticks);
}

PooledEffectNode SowEffectFadingTwinDiscNode(const EffectFadingTwinDiscPattern& pattern,
                                             glm::vec3 anchor, std::int32_t effect_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kFadingTwinDisc;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = FadingTwinDiscLifeTicks(pattern, effect_life_ticks);
  pooled.node.render_word = kFadingTwinDiscRenderWord;
  // L'IMAGE UNIQUE du nœud, passée par le site.
  pooled.resource_paths = {std::string(pattern.texture)};
  // ⚠ LE STYLE VIT DANS LE NŒUD, et la variante de traînée le porte comme
  // pour les autres sous-types de cette figure : la boucle de pistes du
  // dessinateur ne le décale pas, si bien que les deux disques le partagent.
  pooled.trail_variant =
      static_cast<std::uint8_t>(EffectFadingTwinDiscSeedFor(pattern.builder_argument).node_style);
  pooled.tracks = SeedEffectFadingTwinDiscTracks(pattern);
  return pooled;
}

void AdvanceFadingTwinDiscTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 std::int32_t life_ticks) {
  const std::int32_t seuil = life_ticks - kFadingTwinDiscFadeTicks;
  // ⚠ LES DEUX PREMIÈRES PISTES SEULEMENT, écrites à déplacement fixe et sans
  // tester qu'elles vivent : le client ajoute avant de regarder.
  for (std::size_t rang = 0; rang < kFadingTwinDiscLiveTracks; ++rang) {
    EffectTrack& track = tracks[rang];
    ++track.ticks;
    TurnOnward(track.tilt_degrees, kFadingTwinDiscTiltPerTick);
    // ⚠ STRICTEMENT AU-DELÀ du seuil : au tic `vie − 25`
    // lui-même, l'alpha tient encore.
    if (track.ticks > seuil) {
      track.alpha = static_cast<std::int16_t>(
          std::max(0, track.alpha - static_cast<std::int32_t>(kFadingTwinDiscAlphaStepPerTick)));
    }
  }
}

}  // namespace ro::render

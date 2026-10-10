#include "ro/render/effect_twin_disc_seed.h"

#include <algorithm>
#include <cstddef>
#include <string>

#include "ro/render/effect_breathing_quad.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {

std::span<const EffectTwinDiscPattern> EffectTwinDiscPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectTwinDiscPattern> table = EffectTwinDiscPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectTwinDiscPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectTwinDiscPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTwinDiscTracks(
    const EffectTwinDiscPattern& pattern) {
  std::array<EffectTrack, kEffectTracksPerNode> node_tracks;
  for (std::size_t rang = 0; rang < kTwinDiscLiveTracks; ++rang) {
    EffectTrack& track = node_tracks[rang];
    track.active = true;
    track.alpha = pattern.alpha;
    track.radius_at_anchor = pattern.radius;
    // ⚠ ZÉRO ET CENT QUATRE-VINGTS : les deux disques se font face, et leurs
    // souffles sont donc en opposition de phase. Les poser à la même
    // inclinaison les superposerait.
    track.tilt_degrees = rang == 0 ? static_cast<std::int16_t>(0) : kTwinDiscSecondTilt;
    track.value[kSlotOfBreathingHeight] = kTwinDiscHeight;
    // ⚠ L'AZIMUT ET LA CASE DE VARIANTE RESTENT À ZÉRO : aucun des deux
    // bâtisseurs ne les écrit, et le constructeur efface les quatre pistes. Le
    // style quatre ne lit l'azimut que si la variante vaut aussi quatre :
    // ici, le départ est le rang fois vingt-trois, comme pour les autres styles.
  }
  return node_tracks;
}

PooledEffectNode SowEffectTwinDiscNode(const EffectTwinDiscPattern& pattern, glm::vec3 anchor,
                                       std::int32_t effect_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kTwinDisc;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  // LA VIE DU NŒUD EST CELLE DE L'INSTANCE.
  pooled.node.life_ticks = effect_life_ticks;
  // LE MOT DE RENDU, treize ou cinq : les deux ont le bit du mélange
  // propre, si bien que l'addition que le style demande tient. Voir
  // `effect_render_word.h`.
  pooled.node.render_word = pattern.render_word;
  // L'IMAGE UNIQUE du nœud, passée par le site.
  pooled.resource_paths = {std::string(pattern.texture)};
  // ⚠ LE STYLE VIT DANS LE NŒUD, et la variante de traînée le porte comme
  // pour le sous-type 39 : la boucle de pistes du dessinateur ne le décale pas,
  // si bien que les deux disques le partagent.
  pooled.trail_variant = static_cast<std::uint8_t>(pattern.node_style);
  pooled.tracks = SeedEffectTwinDiscTracks(pattern);
  return pooled;
}

void AdvanceTwinDiscTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // ⚠ LES DEUX PREMIÈRES PISTES SEULEMENT, et sans tester qu'elles vivent : le
  // client ajoute avant de regarder. Voir l'en-tête.
  for (std::size_t rang = 0; rang < kTwinDiscLiveTracks; ++rang) {
    EffectTrack& track = tracks[rang];
    track.tilt_degrees = static_cast<std::int16_t>(track.tilt_degrees + kTwinDiscTiltPerTick);
    if (track.tilt_degrees >= kEffectTrackFullTurnDegrees) {
      track.tilt_degrees =
          static_cast<std::int16_t>(track.tilt_degrees - kEffectTrackFullTurnDegrees);
    }
  }
}

}  // namespace ro::render

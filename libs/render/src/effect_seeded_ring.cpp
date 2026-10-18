#include "ro/render/effect_seeded_ring.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

namespace ro::render {
namespace {

// UN DEGRÉ TIRÉ ENTRE DEUX BORNES, celles-ci comprises.
//
// ⚠ LE GÉNÉRATEUR N'AVANCE QUE S'IL Y A QUELQUE CHOSE À TIRER : deux bornes
// égales sont un azimut fixe, et brûler un tirage pour elles décalerait tous
// ceux des pistes suivantes.
[[nodiscard]] std::int16_t DegreesWithin(std::int16_t low, std::int16_t high,
                                         std::uint32_t& generator) {
  if (high <= low) {
    return low;
  }
  const std::uint32_t span = static_cast<std::uint32_t>(high - low) + 1U;
  return static_cast<std::int16_t>(low +
                                   static_cast<std::int16_t>(NextEffectNodeDraw(generator) % span));
}

// UNE VALEUR TIRÉE ENTRE DEUX BORNES, celles-ci comprises, en autant de
// valeurs que le client : voir `EffectNodeRange::values`.
//
// ⚠ MÊME RÈGLE QUE POUR L'AZIMUT : deux bornes égales sont une valeur fixe, et
// brûler un tirage pour elles décalerait tous ceux des pistes suivantes.
[[nodiscard]] float ValueWithin(float low, float high, std::int32_t values,
                                std::uint32_t& generator) {
  return DrawEffectNodeRange(EffectNodeRange{low, high, values}, generator);
}

}  // namespace

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSeededRingTracks(
    const EffectSeededRingPattern& pattern, std::uint32_t& generator) {
  const std::span<const EffectSeededRingTrack> table = EffectSeededRingTrackTable();
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    const std::size_t ou = pattern.first_track + rang;
    if (ou >= table.size()) {
      break;
    }
    const EffectSeededRingTrack& seed = table[ou];
    EffectTrack& track = tracks[rang];
    track.active = seed.active;
    track.alpha = seed.alpha;
    track.style = seed.style;
    track.degrees = seed.degrees;
    track.azimuth_degrees = DegreesWithin(seed.azimuth_degrees, seed.azimuth_high, generator);
    track.tilt_degrees = seed.tilt_degrees;
    track.ticks = seed.ticks;
    track.mark.fill(seed.slot_mark);
    // ⚠ SUIVRE UNE PISTE PASSE AVANT TIRER. Une piste qui reprend le tirage
    // d'une autre ne tire rien : refaire le tirage donnerait deux anneaux dont
    // l'écart varie, là où le client en donne deux dont l'écart est constant.
    // Le rang suivi est toujours inférieur au sien, donc déjà semé.
    const std::size_t suivie = static_cast<std::size_t>(seed.follows_track);
    if (seed.follows_track > kSeededRingTrackDrawsAlone && suivie < rang) {
      track.lift_key = tracks[suivie].lift_key + seed.lift_key_plus;
      track.radius_at_anchor = tracks[suivie].radius_at_anchor + seed.radius_at_anchor_plus;
      continue;
    }
    track.lift_key =
        ValueWithin(seed.lift_key, seed.lift_key_high, seed.lift_key_values, generator);
    track.radius_at_anchor = ValueWithin(seed.radius_at_anchor, seed.radius_at_anchor_high,
                                         seed.radius_at_anchor_values, generator);
  }
  // LA COPIE DE LA CLÉ, une fois toutes tirées ou suivies. Voir
  // `EffectSeededRingTrack::lift_key_seeds_tail`.
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    const std::size_t ou = pattern.first_track + rang;
    if (ou < table.size() && table[ou].lift_key_seeds_tail) {
      tracks[rang].tail[kRadialSpreadKeyBaseTailSlot] = tracks[rang].lift_key;
    }
  }
  return tracks;
}

PooledEffectNode SowEffectSeededRingNode(const EffectSeededRingPattern& pattern,
                                         glm::vec3 ground_position, std::uint32_t& generator,
                                         std::int32_t effect_life_ticks) {
  // ⚠ LA FORME VIENT DU PATRON, et le patron la tient du relevé des mouvements.
  // Le dessinateur est commun à tous ; le mouvement ne l'est pas.
  PooledEffectNode pooled = PooledNodeAt(pattern.shape, ground_position);
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.trail_variant = pattern.trail_variant;
  // 🔴 LA VIE VIENT DE L'INSTANCE, ET ELLE SE RELÈVE. Les onze bâtisseurs de
  // cette famille donnent à leur nœud la durée de leur instance, et la table des durées la
  // porte — voir l'en-tête. Le défaut du constructeur, cent tics, tenait lieu de
  // réponse ici : la colonne du portail mourait en une seconde et demie là
  // où le mur en dure vingt-trois.
  const std::int32_t life = std::max(effect_life_ticks, pattern.instance_life_floor_ticks);
  pooled.node.life_ticks = std::max(life, 1);
  pooled.tracks = SeedEffectSeededRingTracks(pattern, generator);
  // 🔴 UNE VIE COURTE ÉTEINT LES PISTES ET LES ÉCHELONNE, chez les bâtisseurs
  // qui relèvent la vie de leur instance : voir
  // `EffectSeededRingPattern::instance_life_floor_ticks`.
  if (pattern.instance_life_floor_ticks > 0 && life <= kSeededRingLitLifeTicks) {
    for (std::size_t rang = 0; rang < pooled.tracks.size(); ++rang) {
      pooled.tracks[rang].alpha = 0;
      pooled.tracks[rang].ticks = static_cast<std::int32_t>(rang) * kSeededRingDelayTicksPerRank;
    }
  }
  return pooled;
}

}  // namespace ro::render

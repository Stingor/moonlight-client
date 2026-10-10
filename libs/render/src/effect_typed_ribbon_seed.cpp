#include "ro/render/effect_typed_ribbon_seed.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// CE QUE LES QUATRE PISTES PARTAGENT, relevé dans le bâtisseur.
constexpr std::int16_t kSweepDegrees = kEffectTrackFullTurnDegrees;
constexpr float kWaveKey = 23.0F;
constexpr float kRadiusAtAnchor = 4.3F;
constexpr std::int16_t kStartsInvisible = 0;

// ET CE QUI LES SÉPARE : deux angles, et rien d'autre.
//
// ⚠ LES AZIMUTS SONT À QUATRE-VINGT-DIX DEGRÉS D'ÉCART, mais ils ne commencent
// pas à zéro : cent quatre-vingts, deux cent soixante-dix, zéro, puis quatre-
// vingt-dix. Comme le rayon de base porte le sinus de l'azimut, l'ordre décide
// de l'étagement — et le remettre à plat superposerait deux rubans sur quatre.
constexpr std::array<std::int16_t, kEffectTracksPerNode> kAzimuths = {180, 270, 0, 90};
constexpr std::array<std::int16_t, kEffectTracksPerNode> kTilts = {75, 63, 52, 40};

// Le plus long nom de fichier qu'un modèle puisse rendre.
constexpr std::size_t kLongestPath = 128;

// ⚠ LA NUMÉROTATION DES IMAGES COMMENCE À UN. Partir de zéro chargerait une
// image `00` qui n'existe pas et perdrait la dernière : le cycle d'images
// tournerait sur une période plus courte d'un cran.
constexpr int kFirstImageNumber = 1;

}  // namespace

std::span<const EffectTypedRibbonPattern> EffectTypedRibbonPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectTypedRibbonPattern> table = EffectTypedRibbonPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectTypedRibbonPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectTypedRibbonPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

std::vector<std::string> EffectTypedRibbonImagePaths(const EffectTypedRibbonPattern& pattern) {
  std::vector<std::string> paths;
  paths.reserve(pattern.image_count);
  const std::string modele(pattern.path_template);
  for (std::uint8_t rang = 0; rang < pattern.image_count; ++rang) {
    std::array<char, kLongestPath> tampon{};
    // ⚠ LE MODÈLE VIENT D'UNE TABLE ENGENDRÉE, pas de l'extérieur : c'est le
    // `sprintf` du client transcrit tel quel, et son seul champ est un entier.
    //
    // `-Wformat-nonliteral` est donc éteint ICI, sur cette ligne, et nulle part
    // ailleurs : un format qui viendrait d'une archive ou du réseau serait une
    // faille, et c'est exactement ce que l'avertissement doit continuer à
    // attraper partout dans ce dépôt.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    const int ecrit = std::snprintf(tampon.data(), tampon.size(), modele.c_str(),
                                    kFirstImageNumber + static_cast<int>(rang));
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    if (ecrit > 0) {
      paths.emplace_back(tampon.data());
    }
  }
  return paths;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectTypedRibbonTracks(
    const EffectTypedRibbonPattern& pattern) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    track.alpha = kStartsInvisible;
    track.degrees = kSweepDegrees;
    track.lift_key = kWaveKey;
    track.radius_at_anchor = kRadiusAtAnchor;
    track.azimuth_degrees = kAzimuths[rang];
    track.tilt_degrees = kTilts[rang];
    // LE TYPE VA DANS LES QUATRE, et la figure le relit dans celle qu'elle
    // dessine — à la différence du carré teinté, qui prend le sien dans la
    // première pour les quatre.
    track.style = static_cast<std::int16_t>(pattern.type);
  }
  return tracks;
}

PooledEffectNode SowEffectTypedRibbonNode(const EffectTypedRibbonPattern& pattern, glm::vec3 anchor,
                                          std::int32_t effect_life_ticks) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kTypedRibbon;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = std::max(effect_life_ticks, kTypedRibbonShortestLifeTicks);
  pooled.resource_paths = EffectTypedRibbonImagePaths(pattern);
  pooled.tracks = SeedEffectTypedRibbonTracks(pattern);
  return pooled;
}

}  // namespace ro::render

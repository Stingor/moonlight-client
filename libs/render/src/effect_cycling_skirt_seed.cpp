#include "ro/render/effect_cycling_skirt_seed.h"

#include <format>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES DEUX JEUX D'IMAGES, et le style qui les sépare : jusqu'à un les bulles,
// au-delà et jusqu'à trois le dessus. Plus haut, le client n'en charge AUCUNE.
constexpr std::uint8_t kLastStyleOfBubbles = 1;
constexpr std::uint8_t kLastStyleWithImages = 3;
constexpr std::string_view kBubbleFormat = "data\\texture\\effect\\W_bubble{:02}.tga";
constexpr std::string_view kTopFormat = "data\\texture\\effect\\W_A_top{:02}.tga";

// LES DEUX STYLES QUI TEINTENT, un par jeu. Les autres laissent la variante à
// zéro.
constexpr std::uint8_t kStyleThatTintsBubbles = 1;
constexpr std::uint8_t kStyleThatTintsTop = 3;
constexpr std::uint8_t kTintedVariant = 4;

// CE QUE LE BÂTISSEUR POSE EN DUR SUR LES QUATRE PISTES.
constexpr std::int16_t kSweepDegrees = 360;
constexpr std::int16_t kTiltDegrees = 80;
constexpr float kRadiusAtAnchor = 4.1F;
// LES QUATRE AZIMUTS, à quatre-vingt-dix degrés d'écart, dans l'ordre des pistes.
constexpr std::array<std::int16_t, kEffectTracksPerNode> kAzimuths = {180, 270, 0, 90};

// ⚠ LES NOMS SE NUMÉROTENT À PARTIR DE UN : le client formate son nom avec
// l'indice de boucle PLUS UN.
constexpr std::size_t kFirstImageNumber = 1;

}  // namespace

std::span<const EffectCyclingSkirtPattern> EffectCyclingSkirtPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectCyclingSkirtPatternTable(), effect_id,
                           &EffectCyclingSkirtPattern::effect_id);
}

std::vector<std::string> EffectCyclingSkirtTexturePaths(std::uint8_t builder_argument) {
  std::vector<std::string> paths;
  if (builder_argument > kLastStyleWithImages) {
    // ⚠ LE CLIENT NE CHARGE RIEN AU-DELÀ DE TROIS, et son nœud reste donc sans
    // image. Lui en donner une par défaut montrerait quelque chose qu'il ne
    // montre pas.
    return paths;
  }
  const bool bulles = builder_argument <= kLastStyleOfBubbles;
  const std::size_t count = bulles ? kCyclingSkirtBubbleCount : kCyclingSkirtTopCount;
  paths.reserve(count);
  for (std::size_t rang = 0; rang < count; ++rang) {
    // ⚠ LE FORMAT NE PEUT PAS ÊTRE CHOISI À L'EXÉCUTION : `std::format` exige de
    // le connaître à la compilation. Les deux appels sont donc écrits en clair,
    // et c'est la seule façon de garder le contrôle de format du compilateur.
    paths.push_back(bulles ? std::format(kBubbleFormat, rang + kFirstImageNumber)
                           : std::format(kTopFormat, rang + kFirstImageNumber));
  }
  return paths;
}

std::uint8_t EffectCyclingSkirtVariantFor(std::uint8_t builder_argument) {
  return builder_argument == kStyleThatTintsBubbles || builder_argument == kStyleThatTintsTop
             ? kTintedVariant
             : 0;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectCyclingSkirtTracks(
    const EffectCyclingSkirtPattern& pattern) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    EffectTrack& track = tracks[rang];
    track.active = true;
    track.ticks = 0;
    track.degrees = kSweepDegrees;
    track.tilt_degrees = kTiltDegrees;
    track.radius_at_anchor = kRadiusAtAnchor;
    track.azimuth_degrees = kAzimuths[rang];
    track.lift_key = kCyclingSkirtLiftKeys[rang];
    // ⚠ LE STYLE DE LA PISTE EST L'OCTET DU SITE, et il n'est PAS ce que le
    // mouvement appelle un style : celui du corps commun est le vingt-deux, qui
    // monte moins vite. Aucun de nos octets ne vaut vingt-deux, si bien que le
    // mouvement prend partout son rythme ordinaire — et le dire est ce qui
    // permettra de s'en apercevoir le jour où un octet vaudrait vingt-deux.
    track.style = pattern.builder_argument;
    // ⚠ LES QUATRE ALPHAS DÉCROISSENT DE QUARANTE-CINQ, la dernière piste portant
    // celui du site. C'est cet écart qui distingue les quatre anneaux.
    track.alpha = static_cast<std::int16_t>(
        pattern.base_alpha +
        kCyclingSkirtAlphaStep * static_cast<std::int16_t>(tracks.size() - 1 - rang));
  }
  return tracks;
}

PooledEffectNode SowEffectCyclingSkirtNode(const EffectCyclingSkirtPattern& pattern,
                                           glm::vec3 anchor, std::int32_t effect_life_ticks) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kCyclingSkirt, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  // L'IMAGE NE CHANGE PAS D'ELLE-MÊME : le mouvement la pose depuis le compteur.
  pooled.node.frame_hold_ticks = 0;
  pooled.resource_paths = EffectCyclingSkirtTexturePaths(pattern.builder_argument);
  pooled.tracks = SeedEffectCyclingSkirtTracks(pattern);
  pooled.trail_variant = EffectCyclingSkirtVariantFor(pattern.builder_argument);
  return pooled;
}

}  // namespace ro::render

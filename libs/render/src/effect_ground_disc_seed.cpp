#include "ro/render/effect_ground_disc_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES TROIS STYLES DE NŒUD, à un près les uns des autres et pourtant distincts :
// c'est la variante de traînée, et la palette y lit trois teintes.
constexpr std::int32_t kStyleOfOrdinaryDisc = 110;
constexpr std::int32_t kStyleOfMapCircle = 111;
constexpr std::int32_t kStyleOfGlow = 112;

// LES DEUX RAYONS ÉCRITS EN DUR, et celui qui sert de repli quand l'instance
// n'impose rien.
constexpr float kNarrowRadius = 30.0F;
constexpr float kWideRadius = 40.0F;

// LES DEUX HAUTEURS. Négatives : le repère du client descend, donc le disque est
// AU-DESSUS de son ancre — d'un demi-point, ou de quatre dixièmes.
constexpr float kHeightAboveAnchor = -0.5F;
constexpr float kLowerHeightAboveAnchor = -0.4F;

// LES TROIS JEUX D'AZIMUT ET D'ALPHA. Trois cent cinquante-huit n'est pas zéro
// à deux degrés près : c'est un azimut de départ, et le disque qui le porte est
// aussi le plus pâle.
constexpr std::int16_t kAzimuthAtOrigin = 0;
constexpr std::int16_t kAzimuthNearFullTurn = 358;
constexpr std::int16_t kAlphaOpaque = 250;
constexpr std::int16_t kAlphaDim = 200;
constexpr std::int16_t kAlphaFaint = 50;

// LES OCTETS QUE LE CLIENT NOMME. Il les teste un à un, par égalité : ce ne sont
// pas des bornes, et un octet entre deux ne se déduit pas de ses voisins.
constexpr std::uint8_t kOctetDim = 0;
constexpr std::uint8_t kOctetWideDim = 2;
constexpr std::uint8_t kOctetWideFaint = 3;
constexpr std::uint8_t kOctetMapCircle = 4;
constexpr std::uint8_t kOctetGlow = 5;
constexpr std::uint8_t kOctetMapCircleBackwards = 14;

[[nodiscard]] bool ReadsRadiusFromInstance(std::uint8_t octet) {
  return octet == kOctetMapCircle || octet == kOctetMapCircleBackwards;
}

}  // namespace

std::span<const EffectGroundDiscPattern> EffectGroundDiscPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectGroundDiscPattern> table = EffectGroundDiscPatternTable();
  const auto first =
      std::ranges::lower_bound(table, effect_id, {}, &EffectGroundDiscPattern::effect_id);
  const auto last =
      std::ranges::upper_bound(table, effect_id, {}, &EffectGroundDiscPattern::effect_id);
  return table.subspan(static_cast<std::size_t>(first - table.begin()),
                       static_cast<std::size_t>(last - first));
}

EffectGroundDiscSeed EffectGroundDiscSeedFor(std::uint8_t builder_argument,
                                             float radius_from_instance) {
  // ⚠ LA STRUCTURE EST CELLE DU CLIENT, pas une table d'octets. Les sept octets
  // relevés ne sont pas les seuls que le bâtisseur sache traiter : ses branches
  // répondent à tous. Les énumérer donnerait une réponse inventée au huitième.
  EffectGroundDiscSeed seed;

  if (builder_argument == kOctetMapCircle || builder_argument == kOctetMapCircleBackwards) {
    seed.node_style = kStyleOfMapCircle;
  } else if (builder_argument == kOctetGlow) {
    seed.node_style = kStyleOfGlow;
  } else {
    seed.node_style = kStyleOfOrdinaryDisc;
  }

  // ⚠ LE CLIENT TESTE `(octet - 2) > 1` SANS SIGNE, ce qui désigne exactement
  // les octets deux et trois — et non « plus grand que trois ». Le lire signé
  // rangerait le zéro et le un du mauvais côté, et leur donnerait le rayon large.
  const bool est_large = builder_argument == kOctetWideDim || builder_argument == kOctetWideFaint;
  if (est_large) {
    seed.radius_at_anchor = kWideRadius;
  } else if (ReadsRadiusFromInstance(builder_argument)) {
    seed.radius_at_anchor = radius_from_instance > 0.0F ? radius_from_instance : kWideRadius;
  } else {
    seed.radius_at_anchor = kNarrowRadius;
  }

  if (builder_argument == kOctetMapCircle || builder_argument == kOctetGlow ||
      builder_argument == kOctetMapCircleBackwards) {
    seed.azimuth_degrees = kAzimuthAtOrigin;
    seed.alpha = kAlphaOpaque;
    seed.height = kHeightAboveAnchor;
  } else if (builder_argument == kOctetDim || builder_argument == kOctetWideDim) {
    seed.azimuth_degrees = kAzimuthAtOrigin;
    seed.alpha = kAlphaDim;
    seed.height = kHeightAboveAnchor;
  } else {
    seed.azimuth_degrees = kAzimuthNearFullTurn;
    seed.alpha = kAlphaFaint;
    seed.height = kLowerHeightAboveAnchor;
  }
  return seed;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGroundDiscTracks(
    const EffectGroundDiscPattern& pattern, float radius_from_instance) {
  const EffectGroundDiscSeed seed =
      EffectGroundDiscSeedFor(pattern.builder_argument, radius_from_instance);
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  // ⚠ UNE SEULE PISTE VIVE : le bâtisseur allume la première et ÉTEINT les trois
  // autres nommément. Les allumer toutes empilerait quatre disques identiques.
  EffectTrack& track = tracks.front();
  track.active = true;
  track.ticks = 0;
  track.alpha = seed.alpha;
  track.radius_at_anchor = seed.radius_at_anchor;
  track.azimuth_degrees = seed.azimuth_degrees;
  track.value[kSlotOfGroundDiscHeight] = seed.height;
  // ⚠ L'OCTET SE RANGE DANS LA PISTE, et c'est ce qui permet au mouvement de
  // décider du sens de rotation sans rien savoir du site d'appel.
  track.value[kSlotOfGroundDiscVariant] = static_cast<float>(pattern.builder_argument);
  return tracks;
}

PooledEffectNode SowEffectGroundDiscNode(const EffectGroundDiscPattern& pattern, glm::vec3 anchor,
                                         std::int32_t effect_life_ticks,
                                         float radius_from_instance) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kGroundDisc;
  pooled.ground_position = anchor;
  pooled.owner_position = anchor;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectGroundDiscTracks(pattern, radius_from_instance);
  // ⚠ LE STYLE VIT DANS LE NŒUD : la figure le lit à un déplacement que sa
  // boucle de pistes ne décale pas, si bien que les quatre le partagent.
  pooled.trail_variant = static_cast<std::uint8_t>(
      EffectGroundDiscSeedFor(pattern.builder_argument, radius_from_instance).node_style);
  return pooled;
}

}  // namespace ro::render

#include "ro/render/effect_concentric_square_seed.h"

#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES SEUILS DES HALOS.
constexpr std::int32_t kTwoHalosNeedCount = 10;
constexpr std::int32_t kTwoHalosNeedRemaining = 11;
constexpr std::int32_t kOneHaloNeedsCount = 5;
constexpr std::int32_t kOneHaloNeedsRemaining = 6;
constexpr std::uint8_t kMarkWithOneHalo = 20;
constexpr std::uint8_t kMarkWithTwoHalos = 21;

// LES OCTETS DE SITE QUI ONT LEUR BRANCHE : les sphères de l'enfant (1), les
// pièces (3 et 4), et les quatre éléments (10 à 13). Tout autre octet suit la
// branche des sphères du Monk.
constexpr std::uint8_t kChildStyle = 1;
constexpr std::uint8_t kFirstMoneyStyle = 3;
constexpr std::uint8_t kLastMoneyStyle = 4;
constexpr std::uint8_t kFirstElementStyle = 10;

// LA MARQUE DES PIÈCES, et celle, nulle, des sphères sans halo.
constexpr std::uint8_t kMoneyMark = 1;
constexpr std::uint8_t kPlainMark = 0;

// LA HAUTEUR, LE RAYON ET LA CLÉ DU FACTEUR de chaque branche.
constexpr float kChildHeight = -14.0F;
constexpr float kHeight = -15.0F;
constexpr float kChildRadius = 1.3F;
constexpr float kMoneyRadius = 1.0F;
constexpr float kRadius = 2.0F;
constexpr float kElementRadius = 6.0F;
constexpr float kChildFactorKey = 1.0F;

// LE PAS DU CAP D'UN RANG AU SUIVANT, et le tour entier que les pièces se
// partagent quand elles sont cinq au plus.
constexpr float kHeadingStep = 72.0F;
constexpr float kFullTurn = 360.0F;

[[nodiscard]] bool IsMoneyStyle(std::uint8_t style) {
  return style >= kFirstMoneyStyle && style <= kLastMoneyStyle;
}

// LE RAYON d'un nœud, selon sa branche et sa place dans la gerbe.
[[nodiscard]] float RadiusFor(std::int32_t raw_count, std::int32_t rank, std::uint8_t style) {
  if (style == kChildStyle) {
    return kChildRadius;
  }
  if (style >= kFirstElementStyle) {
    return kElementRadius;
  }
  if (IsMoneyStyle(style) &&
      !(raw_count > kOneHaloNeedsCount && raw_count - rank >= kOneHaloNeedsRemaining)) {
    return kMoneyRadius;
  }
  return kRadius;
}

// OÙ LE SEMIS ÉCRIT, dans les vingt et une cases.
constexpr std::size_t kSlotOfOrbit = 6;
constexpr std::size_t kSlotOfHeight = 7;
constexpr std::size_t kSlotOfBreath = 8;
constexpr std::size_t kSlotOfFactorKey = 10;

[[nodiscard]] float DrawAngleDegrees(std::uint32_t& rng_state) {
  return static_cast<float>(NextEffectNodeDraw(rng_state) %
                            static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees));
}

}  // namespace

std::span<const EffectConcentricSquarePattern> EffectConcentricSquarePatternsFor(
    std::uint32_t effect_id) {
  return core::RowsWithKey(EffectConcentricSquarePatternTable(), effect_id,
                           &EffectConcentricSquarePattern::effect_id);
}

std::uint8_t EffectConcentricSquareMarkFor(std::int32_t raw_count, std::int32_t rank,
                                           std::uint8_t style) {
  // L'ENFANT N'EN A AUCUNE, les pièces la leur, et un élément porte son octet
  // même quand le compte lui donnerait un halo.
  if (style == kChildStyle) {
    return kPlainMark;
  }
  if (IsMoneyStyle(style)) {
    return kMoneyMark;
  }
  if (style >= kFirstElementStyle) {
    return style;
  }
  // ⚠ LE COMPTE EMPLOYÉ ICI EST LE BRUT, non borné à cinq : c'est la boucle qui
  // est bornée, pas ce nombre. Les confondre ferait croire que ces deux marques
  // ne sont jamais posées, et l'on retirerait les halos de la figure.
  if (raw_count > kTwoHalosNeedCount && raw_count - rank >= kTwoHalosNeedRemaining) {
    return kMarkWithTwoHalos;
  }
  if (raw_count > kOneHaloNeedsCount && raw_count - rank >= kOneHaloNeedsRemaining) {
    return kMarkWithOneHalo;
  }
  return kPlainMark;
}

float EffectConcentricSquareHeadingFor(std::int32_t raw_count, std::int32_t rank,
                                       std::uint8_t style) {
  // CINQ PIÈCES AU PLUS se partagent le TOUR ENTIER ; partout ailleurs, le cap
  // avance de soixante-douze degrés par rang, quel que soit le compte.
  if (IsMoneyStyle(style) && raw_count > 0 && raw_count <= kOneHaloNeedsCount) {
    return kFullTurn * static_cast<float>(rank) / static_cast<float>(raw_count);
  }
  return kHeadingStep * static_cast<float>(rank);
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectConcentricSquareTracks(
    const EffectConcentricSquarePattern& pattern, std::int32_t raw_count, std::int32_t rank,
    std::uint32_t& generator) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& track = tracks[0];
  track.active = true;
  track.alpha = 0;

  // ⚠ SEPT ANGLES TIRÉS, ET L'ORDRE COMPTE : six pour les trois paires du
  // tremblement, un septième pour le souffle du rayon. Les tirer dans un autre
  // ordre donnerait une autre gerbe pour la même graine.
  for (std::size_t pair = 0; pair < 6; ++pair) {
    track.value[pair] = DrawAngleDegrees(generator);
  }

  track.value[kSlotOfOrbit] = EffectConcentricSquareHeadingFor(raw_count, rank, pattern.style);
  // ⚠ UN ÉLÉMENT TIRE EN PLUS UN DÉCALAGE DE SON CAP, avant le souffle : les
  // sphères d'un même élément ne s'alignent pas d'une gerbe à l'autre.
  if (pattern.style >= kFirstElementStyle) {
    track.value[kSlotOfOrbit] += DrawAngleDegrees(generator);
    if (track.value[kSlotOfOrbit] >= kFullTurn) {
      track.value[kSlotOfOrbit] -= kFullTurn;
    }
  }
  track.value[kSlotOfBreath] = DrawAngleDegrees(generator);

  track.mark[0] = EffectConcentricSquareMarkFor(raw_count, rank, pattern.style);
  track.value[kSlotOfHeight] = pattern.style == kChildStyle ? kChildHeight : kHeight;
  track.radius_at_anchor = RadiusFor(raw_count, rank, pattern.style);
  // ⚠ LA CLÉ DU FACTEUR N'EST POSÉE QUE PAR LA BRANCHE DE L'ENFANT, et c'est
  // elle que le mouvement compare à UN pour resserrer orbite et tremblement.
  track.value[kSlotOfFactorKey] = pattern.style == kChildStyle ? kChildFactorKey : 0.0F;
  return tracks;
}

PooledEffectNode SowEffectConcentricSquareNode(const EffectConcentricSquarePattern& pattern,
                                               std::int32_t raw_count, std::int32_t rank,
                                               glm::vec3 anchor, std::int32_t effect_life_ticks,
                                               std::uint32_t& generator) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kConcentricSquare, anchor);
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectConcentricSquareTracks(pattern, raw_count, rank, generator);
  return pooled;
}

}  // namespace ro::render

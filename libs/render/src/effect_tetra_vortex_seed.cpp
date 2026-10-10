#include "ro/render/effect_tetra_vortex_seed.h"

#include <algorithm>
#include <string>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES CASES QUE LA FIGURE DE L'ARC LIT : la marque zéro est l'image, la deux la parure, la case
// huit l'angle qui fait respirer le rayon, la dix le jeu d'écarts.
constexpr std::size_t kMarkOfFrame = 0;
constexpr std::size_t kMarkOfTint = 2;
constexpr std::size_t kSlotOfSpinDegrees = 8;
constexpr std::size_t kSlotOfShape = 10;

// L'UNIQUE PISTE VIVE : le bâtisseur éteint les quatre, puis rallume la
// première.
constexpr std::size_t kLivingTrack = 0;

// LES ÉCRITURES QUE PERSONNE NE LIT, ni le mouvement ni le dessinateur : un
// zéro, l'ordonnée de l'instance moins dix, un tirage `rand() % 360` et l'octet
// du site. Aucune n'atteint le pixel ; le tirage ne se rejoue donc pas, et le
// semis n'a pas besoin de générateur.

// UNE PARURE que le bâtisseur pose pour un identifiant.
struct TintOfEffect {
  std::uint32_t effect_id = 0;
  std::uint8_t tint = 0;
};

constexpr std::array<TintOfEffect, 4> kTints = {{
    {970, 20},
    {971, 18},
    {972, 17},
    {973, 19},
}};

// Ce qu'un nœud neuf porte dans sa marque de parure quand le bâtisseur n'y écrit rien : voir
// `EffectTetraVortexTintFor`.
constexpr std::uint8_t kUnwrittenTint = 0;

[[nodiscard]] const TintOfEffect* TintEntryFor(std::uint32_t effect_id) {
  const auto found = std::ranges::find(kTints, effect_id, &TintOfEffect::effect_id);
  return found == kTints.end() ? nullptr : &*found;
}

}  // namespace

std::uint8_t EffectTetraVortexTintFor(std::uint32_t effect_id) {
  const TintOfEffect* entry = TintEntryFor(effect_id);
  return entry == nullptr ? kUnwrittenTint : entry->tint;
}

bool EffectTetraVortexTintIsWritten(std::uint32_t effect_id) {
  return TintEntryFor(effect_id) != nullptr;
}

std::span<const EffectTetraVortexPattern> EffectTetraVortexPatternsFor(std::uint32_t effect_id) {
  const std::span<const EffectTetraVortexPattern> table = EffectTetraVortexPatternTable();
  const auto [first, last] =
      std::ranges::equal_range(table, effect_id, {}, &EffectTetraVortexPattern::effect_id);
  return {first, last};
}

std::array<EffectTrack, kEffectTracksPerNode> SeedTetraVortexTracks(std::uint32_t effect_id,
                                                                    glm::vec3 target_position) {
  // Les quatre pistes naissent éteintes, et leurs marques à zéro.
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& living = tracks[kLivingTrack];
  living.ticks = 0;
  living.active = true;
  living.radius_at_anchor = kTetraVortexRadius;
  living.mark[kMarkOfTint] = EffectTetraVortexTintFor(effect_id);
  living.degrees = kTetraVortexFirstCornerDegrees;
  living.value[kSlotOfShape] = kTetraVortexShapeKey;
  living.value[kSlotOfSpinDegrees] = 0.0F;
  living.alpha = kTetraVortexBirthAlpha;
  // ⚠ LE POINT VISÉ, ÉLEVÉ DE TRENTE-CINQ : le client retranche à son ordonnée
  // qui descend.
  living.anchor = target_position + glm::vec3{0.0F, kTetraVortexLiftAboveTarget, 0.0F};
  living.mark[kMarkOfFrame] = 0;
  return tracks;
}

std::optional<PooledEffectNode> SowTetraVortexNode(std::uint32_t effect_id,
                                                   glm::vec3 owner_position,
                                                   const std::optional<glm::vec3>& target_position,
                                                   std::int32_t effect_life_ticks) {
  if (!target_position.has_value()) {
    return std::nullopt;
  }
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kTetraVortex;
  pooled.ground_position = owner_position;
  pooled.owner_position = owner_position;
  pooled.node.render_word = kTetraVortexRenderWord;
  pooled.node.life_ticks = std::max(effect_life_ticks, 1);
  pooled.resource_paths.reserve(kTetraVortexFrames.size());
  for (const std::string_view image : kTetraVortexFrames) {
    pooled.resource_paths.emplace_back(image);
  }
  pooled.tracks = SeedTetraVortexTracks(effect_id, *target_position);
  return pooled;
}

}  // namespace ro::render

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA GERBE, sous-type 146.
//
// DEUX bâtisseurs — la gerbe dirigée pour 832 et 943, la triple gerbe pour
// 1678 — et QUATRE lignes
// de table pour trois identifiants : le troisième appelle son bâtisseur DEUX
// FOIS de suite, sur deux images différentes.
//
// ⚠ AUCUN TIRAGE : les deux bâtisseurs posent tout en dur, et leur seul nombre
// venu d'ailleurs est le CAP VERS LA CIBLE.

// COMBIEN DE TICS séparent deux pistes, et le balayage qu'elles partagent.
inline constexpr std::int32_t kAnchoredSkirtDelayPerTrack = 8;
inline constexpr std::int16_t kAnchoredSkirtSweepDegrees = 315;

// DE COMBIEN LE CAP DE LA GERBE TOURNE par rapport à sa cible.
inline constexpr std::int16_t kAnchoredSkirtHeadingOffset = 195;

// DE COMBIEN LE RAYON DÉCROÎT d'une piste à la suivante, et l'inclinaison de la
// première — cinq fois quatorze — qui perd cinq degrés par piste.
inline constexpr float kAnchoredSkirtRadiusPerTrack = 0.1F;
inline constexpr std::int16_t kAnchoredSkirtTiltPerTrack = 5;
inline constexpr std::int16_t kAnchoredSkirtFirstTiltStep = 14;

// UNE GERBE qu'un effet lance.
struct EffectAnchoredSkirtPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  // LES IMAGES DU NŒUD, dans l'ordre où elles défilent. TREIZE pour les bulles,
  // UNE pour les ondes.
  std::span<const std::string_view> textures;
  // LA LONGUEUR DU RUBAN, que la piste appelle sa « clé » (ADR 0017).
  float ribbon_length = 0.0F;
  // LE RAYON DE LA PREMIÈRE PISTE ; les trois autres perdent un dixième chacune.
  float first_radius = 0.0F;
  // LA VARIANTE DE TEINTE, que le dessinateur recopie dans le nœud.
  float trail_variant = 0.0F;
  EffectNodeSchedule schedule;
};

[[nodiscard]] std::span<const EffectAnchoredSkirtPattern> EffectAnchoredSkirtPatternTable();

[[nodiscard]] std::span<const EffectAnchoredSkirtPattern> EffectAnchoredSkirtPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, toutes vives et échelonnées.
//
// `toward_target` est le vecteur du nœud vers la cible du sort. `render` ne
// connaît pas cette cible (CLAUDE.md) : l'appelant la donne, comme il la donne
// à l'essaim.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectAnchoredSkirtTracks(
    const EffectAnchoredSkirtPattern& pattern, glm::vec3 toward_target);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectAnchoredSkirtNode(const EffectAnchoredSkirtPattern& pattern,
                                                          glm::vec3 anchor, glm::vec3 toward_target,
                                                          std::int32_t effect_life_ticks);

}  // namespace ro::render

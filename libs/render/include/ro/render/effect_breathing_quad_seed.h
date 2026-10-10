#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_breathing_quad.h"
#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU CARRÉ QUI RESPIRE, et les effets qui l'emploient par le
// sous-type 39.
//
// Un seul bâtisseur : une image, un octet, et UNE seule piste vive.
//
// ⚠ L'OCTET CHOISIT QUATRE VALEURS D'UN COUP — rayon, azimut, alpha, et le champ
// de style du nœud — et ces quatre-là ne suivent aucune règle commune : le
// client les écrit branche par branche. Les déduire l'une de l'autre donnerait
// six carrés au même rayon.
//
// ⚠ ET TROIS OCTETS POSENT UNE INCLINAISON FIXE au lieu de la tirer. Comme c'est
// l'inclinaison qui mène le souffle du rayon, ces trois-là ne respirent pas au
// premier tic : leur carré part à son rayon plein.

// UN CARRÉ QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectBreathingQuadPattern {
  std::uint32_t effect_id = 0;
  // L'étiquette du site d'appel qui pose ce carré.
  std::string_view client_function;
  std::string_view texture;
  std::uint8_t builder_argument = 0;
};

// Tous les carrés connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectBreathingQuadPattern> EffectBreathingQuadPatternTable();

// TOUS les carrés d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectBreathingQuadPattern> EffectBreathingQuadPatternsFor(
    std::uint32_t effect_id);

// CE QU'UN OCTET POSE. Rendu d'un bloc parce que le client le décide d'un bloc.
struct EffectBreathingQuadSeed {
  float radius_at_anchor = 0.0F;
  std::int16_t azimuth_degrees = 0;
  std::int16_t alpha = 0;
  // Le champ de style du NŒUD, que la figure relit.
  std::int32_t node_style = 0;
  // ⚠ VRAI quand l'inclinaison est POSÉE au lieu d'être tirée. Trois octets sur
  // sept sont dans ce cas.
  bool tilt_is_fixed = false;
};

[[nodiscard]] EffectBreathingQuadSeed EffectBreathingQuadSeedFor(std::uint8_t builder_argument);

// LES QUATRE PISTES D'UN CARRÉ : une seule vive, et son inclinaison tirée — sauf
// pour trois octets, qui la posent.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectBreathingQuadTracks(
    const EffectBreathingQuadPattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, piste semée, prêt à rejoindre la troupe.
//
// ⚠ SON STYLE VIT DANS LE NŒUD, pas dans la piste : c'est là que la figure le
// lit. Il est rangé dans la variante de traînée, qui porte ce champ.
[[nodiscard]] PooledEffectNode SowEffectBreathingQuadNode(const EffectBreathingQuadPattern& pattern,
                                                          glm::vec3 anchor,
                                                          std::int32_t effect_life_ticks,
                                                          std::uint32_t& generator);

}  // namespace ro::render

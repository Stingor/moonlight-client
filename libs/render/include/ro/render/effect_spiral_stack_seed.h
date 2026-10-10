#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_spiral_stack.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA PILE EN SPIRALE, et les sept effets qui l'emploient par le
// sous-type 103.
//
// Une image, un octet, et UNE SEULE piste vive sur quatre.
//
// ⚠ L'OCTET NE FAIT PAS QUE CHOISIR UNE COULEUR. Pour la plupart des valeurs il
// ne pose que le style, mais l'une d'elles refait tout le gabarit — quatorze
// étages au lieu de dix, un rayon et un sommet plus grands, et AUCUN
// rétrécissement. Traiter l'octet comme une simple teinte donnerait à cette
// valeur-là la silhouette des six autres.
//
// ⚠ ET LA PILE NAÎT INVISIBLE : l'alpha part à zéro et monte d'une unité par
// tic pendant quarante tics. C'est le mouvement qui l'allume.

// UNE PILE QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectSpiralStackPattern {
  std::uint32_t effect_id = 0;
  // LA FONCTION DU CLIENT qui pose cette pile.
  std::string_view client_function;
  std::string_view texture;
  // L'octet : le style, et pour une valeur le gabarit tout entier.
  std::uint8_t style = 0;
};

// Toutes les piles connues, par identifiant d'effet.
[[nodiscard]] std::span<const EffectSpiralStackPattern> EffectSpiralStackPatternTable();

// TOUTES les piles d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectSpiralStackPattern> EffectSpiralStackPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UNE PILE : une seule vive, et son azimut TIRÉ au sort.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSpiralStackTracks(
    const EffectSpiralStackPattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, piste semée, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectSpiralStackNode(const EffectSpiralStackPattern& pattern,
                                                        glm::vec3 anchor,
                                                        std::int32_t effect_life_ticks,
                                                        std::uint32_t& generator);

}  // namespace ro::render

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE GRAIN QUI TOMBE, ET SON HALO : le dessinateur du sous-type 157.
//
// Il sort par la voie A DEUX fois par piste vive : un carré teinté, puis le
// même agrandi d'un dixième et peint d'un rouge presque pur. C'est ce second
// carré qui fait le halo, et le peindre de la même teinte que le premier
// l'effacerait.
//
// ⚠ SES QUATRE COINS SONT UN CARRÉ, aux mêmes quatre index fixes que la colonne
// d'aura — quarante-cinq, cent trente-cinq, deux cent vingt-cinq, trois cent
// quinze. Le client ne lit pas quatre tables : il lit ses tables de sinus et de
// cosinus à ces quatre index fixes.
//
// ⚠ ET LE PREMIER CARRÉ NE PARAÎT QUE POUR TROIS MARQUES. Son `switch` couvre
// zéro, un et deux, et n'a PAS de cas par défaut : une marque au-delà n'émet que
// le halo. Lui donner une teinte par défaut montrerait un grain que le client ne
// montre pas.

// DE COMBIEN LE HALO EST PLUS GRAND que le grain qu'il entoure.
inline constexpr float kRainParticleHaloScale = 1.1F;

// LA MARQUE QUI CHOISIT LA TEINTE DU GRAIN, et son sens appartient à cette figure
// (ADR 0017) — le semis la tire au sort entre trois valeurs.
inline constexpr std::size_t kMarkOfRainParticleTint = 0;

// LE RAYON DU DISQUE OÙ LE GRAIN SE POSE, et c'est le MOUVEMENT qui le lit — la
// figure, elle, ne connaît que l'ancre déjà calculée. Il vit ici parce que le
// semis et le mouvement doivent le nommer pareil.
inline constexpr std::size_t kSlotOfRainParticleOrbitRadius = 0;

// LA TEINTE DU HALO, la même pour tous : un rouge presque pur.
inline constexpr std::array<std::uint8_t, 3> kRainParticleHaloTint = {255, 1, 1};

// LA TEINTE QU'UNE MARQUE DONNE AU GRAIN, ou rien pour une marque que le client
// ne nomme pas — le grain ne paraît alors pas du tout.
[[nodiscard]] bool EffectRainParticleTintFor(std::uint8_t mark, std::array<std::uint8_t, 3>& tint);

// LES QUADS D'UNE PISTE : le grain puis son halo, ou le halo seul.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeRainParticleQuads(
    const EffectTrack& track, std::int16_t turn_degrees, std::int16_t tilt_degrees,
    glm::vec3 node_origin, const EffectOwnerTint& owner = {});

}  // namespace ro::render

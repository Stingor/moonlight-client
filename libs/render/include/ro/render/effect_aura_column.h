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

// LA COLONNE D'AURA : dix carrés emboîtés, du plus large au plus étroit.
//
// Le dessinateur du sous-type 131. Il sort par la voie A, jusqu'à dix fois par
// piste vive.
//
// ⚠ SES QUATRE COINS SONT UN CARRÉ : les tables de sinus et de cosinus lues à
// quatre index FIXES — quarante-cinq, cent trente-cinq, deux cent vingt-cinq et
// trois cent quinze. Aucune table propre à la colonne n'entre en jeu.
//
// ⚠ ET CHAQUE COUCHE A SON PROPRE ALPHA. La piste en porte dix, un par couche, et
// c'est le mouvement qui les fait monter puis redescendre à des vitesses et des
// plafonds différents. Une couche dont l'alpha est nul ne s'émet PAS — la colonne
// s'ouvre donc couche par couche, de la plus large à la plus étroite.
//
// ⚠ LA PLUS LARGE EST AUSSI LA PLUS CLAIRE, et de loin : cinquante-cinq de rouge
// et de vert contre cinq pour les neuf autres. C'est elle qui donne son bord à la
// colonne ; les peindre toutes pareil en ferait une masse.

// COMBIEN DE COUCHES, et de combien leur rayon décroît. Le pas vaut UN pour une
// marque, TROIS pour toutes les autres.
inline constexpr std::size_t kAuraColumnLayers = 10;
inline constexpr float kAuraColumnWideStep = 3.0F;
inline constexpr float kAuraColumnTightStep = 1.0F;

// LA MARQUE QUI SERRE LA COLONNE, et qui la remonte de dix unités de plus.
inline constexpr std::size_t kMarkOfAuraColumnShape = 4;
inline constexpr std::uint8_t kAuraColumnTightMark = 2;
inline constexpr float kAuraColumnTightRise = 40.0F;
inline constexpr float kAuraColumnWideRise = 30.0F;

// LA MARQUE QUI CHOISIT L'IMAGE, et la case qui décale la colonne sur un axe.
inline constexpr std::size_t kMarkOfAuraColumnImage = 0;
inline constexpr std::size_t kSlotOfAuraColumnShift = 0;

// LES DEUX TEINTES : celle de la couche la plus large, et celle des neuf autres.
inline constexpr std::array<std::uint8_t, 3> kAuraColumnOuterTint = {55, 55, 255};
inline constexpr std::array<std::uint8_t, 3> kAuraColumnInnerTint = {5, 5, 255};

// LES QUADS D'UNE PISTE, de la couche la plus large à la plus étroite, ou un lot
// VIDE. Une couche dont l'alpha est nul n'en fait pas partie.
//
// `turn_degrees` et `tilt_degrees` sont les deux angles que le mouvement a rangés
// dans les styles des deux premières pistes.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeAuraColumnQuads(
    const EffectTrack& track, std::int16_t turn_degrees, std::int16_t tilt_degrees,
    glm::vec3 node_origin, const EffectOwnerTint& owner = {});

}  // namespace ro::render

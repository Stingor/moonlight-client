#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES ICÔNES QUI FLOTTENT : le sous-type 123.
//
// Le mouvement de ce sous-type ; la figure est celle du carré teinté —
// `ComposeTintedRotatedQuad`.
//
// Chaque piste est une icône qui tourne sur elle-même d'un degré par tic, dérive
// lentement dans le plan, s'allume sur cinquante tics et s'éteint sur les
// cinquante derniers de sa vie. Les quatre pistes vivent chacune pour soi : ce
// mouvement boucle sur les quatre, à la différence de la traîne du 116.

// L'ALPHA : plus cinq tant que le compteur est sous cinquante, moins cinq dès
// qu'il DÉPASSE le seuil que le bâtisseur range dans le cap.
//
// ⚠ AUCUN PLANCHER NI PLAFOND : les deux pas s'appliquent au même tic quand les
// deux gardes se recouvrent, et l'alpha peut passer sous zéro — la figure ne
// dessine alors plus rien.
inline constexpr std::int32_t kDriftingIconFirstTickPastRise = 50;
inline constexpr std::int16_t kDriftingIconAlphaStep = 5;

// LES DEUX ANGLES QUI COURENT VERS LEUR CIBLE : deux couples (valeur, cible),
// cases zéro et un, deux et trois. La valeur fait un pas d'un degré vers sa
// cible, bornée à [0, 359].
inline constexpr std::size_t kDriftingIconChasingPairs = 2;
inline constexpr std::size_t kDriftingIconSlotsPerPair = 2;
inline constexpr float kDriftingIconChaseStep = 1.0F;
inline constexpr float kDriftingIconChaseCeiling = 359.0F;

// LA DÉRIVE : un dixième du SINUS de chaque valeur courante. ⚠ DEUX SINUS,
// AUCUN COSINUS : la première valeur pousse l'ancre en x, la seconde en
// profondeur.
inline constexpr float kDriftingIconDriftFactor = 0.1F;

// L'ANGLE PROPRE que la figure lit pour faire respirer son rayon : plus un
// degré par tic, replié d'un tour par soustraction.
inline constexpr std::size_t kDriftingIconSlotOfSpinDegrees = 8;

// L'ANGLE DU CARRÉ : plus un degré par tic, replié d'un tour.
inline constexpr std::int16_t kDriftingIconTurnPerTick = 1;

// Avance les quatre pistes d'un tic, et range les deux angles de caméra.
//
// ⚠ LES ANGLES DE CAMÉRA SONT RANGÉS AVANT LA BOUCLE, dans le style des pistes
// zéro et un : le carré teinté les y relit.
//
// ⚠ LE GÉNÉRATEUR AVANCE seulement quand une valeur atteint ou dépasse sa cible
// et qu'elle n'est pas nulle : `rand()` n'est pas appelé pour une valeur qui
// tronque à zéro.
void AdvanceDriftingIconTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees,
                               std::uint32_t& generator);

}  // namespace ro::render

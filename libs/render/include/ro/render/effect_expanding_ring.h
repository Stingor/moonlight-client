#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ANNEAU QUI S'ÉLARGIT ET RENAÎT : le mouvement du sous-type 169.
//
// Le cas 169 du répartiteur de mouvement fait tout le travail EN LIGNE, sans
// appeler de fonction. Sa figure est le ruban de traînée
// (`ComposeEffectTrackRibbon`) : onze points, pas de soulèvement.
//
// ⚠ C'EST LE CORPS COMMUN DES CAS 30, 139 ET 144 — ET CE N'EST PAS LUI. L'onde
// est la même, constante pour constante : la longueur de chaque segment vaut la
// clé, plus trois dixièmes de la clé fois le sinus de neuf degrés par segment
// fois le sinus d'une phase que le rang choisit entre deux formules. Tout le
// reste diffère, et chaque écart est une constante du cas :
//
//   - LE RAYON CROÎT DE QUATRE CENTIÈMES par tic, pas de sept ;
//   - LE SEUIL N'EST PAS UN NOMBRE MAIS LA BASE DE LA PISTE PLUS UN :
//     la première case de QUEUE, que le bâtisseur
//     pose au rayon de naissance avant son tirage. Chaque piste s'efface donc à
//     une unité au-delà de SON départ, non à quatre du centre ;
//   - L'ALPHA TOMBE DE DEUX et monte de dix en plafonnant à DEUX CENTS — le
//     corps commun ne plafonne pas ;
//   - LA RENAISSANCE REMET LE RAYON À CETTE BASE et l'inclinaison à
//     soixante-quatorze, SANS condition sur la vie qui
//     reste et SANS style qui change la branche : le cas ne lit ni l'une ni
//     l'autre ;
//   - AUCUNE ATTENTE : le cas ne compare pas le compteur à zéro. Le bâtisseur
//     le pose à zéro, si bien que la garde du corps commun ne changerait rien ici
//     — mais la prêter dirait une règle que le client n'a pas.
//
// ⚠ ET L'ORDRE EST CELUI DU CLIENT : compteur, rayon, inclinaison (dont le
// plancher éteint l'alpha), PUIS l'alpha contre le rayon, PUIS l'onde. Poser
// l'alpha avant le plancher laisserait une image allumée à dix degrés.
//
// Une piste éteinte n'avance pas.
void AdvanceExpandingRingTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LA CASE DE QUEUE où le bâtisseur range le rayon de naissance, et où le
// mouvement lit son seuil et sa valeur de renaissance : la première des neuf
// cases de queue.
inline constexpr std::size_t kExpandingRingTailOfBaseRadius = 0;

// CE QUE LE CAS 169 POSE EN DUR, une constante chacun.
inline constexpr float kExpandingRingGrowthPerTick = 0.04F;
inline constexpr float kExpandingRingFadeBeyondBase = 1.0F;
inline constexpr float kExpandingRingUndulationAmplitude = 0.3F;
inline constexpr std::int16_t kExpandingRingTiltFallPerTick = 1;
inline constexpr std::int16_t kExpandingRingTiltFloorDegrees = 10;
inline constexpr std::int16_t kExpandingRingTiltAtRebirthDegrees = 74;
inline constexpr std::int16_t kExpandingRingAlphaCeiling = 200;
inline constexpr std::int16_t kExpandingRingAlphaRisePerTick = 10;
inline constexpr std::int16_t kExpandingRingAlphaFallPerTick = 2;

}  // namespace ro::render

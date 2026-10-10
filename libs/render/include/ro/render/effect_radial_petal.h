#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES PÉTALES QUI S'ÉCARTENT, sous-type 94 : la figure et le mouvement.
//
// FIGURE — dessinée pour le seul sous-type 94. Pour chaque piste allumée : un
// carré à plat, ses coins aux index 45, 135, 225 et 315 des tables de cosinus
// et de sinus, de la demi-diagonale que porte la piste, redressé par les deux
// angles de panneau, puis translaté par la position de la piste. Il sort par la
// voie A : alpha de la piste, mode de mélange UN, image zéro, coordonnées
// RELATIVES au nœud, blanc pur, et les quatre nombres de texture 0, 1, 0, 1 —
// les MÊMES que le grain qui dérive, dont la transcription les lit comme une
// image RETOURNÉE : un aux deux premiers coins, zéro aux deux derniers.
//
// ⚠ CE N'EST PAS LE GRAIN QUI DÉRIVE (`effect_drifting_mote.h`), bien que les
// deux se lisent aux mêmes cases et naissent dans le même cas : le grain teinte
// par sa marque, se tait au-dessus du nœud et mélange en mode zéro. Ici, aucune
// de ces trois choses.
//
// MOUVEMENT — le cas 94 du répartiteur des nœuds, écrit EN LIGNE, sans
// fonction à lui. Voir `AdvanceRadialPetalTracks`.

// LE CARRÉ DE DÉPART : quarante-cinq degrés, puis un quart de tour par coin.
inline constexpr std::int32_t kRadialPetalFirstCornerDegrees = 45;
inline constexpr std::int32_t kRadialPetalDegreesPerCorner = 90;

// OÙ VIT LA POSITION de la piste, dans la queue, en repère du client : la
// deuxième DESCEND. Ce sont les mêmes cases que le grain qui dérive.
inline constexpr std::size_t kRadialPetalSlotOfSideways = 3;
inline constexpr std::size_t kRadialPetalSlotOfDepthBelow = 4;
inline constexpr std::size_t kRadialPetalSlotOfForward = 5;

// LE MODE DE MÉLANGE que le dessinateur pousse : le mélange par l'alpha de la
// source, et non l'addition du mode zéro. Voir `EffectParticleBlend`.
inline constexpr std::int16_t kRadialPetalBlendMode = 1;

// COMBIEN DE TICS la piste file vers l'extérieur ; au-delà elle s'arrête où
// elle est.
inline constexpr std::int32_t kRadialPetalDriftTicks = 90;

// L'AMPLITUDE DU SAUT : la vitesse multipliée par cinquante.
inline constexpr float kRadialPetalHopScale = 50.0F;

// LE SAUT FAIT DEUX DEGRÉS PAR TIC : un demi-tour de sinus sur les
// quatre-vingt-dix tics de la course.
inline constexpr std::int32_t kRadialPetalHopDegreesPerTick = 2;

// L'ALPHA : il monte de deux par tic jusqu'au vingt-cinquième, se tient, puis
// perd un par tic passé le soixante-dixième.
inline constexpr std::int32_t kRadialPetalRisesUntilTick = 25;
inline constexpr std::int16_t kRadialPetalAlphaRise = 2;
inline constexpr std::int32_t kRadialPetalFadesAfterTick = 70;
inline constexpr std::int16_t kRadialPetalAlphaFall = 1;

// LE MOUVEMENT DU SOUS-TYPE 94, d'un tic.
//
// ⚠ LES DEUX ANGLES DE PANNEAU SE RANGENT D'ABORD, hors de la boucle et sur les
// deux premières pistes même éteintes. Ils arrivent ici déjà complétés — le
// client écrit `360 − cap` et `450 − inclinaison` replié —, et se rangent tels
// quels.
//
// Puis, pour chaque piste ALLUMÉE :
//   - le compteur avance ; tant qu'il n'est pas positif, la piste ne fait RIEN
//     d'autre — c'est le retard du semis ;
//   - jusqu'au quatre-vingt-dixième tic, la piste file le long de son cap à sa
//     vitesse, et sa hauteur vaut la hauteur de base moins
//     `vitesse · sin(2 · tic) · 50` : un seul saut, en repère du client, donc
//     VERS LE HAUT ;
//   - l'alpha monte, se tient puis retombe, borné à zéro, et SANS borne haute :
//     il plafonne de lui-même à cinquante.
void AdvanceRadialPetalTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int16_t billboard_turn_degrees,
                              std::int16_t billboard_tilt_degrees);

// LE QUAD D'UNE PISTE, ou rien si elle est éteinte.
//
// `turn_degrees` et `tilt_degrees` sont les styles des pistes zéro et un, où le
// mouvement les a rangés. `node_origin` est l'origine du nœud dans NOTRE repère.
//
// ⚠ AUCUNE GARDE D'ALPHA ni de hauteur : le client émet le carré d'une piste
// allumée même à alpha nul, et c'est la sortie qui le rend invisible.
[[nodiscard]] std::optional<EffectWorldQuad> ComposeRadialPetalQuad(const EffectTrack& track,
                                                                    std::int16_t turn_degrees,
                                                                    std::int16_t tilt_degrees,
                                                                    glm::vec3 node_origin);

}  // namespace ro::render

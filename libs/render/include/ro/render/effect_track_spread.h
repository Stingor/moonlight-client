#pragma once

#include <array>
#include <cstdint>

#include "ro/render/effect_quad_burst.h"
#include "ro/render/effect_track_fwd.h"

// LES DEUX MOUVEMENTS QUI FONT POURSUIVRE DES ANGLES.
//
// ⚠ ILS SONT SORTIS D'`effect_track_burst.cpp` AU PLAFOND DE HUIT CENTS
// LIGNES, comme celui-ci l'était d'`effect_track.cpp`. Mais la coupure a
// trouvé une frontière réelle : ces deux-là — la RAFALE et le GRAIN — font
// dériver leurs particules en suivant des PAIRES D'ANGLES qui poursuivent une
// cible, et c'est la même poursuite chez les deux, au nombre de paires près.
// Aucun autre mouvement du lot ne fait cela.
//
// La rafale fait vivre SEIZE particules par nœud et le grain UNE ; ce qu'ils
// partagent n'est donc pas leur taille mais leur manière.

namespace ro::render {

// COMBIEN DE PAIRES D'ANGLES une piste entretient, et ce qu'elles font.
//
// ⚠ HUIT PAIRES POUR QUATRE PARTICULES : chacune en emploie DEUX, et chaque
// paire est une poursuite — un angle courant et sa cible. Le courant descend
// de DEUX vers une cible plus basse, ou monte de TROIS vers une plus haute, et
// dès qu'il l'atteint une nouvelle cible est tirée. Les lire comme seize
// angles indépendants supprimerait la dérive, qui est tout le mouvement.
inline constexpr std::size_t kQuadBurstAnglePairs = 8;
inline constexpr float kQuadBurstAngleFall = 2.0F;
inline constexpr float kQuadBurstAngleRise = 3.0F;
inline constexpr float kQuadBurstAngleCeiling = 359.0F;

// LES DEUX PAS DE DÉRIVE, en unités du monde par tic.
//
// ⚠ LA MARQUE QUINZE DÉRIVE MOINS VITE QUE LES AUTRES : un dixième contre
// quinze centièmes. C'est le seul écart entre sa dérive et celle des marques
// ordinaires, et il est mesuré.
inline constexpr float kQuadBurstFadingDrift = 0.1F;
inline constexpr float kQuadBurstOrdinaryDrift = 0.15F;

// OÙ LA PARTICULE RENAÎT, et de combien elle s'écarte en renaissant.
inline constexpr float kQuadBurstRebirthFloor = -30.0F;
inline constexpr float kQuadBurstRisingFloor = -40.0F;
inline constexpr std::int32_t kQuadBurstRebirthHeightSpread = 100;
inline constexpr std::int32_t kQuadBurstGreyScatterSpread = 15;
inline constexpr std::int32_t kQuadBurstGreyScatterBias = 8;
inline constexpr std::int32_t kQuadBurstWideScatterSpread = 35;
inline constexpr std::int32_t kQuadBurstWideScatterBias = 17;

// DE COMBIEN LA MARQUE QUINZE S'USE, et où elle se remet à zéro.
inline constexpr std::uint8_t kQuadBurstFadeStep = 5;
inline constexpr std::uint8_t kQuadBurstFadeCeiling = 120;
inline constexpr std::int32_t kQuadBurstFadeTriggerDistance = 10;

// CE QUI DIFFÈRE D'UNE PARTICULE À L'AUTRE.
//
// ⚠ LES QUATRE NE DÉRIVENT PAS DANS LE MÊME SENS, et les signes ne suivent
// aucune règle : la première s'écarte des deux côtés, la deuxième rentre des
// deux, la troisième et la quatrième font chacune moitié-moitié. La marque
// QUATORZE, elle, a ses propres signes, qui ne sont ceux d'aucune des deux
// autres pour la première et la deuxième particule.
//
// ⚠ ET LE SEUIL DE RENAISSANCE DE LA MARQUE ONZE N'EST PAS LE MÊME sur la
// première particule : MOINS CINQUANTE chez elle, moins trente chez les trois
// autres. Les aligner ferait renaître la première vingt unités trop tôt.
//
// Ces cinq nombres sont en TABLE et non en quatre blocs parce qu'ils sont
// exactement ce qui distingue les particules : tout le reste est identique.
struct EffectQuadBurstDrift {
  float rising_sideways = 1.0F;
  float rising_forward = -1.0F;
  float drift_sideways = 1.0F;
  float drift_forward = 1.0F;
  float grey_rebirth_floor = kQuadBurstRebirthFloor;
};

inline constexpr std::array<EffectQuadBurstDrift, kQuadBurstParticles> kQuadBurstDrifts = {{
    {1.0F, -1.0F, 1.0F, 1.0F, -50.0F},
    {1.0F, -1.0F, -1.0F, -1.0F, kQuadBurstRebirthFloor},
    {1.0F, -1.0F, 1.0F, -1.0F, kQuadBurstRebirthFloor},
    {-1.0F, 1.0F, -1.0F, 1.0F, kQuadBurstRebirthFloor},
}};

// LA RAFALE QUI DÉRIVE : le mouvement des sous-types 29 et 80.
//
// Pour chaque piste vive :
//
//   - les HUIT paires d'angles poursuivent leur cible ;
//   - puis chacune des QUATRE particules dérive selon la marque du nœud :
//     la quatorze monte et se recale en haut, la seize monte en se dispersant
//     au sort, la quinze dérive à plat et s'use, et toutes les autres
//     descendent et RENAISSENT en haut quand elles sont tombées trop bas.
//
// ⚠ LES DEUX ANGLES DE CAMÉRA SE RANGENT DANS LES CHAMPS DE STYLE des pistes
// zéro et un, une seule fois par tic et non par piste. `render` ne connaît pas
// la caméra (CLAUDE.md) : l'appelant les donne.
void AdvanceQuadBurstTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                            std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees,
                            std::uint32_t& generator);

// COMBIEN DE PAIRES D'ANGLES le grain entretient, et de combien il dérive.
//
// ⚠ DEUX PAIRES POUR UNE SEULE PARTICULE, là où la rafale en a huit pour
// quatre. Le rapport est le même : deux paires par particule, un angle pour
// chaque direction du sol.
inline constexpr std::size_t kDriftingMoteAnglePairs = 2;
inline constexpr float kDriftingMoteDrift = 0.03F;

// LES DEUX PAS DE L'ALPHA, et le tic qui les sépare.
//
// ⚠ IL MONTE DE CINQ PENDANT TRENTE TICS PUIS TOMBE DE DEUX : la montée est
// deux fois et demie plus raide que la chute, si bien que le grain paraît vite
// et s'efface longtemps.
inline constexpr std::int32_t kDriftingMoteFadesAfterTick = 30;
inline constexpr std::int16_t kDriftingMoteAlphaRise = 5;
inline constexpr std::int16_t kDriftingMoteAlphaFall = 2;

// LE GRAIN QUI DÉRIVE : le mouvement du sous-type 96.
//
// Pour chaque piste vive : le compteur avance, l'alpha monte puis tombe, les
// DEUX paires d'angles poursuivent leur cible, et le grain DESCEND de sa clé —
// dans le repère du client, donc il MONTE chez nous — en dérivant de côté tant
// qu'il est au-dessus du nœud.
//
// ⚠ IL NE RENAÎT PAS, contrairement à la rafale : une fois passé sous le nœud
// il cesse de dériver, continue de monter et s'éteint par son alpha. C'est la
// VIE DE L'EFFET qui le borne.
void AdvanceDriftingMoteTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees,
                               std::uint32_t& generator);

// LA POURSUITE D'ANGLES, et TROIS familles la demandent.
//
// Un courant, une cible, et un pas vers elle : le courant descend de DEUX vers
// une cible plus basse, monte de TROIS vers une plus haute, et dès qu'il
// l'atteint une cible neuve est tirée. Le plafond vaut trois cent
// cinquante-neuf.
//
// ⚠ LE NOMBRE DE PAIRES ET LES DEUX PAS SONT LES SEULES DIFFÉRENCES entre les
// appelants : HUIT paires pour la rafale du 29, DEUX pour le grain du 96, SIX
// pour les trois plans du 111, DEUX pour la ronde du 27. Le reste est le même
// code du client, et le recopier une quatrième fois l'aurait fait diverger à la
// première correction.
//
// ⚠ ET LA RONDE NE DESCEND PAS DE DEUX MAIS D'UN, ni ne monte de trois mais
// d'un. Lui prêter les pas de ses sœurs la ferait trembler deux à trois fois
// trop vite ; les trois premiers appelants gardent les leurs par défaut.
//
// ⚠ ELLE TIRE AU SORT, et c'est pourquoi elle prend le générateur de la
// troupe : une même graine rejoue la même dérive.
void ChaseEffectTrackAngles(EffectTrack& track, std::size_t pairs, std::uint32_t& generator,
                            float fall = kQuadBurstAngleFall, float rise = kQuadBurstAngleRise);

}  // namespace ro::render

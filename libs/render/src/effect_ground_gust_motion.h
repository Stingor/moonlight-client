#pragma once

#include <array>
#include <cstdint>

#include "ro/render/effect_track_fwd.h"

// CE QUE QUATRE MOUVEMENTS DU CLIENT ONT EN COMMUN : la dérive de la meneuse et
// l'écho de ses trois suivantes.
//
// En-tête PRIVÉ : il vit à côté des sources qui l'emploient et ne sort pas de
// `libs/render/src`.
//
// ⚠ C'EST LE MÊME CODE, RECOPIÉ QUATRE FOIS CHEZ LE CLIENT : les cas 119 et 126
// du répartiteur, et les fins des deux mouvements qui poursuivent un acteur, se
// lisent instruction pour instruction. Les transcrire quatre fois les ferait
// diverger à la première correction.

namespace ro::render {

// CE QUI SÉPARE LE MOUVEMENT EN LIGNE DU 119 DE CELUI DU 126 : quatre nombres
// d'alpha, et rien d'autre. Les facteurs de vitesse, le cap et l'écho sont
// communs.
struct GroundGustLineProfile {
  // Jusqu'à quel tic l'alpha monte, de combien, et jusqu'où.
  std::int32_t rise_until_tick = 0;
  std::int16_t rise = 0;
  std::int16_t ceiling = 0;
  // De combien il retombe ensuite. La piste meurt sous zéro.
  std::int16_t fall = 0;
};

// LE MOUVEMENT EN LIGNE, tel que les cas 119 et 126 du répartiteur le font sur
// place : la meneuse tourne, pâlit ou s'allume, file le long de son cap, et
// l'écho passe tous les trois tics.
void AdvanceGroundGustLine(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                           const GroundGustLineProfile& profile);

// LA DÉRIVE DE LA MENEUSE : son ancre file le long de son cap, à plat, de la
// vitesse rangée dans sa case un. Sa hauteur ne bouge pas.
void DriftGroundGustLeader(EffectTrack& leader);

// L'ÉCHO, tous les trois tics du compteur de la meneuse : chaque suivante
// reprend l'ancre et l'inclinaison de sa voisine, et les quatre cinquièmes de
// son alpha.
void EchoGroundGustTrail(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render

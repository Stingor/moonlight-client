#pragma once

#include <array>
#include <cstdint>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE MOUVEMENT DU SOUS-TYPE 57 : quatre anneaux concentriques qui RESPIRENT.
//
// Le cas 57 du répartiteur de mouvement fait le travail sur place. Voir
// `scripts/releve_pieces_de_l_anneau_de_teleportation.py`.
//
// ⚠ IL N'EST PAS CELUI DU SOUS-TYPE 30, et la ressemblance est trompeuse : les
// deux ondulent leurs vingt et un segments sur un demi-tour de sinus par pas de
// neuf degrés, et tout le reste diffère.
//
//   - LE RAYON NE CROÎT PAS, IL RESPIRE. Le client le RECALCULE à chaque tic
//     depuis une amplitude figée au semis, multipliée par le sinus du DOUBLE du
//     tic. Le faire croître comme celui du trente ouvrirait l'anneau sans jamais
//     le refermer ;
//   - CHAQUE ANNEAU TOURNE À SA VITESSE, et c'est son RANG qui la donne : le
//     premier avance de cinq degrés par tic, le dernier de deux. Les faire
//     tourner ensemble rendrait les quatre anneaux solidaires ;
//   - LA PHASE A DEUX FORMULES, comme au trente, mais ni le seuil, ni le pas, ni
//     le facteur ne sont les mêmes.
//
// ⚠ ET LE SENS DES CHAMPS N'EST PAS LE SIEN NON PLUS (ADR 0017). Ce que l'anneau
// du trente lit dans `radius_at_anchor`, celui-ci le lit dans `lift_key`. Leur
// prêter le même sens ferait respirer le mauvais rayon.

// DE COMBIEN UN ANNEAU TOURNE PAR TIC : cinq degrés, moins son rang.
inline constexpr std::int16_t kTeleportRingTurnPerTickDegrees = 5;

// LE DOUBLE DU TIC, que le client emploie deux fois : pour lire son sinus, et
// pour placer les deux seuils qui suivent.
inline constexpr std::int32_t kTeleportRingBreathFactor = 2;
// OÙ L'ANNEAU COMMENCE À S'ÉTEINDRE, et où il BASCULE — en double-tics, comme le
// client les compare. Cent trente-cinq et cent quatre-vingts.
inline constexpr std::int32_t kTeleportRingFadeThreshold = 135;
inline constexpr std::int32_t kTeleportRingSwitchThreshold = 180;
inline constexpr std::int16_t kTeleportRingFadeBeforeSwitch = 1;
inline constexpr std::int16_t kTeleportRingFadeAfterSwitch = 10;

// LES DEUX FORMULES DE PHASE, et le rang qui choisit.
inline constexpr std::int32_t kTeleportRingPhaseStepPerRank = 90;
inline constexpr std::int32_t kTeleportRingPhaseThreshold = 180;
inline constexpr std::int32_t kTeleportRingFastStepPerRank = 30;
inline constexpr std::int32_t kTeleportRingFastFactor = 3;

// L'AMPLITUDE DE L'ONDE, la même que celle du sous-type 30 : le client lit la
// même constante.
inline constexpr float kTeleportRingUndulation = 0.3F;

// LES DEUX OCTETS QUE CETTE FAMILLE EMPLOIE, et leur sens lui appartient.
//
// ⚠ LE PREMIER EST UN DRAPEAU, LE SECOND UNE AMPLITUDE TRONQUÉE. Le semis range
// dans `mark[1]` la partie entière du rayon, en un octet SIGNÉ : au-delà de cent
// vingt-sept, l'anneau respire à l'envers. Le client ne s'en garde pas, et le
// corriger ici montrerait autre chose que ce qu'il montre.
inline constexpr std::size_t kTeleportRingMarkOfSwitch = 0;
inline constexpr std::size_t kTeleportRingMarkOfAmplitude = 1;

// UN TIC DES QUATRE PISTES. Le rang décide de la vitesse et de la phase : il
// n'est donc pas un détail d'appel, mais une donnée du mouvement.
void AdvanceTeleportRingTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render

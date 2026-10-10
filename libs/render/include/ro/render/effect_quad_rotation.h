#pragma once

#include <array>
#include <cstdint>

#include <glm/glm.hpp>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA ROTATION DES QUATRE COINS D'UN QUAD, par deux angles entiers.
//
// Cinquante-quatre dessinateurs y passent. C'est, avec la primitive de quad
// projeté, la seconde pièce
// partagée de la voie A : les dessinateurs bâtissent leurs coins à plat, puis
// passent par ici pour les coucher et les tourner.
//
// ⚠ LE SECOND ANGLE S'APPLIQUE EN PREMIER. Le client reçoit le tour puis
// l'inclinaison, et incline AVANT de tourner. Les deux rotations ne commutent
// pas — la seconde travaille sur la profondeur que la première vient de
// changer —, si bien que les appliquer dans l'ordre de la signature donne une
// figure penchée du mauvais côté. L'ordre des paramètres est celui du client
// pour que la transcription d'un site d'appel se lise sans permutation ; c'est
// cet avertissement qui porte l'ordre réel, pas la signature.
//
// ⚠ LES ANGLES SONT DES ENTIERS DE DEGRÉS, et le client les emploie à indexer
// une table de trois cent soixante entrées. Ses appelants les y ramènent
// eux-mêmes, par des soustractions de deux cent soixante-dix ; nous ramenons
// ici, ce qui donne le même angle et supporte en plus les négatifs.
//
// ⚠ LE REPÈRE EST CELUI DU CLIENT, dont l'ordonnée DESCEND. Les coins entrent
// et sortent dans ce repère ; c'est à la figure de les rendre au nôtre, une
// fois qu'elle a fini de les poser. Convertir ici obligerait chaque appelant à
// savoir si ses coins sont déjà convertis ou non.

// L'inclinaison mêle l'ordonnée et la profondeur ; le tour mêle l'abscisse et
// la profondeur.
void RotateEffectQuadCorners(std::array<glm::vec3, 4>& corners, std::int16_t turn_degrees,
                             std::int16_t tilt_degrees);

// Le même couple de rotations sur UN point, dans le même ordre. Les figures qui
// posent un centre avant leurs coins en ont besoin.
[[nodiscard]] glm::vec3 RotateEffectQuadPoint(glm::vec3 point, std::int16_t turn_degrees,
                                              std::int16_t tilt_degrees);

// LES DEUX ANGLES DE CAMÉRA RANGÉS DANS LE STYLE DES PISTES ZÉRO ET UN, où la
// figure les relit pour redresser ses carrés vers l'œil. Une seule fois par tic,
// avant la boucle des pistes, et même quand elles dorment.
//
// 🔴 ILS SE RANGENT TELS QU'ILS ARRIVENT. Le client écrit là `360 − lacet` et
// `450 − altitude` de SA caméra ; `EffectBillboardTurnDegrees` et
// `EffectBillboardTiltDegrees` les fournissent déjà ainsi. Les recompléter ici
// ferait tourner la figure à l'envers de la caméra : elle se présenterait par
// la tranche dès qu'on pivote.
void StoreEffectBillboardAngles(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::int16_t turn_degrees, std::int16_t tilt_degrees);

}  // namespace ro::render

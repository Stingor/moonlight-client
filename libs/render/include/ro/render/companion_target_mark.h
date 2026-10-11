#pragma once

#include <cstddef>

#include "ro/render/sleep_bubbles.h"

// LE MARQUEUR DE CIBLE D'UN COMPAGNON : l'anneau de ciblage du curseur, posé sur
// l'acteur que le geste d'attaque de l'homoncule ou du mercenaire a marqué.
//
// C'est un sprite-enfant du fichier du curseur (`kCursorSpritePath`), dans la
// même tenue que les bulles du sommeil : vingt unités au-dessus des pieds de
// son porteur, sans ombre, en avant de toute la scène, voilé comme tout
// sprite-enfant sous les invocations translucides. Il naît quand la marque
// atteint l'acteur et meurt quand elle le quitte.
//
// ⚠ SA CADENCE EST CELLE QUE LE FICHIER DÉCLARE, image après image, et non celle
// du curseur à l'écran, qui compte ses propres tics : le même anneau tourne ici
// à un autre rythme que sous la souris.
//
// ⚠ UN JOUEUR NE LE PORTE JAMAIS : seul un acteur qui se compose en créature —
// monstre, PNJ, compagnon — le montre, même quand le geste a marqué un joueur.
namespace ro::render {

// L'action du fichier du curseur qui porte l'anneau de ciblage.
inline constexpr std::size_t kCompanionTargetMarkAction = 10;

// De combien l'anneau flotte au-dessus des pieds de son porteur, en unités du
// monde : la hauteur des bulles du sommeil.
inline constexpr float kCompanionTargetMarkRiseUnits = kSleepBubblesRiseUnits;

}  // namespace ro::render

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE CARRÉ QUI FILE EN TOURNOYANT, suivi de trois échos : le sous-type 116.
//
// La figure est celle du carré teinté — `ComposeTintedRotatedQuad` —, et c'est
// le mouvement seul qui fait de ces quatre pistes un objet qui file et sa
// traînée.
//
// ⚠ UNE SEULE PISTE BOUGE, LA PREMIÈRE ; les trois autres ne sont que des
// ÉCHOS. Le client écrit la meneuse à des déplacements ABSOLUS que rien ne
// décale, puis la recopie en cascade tous les trois tics : la piste trois
// reçoit la deux, la deux reçoit la une, la une reçoit la meneuse — dans cet
// ordre, pour que chaque écho prenne l'état que sa voisine avait AVANT d'être
// elle-même écrasée.

// LA CADENCE DES ÉCHOS : une recopie quand le compteur de la meneuse est
// multiple de trois.
inline constexpr std::int32_t kSwirlTrailEchoPeriodTicks = 3;

// CE QUE L'ÉCHO PERD D'ALPHA en recopiant sa voisine : cent cinquante pour le
// premier, qui recopie la meneuse, vingt-cinq pour les deux autres. Chacun est
// planché à zéro.
inline constexpr std::int16_t kSwirlTrailFirstEchoAlphaDrop = 150;
inline constexpr std::int16_t kSwirlTrailEchoAlphaDrop = 25;

// LA ROTATION PROPRE DE LA MENEUSE, en degrés par tic, dans le sens qui DÉCROÎT
// (moins quinze, rattrapé d'un tour sous zéro).
inline constexpr std::int16_t kSwirlTrailSpinPerTick = 15;

// SON ALPHA : plus quarante-cinq tant que le compteur ne passe pas cinq,
// plafonné à 255, puis moins dix dès le quinzième tic, planché à zéro. Entre
// les deux, il tient.
inline constexpr std::int32_t kSwirlTrailLastTickOfRise = 5;
inline constexpr std::int16_t kSwirlTrailAlphaRise = 45;
inline constexpr std::int32_t kSwirlTrailFirstTickOfFall = 15;
inline constexpr std::int16_t kSwirlTrailAlphaFall = 10;

// LA CASE QUI PORTE LA VITESSE de la meneuse, et celle de l'angle propre que la
// figure lit pour faire respirer son rayon, que le mouvement recopie sans
// jamais l'avancer.
inline constexpr std::size_t kSwirlTrailSlotOfSpeed = 1;
inline constexpr std::size_t kSwirlTrailSlotOfSpinDegrees = 8;

// LA MARQUE QUE L'ÉCHO REÇOIT de sa voisine : la zéro, celle qui choisit
// l'image. La marque de teinte n'est lue que sur la meneuse, et le mouvement
// ne la recopie pas.
inline constexpr std::size_t kSwirlTrailMarkOfTexture = 0;

// Avance les quatre pistes d'un tic, et range les deux angles de caméra.
//
// ⚠ LES ANGLES DE CAMÉRA SONT RANGÉS AVANT TOUTE GARDE, dans le style des
// pistes zéro et un : même une meneuse éteinte les reçoit. Le carré teinté les
// y relit.
//
// ⚠ LA MENEUSE AVANCE SELON SON CAP de sa vitesse, par cosinus et sinus, sans
// jamais toucher sa hauteur. Le cap est tiré au sort au semis, et le semis
// recule l'ancre d'autant de pas que la meneuse en fera en quinze tics :
// l'objet traverse ainsi la place de son porteur.
void AdvanceSwirlTrailTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                             std::int16_t camera_turn_degrees, std::int16_t camera_tilt_degrees);

}  // namespace ro::render

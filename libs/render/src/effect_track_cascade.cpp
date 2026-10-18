#include <algorithm>
#include <cmath>

#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// COMMENT LA CASCADE LIT SES CASES : les mêmes que celles du carré teinté,
// puisque c'est sa figure.
constexpr std::size_t kSlotOfSpinDegrees = 8;
// La case que chaque saut remet à zéro. Le dessinateur du carré ne la lit pas ;
// elle est recopiée parce que le client la recopie, et non parce qu'on sait à
// quoi elle sert.
constexpr std::size_t kSlotClearedOnShift = 7;
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kMarkOfDrawing = 2;
// La case de queue où la hauteur du propriétaire se range, sans le balancement.
constexpr std::size_t kTailSlotOfHeight = 4;

// TOUS LES SIX TICS, la traînée avance d'un cran. Entre deux, seule la tête bouge.
constexpr std::int32_t kTicksBetweenShifts = 6;

// CE QU'UN SAUT COÛTE À LA PISTE QUI REÇOIT : un rayon plus large, un alpha plus
// faible. C'est toute la forme du sillage.
constexpr float kRadiusGrowthPerShift = 0.5F;
constexpr std::int16_t kAlphaDropPerShift = 50;

// ⚠ LA MARQUE DE DESSIN DE LA TRAÎNÉE EST IMPOSÉE, elle ne se recopie pas. Le
// client écrit deux dans chaque piste qui reçoit, quelle que soit la marque de
// la tête : le sillage a donc toujours la même parure, et seule la tête porte la
// sienne. La recopier ferait traîner la couleur du style au lieu du sillage.
constexpr std::uint8_t kMarkOfTrail = 2;

// CE QUE LA TÊTE FAIT À CHAQUE TIC.
constexpr float kSpinStepPerTick = 5.0F;
constexpr std::int16_t kAzimuthStepPerTick = 1;
// De combien la tête se balance de haut en bas, au rythme de son azimut.
constexpr float kBobAmplitude = 3.0F;
// ⚠ DE COMBIEN LA TÊTE SE TIENT AU-DESSUS de son propriétaire, et le nom dit le
// SENS et non le signe. Le client RETRANCHE six à l'ordonnée de l'acteur, dans
// un repère qui DESCEND : la soustraction élève. Recopier le signe mettrait la
// figure sous les pieds de celui qui la porte.
constexpr float kLiftAboveOwner = 6.0F;

// LA SEULE MARQUE QUI FAIT TOURNER LA TÊTE, et de combien par tic.
constexpr std::uint8_t kMarkThatSpins = 7;
constexpr std::int16_t kSpinDegreesPerTick = 10;

// UN SAUT DE LA CASCADE : ce que `vers` prend de `depuis`.
//
// Tout n'est pas recopié, comme chez le client : l'angle, le rayon,
// l'angle propre, l'alpha, l'ancre, la texture. Pas les tics, pas l'azimut, pas
// l'état d'allumage — la piste qui reçoit garde les siens.
void Shift(EffectTrack& vers, const EffectTrack& from) {
  vers.degrees = from.degrees;
  vers.radius_at_anchor = from.radius_at_anchor + kRadiusGrowthPerShift;
  vers.value[kSlotOfSpinDegrees] = from.value[kSlotOfSpinDegrees];
  vers.alpha =
      std::max<std::int16_t>(static_cast<std::int16_t>(from.alpha - kAlphaDropPerShift), 0);
  vers.anchor = from.anchor;
  vers.mark[kMarkOfTexture] = from.mark[kMarkOfTexture];
  vers.mark[kMarkOfDrawing] = kMarkOfTrail;
  vers.value[kSlotClearedOnShift] = 0.0F;
}

}  // namespace

void AdvanceCascadingTrailTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                 glm::vec3 owner_position, std::int16_t billboard_turn_degrees,
                                 std::int16_t billboard_tilt_degrees) {
  // ⚠ CES DEUX-LÀ SE POSENT MÊME QUAND LE NŒUD DORT. Le client les écrit avant
  // de regarder si sa première piste est allumée : un nœud qui se réveille
  // trouve donc des angles à jour, et non ceux d'une caméra qui n'existe plus.
  tracks[0].style = billboard_turn_degrees;
  tracks[1].style = billboard_tilt_degrees;

  EffectTrack& head = tracks[0];
  if (!head.active) {
    return;
  }

  ++head.ticks;
  if (head.ticks % kTicksBetweenShifts == 0) {
    // DE LA DERNIÈRE À LA PREMIÈRE : voir `effect_track.h`. Et seule une piste
    // DÉJÀ allumée reçoit — un nœud à une seule piste n'a donc pas de traînée.
    for (std::size_t rang = tracks.size() - 1; rang > 0; --rang) {
      if (tracks[rang].active) {
        Shift(tracks[rang], tracks[rang - 1]);
      }
    }
  }

  head.value[kSlotOfSpinDegrees] += kSpinStepPerTick;
  if (head.value[kSlotOfSpinDegrees] >= static_cast<float>(kEffectTrackFullTurnDegrees)) {
    head.value[kSlotOfSpinDegrees] -= static_cast<float>(kEffectTrackFullTurnDegrees);
  }
  head.azimuth_degrees = WrappedOnceToInt16(head.azimuth_degrees + kAzimuthStepPerTick);

  // ⚠ LE RÉ-ANCRAGE, et LES DEUX SIGNES SE RETOURNENT ici, pas seulement un. Le
  // client élève sa tête en retranchant, puis la balance en ajoutant, le tout
  // dans un repère qui descend : chez nous il faut donc ajouter l'élévation ET
  // retrancher le balancement. N'en retourner qu'un donnerait une figure au bon
  // endroit qui se balance à contretemps — l'écart le plus dur à voir.
  const float height = owner_position.y + kLiftAboveOwner;
  head.tail[kTailSlotOfHeight] = height;
  head.anchor = {owner_position.x,
                 height - std::sin(RadiansOfDegrees(head.azimuth_degrees)) * kBobAmplitude,
                 owner_position.z};

  // UNE SEULE MARQUE FAIT TOURNER LA TÊTE sur elle-même. Les autres se
  // contentent du balancement, et les faire tourner leur donnerait un mouvement
  // que le client ne leur donne pas.
  if (head.mark[kMarkOfDrawing] == kMarkThatSpins) {
    head.degrees = WrappedOnceToInt16(head.degrees + kSpinDegreesPerTick);
  }
}

}  // namespace ro::render

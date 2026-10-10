#include <algorithm>

#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// COMMENT CE MOUVEMENT LIT SA PISTE.
constexpr std::size_t kSlotOfFallStep = 1;
constexpr std::size_t kSlotOfSpinDegrees = 8;
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kMarkOfStyle = 4;
constexpr std::size_t kTailSlotOfSideways = 3;
constexpr std::size_t kTailSlotOfHeight = 4;
constexpr std::size_t kTailSlotOfDepth = 5;

// LA TRAÎNÉE AVANCE D'UN CRAN PAR TIC, et non tous les six comme celle du
// sous-type 47. Chaque piste attend son rang : la deuxième au deuxième tic, la
// troisième au troisième. C'est ce qui déplie la traînée au lieu de la faire
// paraître d'un bloc.
constexpr std::int32_t kFirstTickOfFirstShift = 1;

// CE QU'UN SAUT COÛTE EN ALPHA. La tête est bien plus vive que sa traînée : le
// premier cran en perd cent, les suivants vingt-cinq. Les égaliser donnerait une
// traînée aussi franche que sa tête, ce qui se voit.
constexpr std::int16_t kAlphaDropFromHead = 100;
constexpr std::int16_t kAlphaDropAlongTrail = 25;

// LA TÊTE MONTE SON ALPHA D'UN COUP puis le tient, et le perd bien plus tard.
constexpr std::int32_t kTicksOfRise = 10;
constexpr std::int16_t kAlphaRisePerTick = 20;
constexpr std::int32_t kTicksWhenFadeBegins = 32;
constexpr std::int16_t kAlphaFadePerTick = 5;

// DE COMBIEN LA TÊTE TOURNE SUR ELLE-MÊME à chaque tic, et dans quel sens.
constexpr std::int16_t kDegreesPerTick = 15;

// ⚠ LE STYLE QUI SE POSE : on croirait ce sprite montant, et il DESCEND. Le
// client part quatre-vingts unités au-dessus de son porteur et s'en rapproche
// de trois par tic, jusqu'à soixante. C'est un objet lancé qui retombe
// (ADR 0017).
constexpr std::uint8_t kStyleThatLands = 4;
constexpr float kHeightWhereItLands = 60.0F;
// L'angle où il doit se trouver en touchant, et la marge acceptée.
constexpr std::int16_t kDegreesAtLanding = 250;
constexpr std::int16_t kDegreesOfLandingSpan = 30;
// Passé ce tic, le style qui ne se pose PAS sonne à son tour.
constexpr std::int32_t kTickWhenStillStyleRings = 33;
// Jusqu'à quel tic un style qui ne se pose pas continue de tourner et de
// descendre. Au-delà il se fige, et n'attend plus que son effacement.
constexpr std::int32_t kLastTickThatStillTurns = 24;

// UN SAUT DE LA TRAÎNÉE : ce que `vers` prend de `depuis`.
void Shift(EffectTrack& vers, const EffectTrack& from, std::int16_t perte) {
  vers.degrees = from.degrees;
  vers.radius_at_anchor = from.radius_at_anchor;
  vers.value[kSlotOfSpinDegrees] = from.value[kSlotOfSpinDegrees];
  vers.alpha = std::max<std::int16_t>(static_cast<std::int16_t>(from.alpha - perte), 0);
  vers.anchor = from.anchor;
  vers.mark[kMarkOfTexture] = from.mark[kMarkOfTexture];
}

[[nodiscard]] std::int16_t SpinBackward(std::int16_t degrees) {
  const std::int32_t recule = degrees - kDegreesPerTick;
  return static_cast<std::int16_t>(recule < 0 ? recule + kEffectTrackFullTurnDegrees : recule);
}

}  // namespace

bool AdvanceFallingTrailTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               glm::vec3 owner_position, std::int16_t billboard_turn_degrees,
                               std::int16_t billboard_tilt_degrees) {
  tracks[0].style = billboard_turn_degrees;
  tracks[1].style = billboard_tilt_degrees;

  EffectTrack& head = tracks[0];
  if (!head.active) {
    return false;
  }
  ++head.ticks;
  if (head.ticks <= 0) {
    return false;
  }

  // ⚠ DE LA DERNIÈRE À LA PREMIÈRE, et chacune attend SON rang. Le client teste
  // le compteur contre trois, deux puis un : la traînée se déplie d'un cran par
  // tic au lieu de paraître entière.
  for (std::size_t rang = tracks.size() - 1; rang > 0; --rang) {
    if (head.ticks > static_cast<std::int32_t>(rang) + kFirstTickOfFirstShift - 1) {
      Shift(tracks[rang], tracks[rang - 1], rang == 1 ? kAlphaDropFromHead : kAlphaDropAlongTrail);
    }
  }

  const std::uint8_t style = head.mark[kMarkOfStyle];
  bool sonne = false;
  if (head.ticks <= kTicksOfRise) {
    head.alpha += kAlphaRisePerTick;
  }

  if (style == kStyleThatLands) {
    head.degrees = SpinBackward(head.degrees);
    // ⚠ ELLE DESCEND : le client AJOUTE son pas dans un repère qui descend, donc
    // nous le retranchons. Voir `kStyleThatLands`.
    head.tail[kTailSlotOfHeight] -= head.value[kSlotOfFallStep];
    if (head.tail[kTailSlotOfHeight] <= kHeightWhereItLands &&
        static_cast<std::int16_t>(head.degrees - kDegreesAtLanding) >= 0 &&
        static_cast<std::int16_t>(head.degrees - kDegreesAtLanding) <= kDegreesOfLandingSpan) {
      // LE STYLE QUI SE POSE CESSE D'ÊTRE LUI-MÊME en touchant : sa marque
      // retombe à zéro, et il finit comme les autres.
      head.mark[kMarkOfStyle] = 0;
      sonne = true;
    }
  } else if (head.ticks > kTicksWhenFadeBegins) {
    if (style == 0 && head.ticks == kTickWhenStillStyleRings) {
      sonne = true;
    }
    head.alpha =
        std::max<std::int16_t>(static_cast<std::int16_t>(head.alpha - kAlphaFadePerTick), 0);
  } else if (style == 0 || head.ticks <= kLastTickThatStillTurns) {
    head.degrees = SpinBackward(head.degrees);
    head.tail[kTailSlotOfHeight] -= head.value[kSlotOfFallStep];
  }

  // L'ANCRE SUIT LE PORTEUR, décalée des trois cases de queue.
  head.anchor = {owner_position.x + head.tail[kTailSlotOfSideways],
                 owner_position.y + head.tail[kTailSlotOfHeight],
                 owner_position.z + head.tail[kTailSlotOfDepth]};
  return sonne;
}

}  // namespace ro::render

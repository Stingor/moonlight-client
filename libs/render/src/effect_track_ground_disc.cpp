#include <cstddef>
#include <cstdint>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

namespace ro::render {
namespace {

// LA CASE OÙ LE SEMIS A RANGÉ L'OCTET DU SITE, et que le mouvement relit.
//
// ⚠ C'EST LA MÊME QUE CELLE DU CARRÉ QUI RESPIRE, et ce n'est pas une
// coïncidence : les deux sous-types passent par le même dessinateur, qui lit la
// variante à ce déplacement-là. Le sens d'un champ appartient au dessinateur
// (ADR 0017), et ici le dessinateur est partagé.
constexpr std::size_t kSlotOfVariant = 4;

// LA VARIANTE QUI TOURNE À REBOURS. Le client compare le flottant à QUATORZE
// par égalité exacte — ce n'est pas un seuil.
constexpr float kVariantThatTurnsBackwards = 14.0F;

// D'UN DEGRÉ PAR TIC, dans un sens ou dans l'autre. Un tour complet demande
// donc trois cent soixante tics.
constexpr std::int16_t kDegreesPerTick = 1;

}  // namespace

void AdvanceGroundDiscTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks) {
  // ⚠ LA PREMIÈRE PISTE SEULE, et sans boucle : le cas du répartiteur écrit un
  // déplacement fixe. Le bâtisseur n'en allume d'ailleurs qu'une.
  EffectTrack& track = tracks.front();
  const std::int16_t step = track.value[kSlotOfVariant] == kVariantThatTurnsBackwards
                                ? static_cast<std::int16_t>(-kDegreesPerTick)
                                : kDegreesPerTick;
  std::int32_t azimuth = track.azimuth_degrees + step;
  // ⚠ LE REBOUCLAGE EST CELUI DU CLIENT, une borne par sens : il n'emploie
  // aucun reste de division, et ajoute ou retranche un tour complet.
  if (azimuth < 0) {
    azimuth += kEffectTrackFullTurnDegrees;
  } else if (azimuth >= kEffectTrackFullTurnDegrees) {
    azimuth -= kEffectTrackFullTurnDegrees;
  }
  track.azimuth_degrees = static_cast<std::int16_t>(azimuth);
}

}  // namespace ro::render

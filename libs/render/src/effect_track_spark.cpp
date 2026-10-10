#include <cmath>
#include <numbers>

#include "ro/render/effect_node.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

[[nodiscard]] float CosOfDegrees(std::int32_t degrees) {
  return std::cos(static_cast<float>(degrees) * std::numbers::pi_v<float> / 180.0F);
}

[[nodiscard]] float SinOfDegrees(std::int32_t degrees) {
  return std::sin(static_cast<float>(degrees) * std::numbers::pi_v<float> / 180.0F);
}

[[nodiscard]] std::int16_t WithinFullTurn(std::int32_t degrees) {
  const std::int32_t remainder = degrees % kEffectTrackFullTurnDegrees;
  return static_cast<std::int16_t>(remainder < 0 ? remainder + kEffectTrackFullTurnDegrees
                                                 : remainder);
}

// CE QUE LE CAS CINQUANTE-NEUF POSE EN DUR.

// LES DEUX PISTES QUI PORTENT LA CAMÉRA. Le mouvement range le tour dans le
// style de la première et l'inclinaison dans celui de la deuxième ; le carré
// teinté les y relit. Voir `EffectTintedQuadStyle`.
constexpr std::size_t kTrackThatTakesTheTurn = 0;
constexpr std::size_t kTrackThatTakesTheTilt = 1;

// LE CARRÉ TOURNE SUR LUI-MÊME, de cinq degrés par tic, dans le sens qui
// DÉCROÎT. Le client retranche puis rattrape le passage sous zéro.
constexpr std::int16_t kSpinPerTick = 5;

// L'ALPHA MONTE NEUF TICS, PAS DIX. Le premier tic est pris par le tirage de
// l'azimut et n'ajoute rien : compter dix montées donnerait cent cinquante là où
// le client s'arrête à cent trente-cinq, et la particule vivrait cinq tics de
// plus.
constexpr std::int32_t kLastTickOfRise = 10;
constexpr std::int16_t kAlphaRisePerTick = 15;
constexpr std::int16_t kAlphaFallPerTick = 3;

// L'ÉCLAT, ET SON TIC PROPRE À CHAQUE PISTE.
//
// ⚠ LE SEUIL DESCEND DE SEPT PAR PISTE, et il descend MÊME QUAND LA PISTE EST
// ÉTEINTE : le client le décrémente en tête de boucle, hors de toute garde. Ne
// le faire avancer que pour les pistes vives décalerait l'éclat des suivantes.
//
// La première piste éclate à ses tics trente-cinq et trente-six, la deuxième à
// vingt-huit et vingt-neuf, et ainsi de suite : les quatre s'allument l'une
// après l'autre au lieu de scintiller ensemble.
constexpr std::int32_t kFirstFlashTick = 36;
constexpr std::int32_t kFlashStepPerTrack = 7;

// ⚠ LA SECONDE MARQUE AUTORISE L'ÉCLAT, LA PREMIÈRE LE PORTE. Seul le semis qui
// charge une DEUXIÈME texture pose la seconde à un ; les autres n'éclatent
// jamais, et leur première marque garde ce que le bâtisseur y a mis.
constexpr std::size_t kMarkThatAllowsTheFlash = 1;
constexpr std::size_t kMarkThatCarriesTheFlash = 0;
constexpr std::uint8_t kFlashAllowed = 1;

// LES DEUX CASES QUE CE MOUVEMENT LIT.
//
// ⚠ LA DÉRIVE EST SOUVENT NULLE, et ce n'est pas un oubli : le semis de Heal y
// pose zéro, si bien que ses étincelles MONTENT SANS DÉRIVER. Le cap tiré au
// sort ne se voit alors nulle part, et c'est le client qui le veut ainsi.
constexpr std::size_t kSlotOfDrift = 1;
constexpr std::size_t kSlotOfRise = 2;

}  // namespace

void AdvanceRisingSparkTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              std::int16_t billboard_turn_degrees,
                              std::int16_t billboard_tilt_degrees, std::uint32_t& generator) {
  tracks[kTrackThatTakesTheTurn].style = billboard_turn_degrees;
  tracks[kTrackThatTakesTheTilt].style = billboard_tilt_degrees;

  std::int32_t flash_tick = kFirstFlashTick;
  for (EffectTrack& track : tracks) {
    const std::int32_t seuil = flash_tick;
    flash_tick -= kFlashStepPerTrack;
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    // UNE PISTE RETARDÉE COMPTE EN SILENCE : son bâtisseur lui donne des tics
    // négatifs, et elle ne bouge qu'une fois revenue à zéro.
    if (track.ticks <= 0) {
      continue;
    }
    track.degrees = WithinFullTurn(track.degrees - kSpinPerTick);
    if (track.ticks == 1) {
      // LE CAP SE TIRE AU PREMIER TIC, pas au semis : le bâtisseur laisse
      // l'azimut à zéro et c'est le mouvement qui le choisit.
      track.azimuth_degrees =
          static_cast<std::int16_t>(NextEffectNodeDraw(generator) % kEffectTrackFullTurnDegrees);
    } else if (track.ticks <= kLastTickOfRise) {
      track.alpha = static_cast<std::int16_t>(track.alpha + kAlphaRisePerTick);
    } else {
      if (track.mark[kMarkThatAllowsTheFlash] == kFlashAllowed) {
        track.mark[kMarkThatCarriesTheFlash] =
            (track.ticks == seuil || track.ticks == seuil - 1) ? 1 : 0;
      }
      track.alpha = static_cast<std::int16_t>(track.alpha - kAlphaFallPerTick);
      // ⚠ LA PISTE S'ÉTEINT QUAND SON ALPHA PASSE SOUS ZÉRO, et c'est ce qui
      // borne sa vie : neuf montées de quinze font cent trente-cinq, que des
      // chutes de trois épuisent en quarante-cinq tics.
      if (track.alpha < 0) {
        track.alpha = 0;
        track.active = false;
      }
    }
    // ⚠ LE REPÈRE DU CLIENT DESCEND, LE NÔTRE MONTE : il RETRANCHE pour élever,
    // donc nous AJOUTONS. Recopier le signe au lieu du sens enterrerait les
    // étincelles au lieu de les faire monter.
    track.anchor.y += track.value[kSlotOfRise];
    // ⚠ LE CAP TIRÉ EST CELUI DU CLIENT, et l'ancre est dans notre repère : la
    // profondeur de la dérive se nie. Tiré sur un tour entier, le miroir ne se
    // voit pas en moyenne, mais à graine égale l'étincelle partirait ailleurs.
    const float derive = track.value[kSlotOfDrift];
    track.anchor.x += derive * CosOfDegrees(track.azimuth_degrees);
    track.anchor.z -= derive * SinOfDegrees(track.azimuth_degrees);
  }
}

}  // namespace ro::render

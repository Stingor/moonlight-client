#include "ro/render/effect_track.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// CE QUE LE CAS 43 POSE EN DUR, et comment il lit les cases de sa
// piste. Aucune de ces valeurs n'est un réglage à notre main.
//
// ⚠ CE MOUVEMENT VIT DANS SON PROPRE FICHIER, et ce n'est pas un rangement :
// deux mouvements donnent des valeurs DIFFÉRENTES aux mêmes idées — l'anneau
// fait descendre son alpha de trois par tic, celui-ci de sept —, et les laisser
// se partager une portée obligerait à inventer des noms pour les distinguer.
// Un fichier par mouvement, et chaque nom reste celui de sa valeur.

// La case qui porte le pas de croissance du rayon, et celle de l'angle de
// rotation. La deuxième case porte la demi-taille du quad, que seule la figure
// lit.
constexpr std::size_t kSlotOfRadiusGrowth = 0;
constexpr std::size_t kSlotOfSpinDegrees = 2;
// La marque qui porte le cas de couleur, et la seule dont le mouvement se sert.
constexpr std::size_t kMarkOfColorCase = 0;
constexpr std::uint8_t kMarkThatTurnsSlowly = 2;

// Les dix premiers tics font monter l'alpha ; passé le second seuil il descend.
// Entre les deux, la particule tient sa pose.
constexpr std::int32_t kLastTickThatRaisesAlpha = 9;
constexpr std::int32_t kFirstTickThatLowersAlpha = 25;
constexpr std::int16_t kAlphaRisePerTick = 15;
constexpr std::int16_t kAlphaRiseForMarkedTrack = 25;
constexpr std::int16_t kAlphaFallPerTick = 7;

// De combien l'azimut et l'angle avancent par tic. L'azimut n'avance que pour
// la marque lente, et l'angle avance plus vite pour toutes les autres.
constexpr std::int16_t kAzimuthStepForMarkedTrack = 3;
constexpr float kSpinStepForMarkedTrack = 3.0F;
constexpr float kSpinStepPerTick = 5.0F;

}  // namespace

void AdvanceRisingParticleTrack(EffectTrack& track) {
  if (!track.active) {
    return;
  }
  const std::int32_t before = track.ticks;
  ++track.ticks;
  // UN TIC NÉGATIF EST UNE ATTENTE : le bâtisseur tire un retard de dix-huit à
  // quarante-sept tics pour que les quatre pistes n'arrivent pas ensemble.
  if (track.ticks > 0) {
    track.radius_at_anchor += track.value[kSlotOfRadiusGrowth];
  }

  const bool marquee = track.mark[kMarkOfColorCase] != 0;
  // ⚠ LA MONTÉE SE DÉCIDE SUR LE TIC D'AVANT, la descente sur celui d'après. Les
  // juger tous deux sur le même ferait manquer une montée ou en ajouter une, et
  // dix pas de vingt-cinq ne pardonnent pas un pas de plus.
  //
  // ⚠ ET LA COMPARAISON DE LA MONTÉE EST NON SIGNÉE dans le client : un compteur
  // encore négatif y paraît immense et tombe donc dans l'autre branche. La
  // rendre signée ferait monter l'alpha pendant tout le retard.
  if (before >= 0 && before <= kLastTickThatRaisesAlpha) {
    track.alpha = static_cast<std::int16_t>(
        track.alpha + (marquee ? kAlphaRiseForMarkedTrack : kAlphaRisePerTick));
  } else if (track.ticks > kFirstTickThatLowersAlpha) {
    track.alpha = static_cast<std::int16_t>(track.alpha - kAlphaFallPerTick);
    if (track.alpha <= 0) {
      track.alpha = 0;
    }
  }

  if (track.mark[kMarkOfColorCase] == kMarkThatTurnsSlowly) {
    track.azimuth_degrees =
        static_cast<std::int16_t>(track.azimuth_degrees + kAzimuthStepForMarkedTrack);
  }
  // LE REPLI SE FAIT DANS LES DEUX CAS, marque ou non : le client le pose après
  // son test, et un azimut semé tout près du tour entier déborderait sinon.
  DropOneTurn(track.azimuth_degrees);

  float& angle = track.value[kSlotOfSpinDegrees];
  angle += track.mark[kMarkOfColorCase] == kMarkThatTurnsSlowly ? kSpinStepForMarkedTrack
                                                                : kSpinStepPerTick;
  if (angle >= static_cast<float>(kEffectTrackFullTurnDegrees)) {
    angle -= static_cast<float>(kEffectTrackFullTurnDegrees);
  }
}

}  // namespace ro::render

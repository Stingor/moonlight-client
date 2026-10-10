#include <array>
#include <cstddef>

#include "ro/render/effect_aura_column.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

namespace ro::render {
namespace {

// LE TIC OÙ LES COUCHES CESSENT DE MONTER, et celui où la dernière se retire.
constexpr std::int32_t kLastTickOfRise = 20;
constexpr std::int32_t kTickWhenTheOuterFades = 100;

constexpr float kRisePerTick = 13.0F;
constexpr float kFallPerTick = 5.0F;
// DE COMBIEN LE RAYON ENFLE À CHAQUE TIC. Multiplicatif : la colonne s'ouvre en
// s'accélérant.
constexpr float kRadiusGrowth = 1.01F;

// LE PLAFOND DE CHAQUE COUCHE, de la plus étroite à la plus large. Vingt de plus
// à chaque fois, et ce n'est pas une règle que le client applique : il écrit les
// dix bornes une à une.
constexpr std::array<float, kAuraColumnLayers> kCeilings = {70.0F,  90.0F,  110.0F, 130.0F, 150.0F,
                                                            170.0F, 190.0F, 210.0F, 230.0F, 250.0F};

// ⚠ LA COUCHE LA PLUS LARGE NE DÉCROÎT PAS AVEC LES AUTRES. La boucle de
// retrait n'en couvre que NEUF ; la dixième attend le centième tic. C'est ce qui
// laisse un bord à la colonne quand son cœur s'est éteint.
constexpr std::size_t kLayersThatFadeEarly = kAuraColumnLayers - 1;

}  // namespace

void AdvanceRingExpansionTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::int16_t billboard_turn_degrees,
                                std::int16_t billboard_tilt_degrees) {
  // ⚠ CES DEUX-LÀ SE POSENT MÊME QUAND LE NŒUD DORT.
  // LES ANGLES DE PANNEAU, RANGÉS TELS QUELS : la troupe passe déjà le
  // complément du client (`EffectBillboardTurnDegrees`, `…TiltDegrees`) ; le
  // recalculer ici rendrait à la figure l'angle brut de la caméra.
  tracks[0].style = billboard_turn_degrees;
  tracks[1].style = billboard_tilt_degrees;

  for (EffectTrack& track : tracks) {
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    if (track.ticks <= 0) {
      continue;
    }

    if (track.ticks < kLastTickOfRise) {
      // ⚠ LES DIX COUCHES MONTENT DU MÊME PAS ET S'ARRÊTENT À DES HAUTEURS
      // DIFFÉRENTES : c'est ce décalage de plafonds qui ouvre la colonne en
      // dégradé au lieu de l'allumer d'un bloc.
      for (std::size_t layer = 1; layer <= kAuraColumnLayers; ++layer) {
        track.value[layer] += kRisePerTick;
        const float ceiling = kCeilings[layer - 1];
        if (track.value[layer] >= ceiling) {
          track.value[layer] = ceiling;
        }
      }
    } else {
      for (std::size_t layer = 1; layer <= kLayersThatFadeEarly; ++layer) {
        track.value[layer] -= kFallPerTick;
        if (track.value[layer] < 0.0F) {
          track.value[layer] = 0.0F;
        }
      }
      if (track.ticks > kTickWhenTheOuterFades) {
        track.value[kAuraColumnLayers] -= kFallPerTick;
        // ⚠ ET LE RAYON ENFLE UNE SECONDE FOIS tant que ce dernier bord vit. Le
        // client multiplie deux fois dans le même tic : la colonne s'ouvre plus
        // vite en se retirant qu'en s'ouvrant.
        if (track.value[kAuraColumnLayers] > 0.0F) {
          track.radius_at_anchor *= kRadiusGrowth;
        }
        if (track.value[kAuraColumnLayers] < 0.0F) {
          track.value[kAuraColumnLayers] = 0.0F;
        }
      }
    }
    track.radius_at_anchor *= kRadiusGrowth;
  }
}

}  // namespace ro::render

#include <cstdint>

#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

namespace ro::render {
namespace {

// PENDANT COMBIEN DE TICS L'ALPHA MONTE. Passé là, il redescend d'un par tic.
constexpr std::int32_t kLastTickOfRise = 10;

// LES DEUX PAS DE MONTÉE, et c'est la VARIANTE DU NŒUD qui choisit. Six pour
// celle-là, deux pour toutes les autres : le sprite de cent treize paraît trois
// fois plus vite que ses voisins.
constexpr std::uint8_t kVariantThatRisesFast = 113;
constexpr std::int16_t kAlphaRiseWhenFast = 6;
constexpr std::int16_t kAlphaRiseOtherwise = 2;
constexpr std::int16_t kAlphaFallPerTick = 1;

// DE COMBIEN LE RAYON ENFLE À CHAQUE TIC. Multiplicatif, non additif : le sprite
// double de taille en une dizaine de tics, et l'écart entre deux boules de
// foudre décalées de quatre tics se voit d'autant mieux.
constexpr float kRadiusGrowthPerTick = 1.07F;

}  // namespace

void AdvanceGrowingSpriteTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                std::uint8_t node_variant) {
  const std::int16_t rise =
      node_variant == kVariantThatRisesFast ? kAlphaRiseWhenFast : kAlphaRiseOtherwise;
  for (EffectTrack& track : tracks) {
    // ⚠ LE CLIENT TESTE L'ALLUMAGE COMME UN ENTIER SIGNÉ, « plus grand que zéro » :
    // une piste éteinte ne vieillit pas, et son rayon n'enfle pas non plus.
    if (!track.active) {
      continue;
    }
    ++track.ticks;
    // ⚠ UNE PISTE RETARDÉE NE FAIT RIEN, pas même enfler. Son compteur remonte
    // vers zéro et c'est tout : c'est ce qui fait paraître les six boules de
    // foudre d'EF_STASIS l'une après l'autre.
    if (track.ticks <= 0) {
      continue;
    }
    if (track.ticks <= kLastTickOfRise) {
      track.alpha = static_cast<std::int16_t>(track.alpha + rise);
    }
    track.radius_at_anchor *= kRadiusGrowthPerTick;
    // ⚠ ET LE RAYON ENFLE AUSSI PENDANT LA MONTÉE : les deux règles ne sont pas
    // exclusives. Le client les écrit l'une après l'autre, et n'en garder qu'une
    // par tic donnerait un sprite qui grandit deux fois moins vite.
    if (track.ticks > kLastTickOfRise) {
      track.alpha = static_cast<std::int16_t>(track.alpha - kAlphaFallPerTick);
      if (track.alpha < 0) {
        track.alpha = 0;
      }
    }
  }
}

}  // namespace ro::render

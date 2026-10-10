#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA ROUE DE VENT : un carré DEBOUT qui roule vers sa cible, suivi de trois
// échos. Sous-type 126, un seul identifiant — 624, Kamaitachi.
//
// ⚠ CE N'EST PAS UN ANNEAU MAIS UN CARRÉ, comme sa sœur du 119, et il n'est
// pas violet : sa teinte est une constante,
// quatre-vingts, deux cent cinquante-cinq, quatre-vingts — un vert, le même
// quel que soit l'ordre des canaux, puisque le rouge et le bleu sont égaux.
//
// ⚠ C'EST LA BOURRASQUE DU 119 REDRESSÉE. Même ancre, même rayon, même
// inclinaison qui tourne de trente degrés par tic, mêmes coins à quatre-vingt-dix
// degrés d'écart en partant d'un quart de tour. Mais là où le 119 couche ses
// coins sur le sol, celui-ci les dresse dans le plan VERTICAL qui contient son
// cap : la partie horizontale du coin suit le cap, la verticale monte. Il roule
// comme une roue.

// COMBIEN DE COINS, et le quart de tour qui les sépare.
inline constexpr std::size_t kRollingGustCorners = 4;
inline constexpr std::int32_t kRollingGustQuarterTurn = kEffectTrackFullTurnDegrees / 4;

// SA TEINTE ET SON MÉLANGE, en dur : le vert, AJOUTÉ — le mode de mélange vaut
// zéro.
inline constexpr std::array<std::uint8_t, 3> kRollingGustColor = {80, 255, 80};
inline constexpr std::int16_t kRollingGustBlend = 0;

// LE CARRÉ D'UNE PISTE, debout dans le plan de son cap.
//
// Rend RIEN quand la piste est éteinte ou que son alpha n'est pas strictement
// positif.
//
// ⚠ LE DESSINATEUR NE LIT PAS LA MARQUE TROIS, que le bâtisseur pose
// pourtant à un : sa teinte est fixe. Ses seules lectures sont l'allumage,
// l'alpha, le cap, l'inclinaison, le rayon et l'ancre.
//
// ⚠ SES COINS SONT ABSOLUS : l'ancre porte déjà la position du monde.
[[nodiscard]] std::optional<EffectWorldQuad> ComposeRollingGustQuad(
    const EffectTrack& track, const EffectOwnerTint& owner = {});

// LE MOUVEMENT DU SOUS-TYPE 126 : celui du 119, à quatre nombres près.
//
// Même rotation, même cap pris au premier tic, mêmes facteurs de vitesse, même
// écho. Seul l'alpha change — il monte de six jusqu'au dixième tic, plafonné à
// soixante, puis retombe de TROIS. La roue est donc quatre fois plus pâle que
// la bourrasque, et s'efface plus lentement.
void AdvanceRollingGustTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render

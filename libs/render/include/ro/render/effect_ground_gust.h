#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA BOURRASQUE AU SOL : un carré plat qui file vers sa cible, suivi de trois
// échos de plus en plus pâles.
//
// La figure et le mouvement du sous-type 119. UN SEUL identifiant l'emploie —
// 547, sur l'image du vent.
//
// ⚠ CE N'EST PAS UN ARC MAIS UN CARRÉ, et rien ne lui vient d'une couleur
// d'ailleurs : sa teinte est l'une de quatre constantes que sa marque choisit.
//
// ⚠ ET CE DESSINATEUR SERT TROIS SOUS-TYPES : les 122 et 125 prennent cette
// FIGURE, avec des mouvements qui poursuivent un ACTEUR. Ce qu'ils en lisent
// n'est que sa position, que `app` donne à chaque tic — voir
// `effect_homing_gust.h`.

// COMBIEN DE COINS un carré porte, et de combien de degrés on avance d'un coin
// au suivant.
inline constexpr std::size_t kGroundGustCorners = 4;
inline constexpr std::int32_t kGroundGustQuarterTurn = kEffectTrackFullTurnDegrees / 4;

// LES QUATRE TEINTES QUE LA MARQUE CHOISIT, et rien au-delà.
inline constexpr std::size_t kGroundGustColors = 4;
inline constexpr std::array<std::array<std::uint8_t, 3>, kGroundGustColors> kGroundGustPalette = {{
    {80, 80, 255},
    {80, 255, 80},
    {255, 255, 255},
    {70, 70, 70},
}};

// LES DEUX MÉLANGES : les deux premières teintes S'AJOUTENT — le mode zéro est
// l'addition —, les deux dernières se peignent par l'alpha de la source.
inline constexpr std::int16_t kGroundGustAddedBlend = 0;
inline constexpr std::int16_t kGroundGustOrdinaryBlend = 1;
inline constexpr std::size_t kGroundGustFirstOrdinaryColor = 2;

// LA CASE DE LA MARQUE qui porte la teinte, et celle que le bâtisseur remet à
// zéro sans que personne ne la lise.
inline constexpr std::size_t kGroundGustMarkOfColor = 3;
inline constexpr std::size_t kGroundGustMarkOfUnused = 0;

// LA CASE DE LA PISTE qui porte la vitesse de la dérive (ADR 0017).
inline constexpr std::size_t kGroundGustSlotOfSpeed = 1;

// LA TEINTE ET LE MÉLANGE d'une piste, selon sa marque.
[[nodiscard]] std::array<std::uint8_t, 3> EffectGroundGustColor(std::uint8_t mark);
[[nodiscard]] std::int16_t EffectGroundGustBlendMode(std::uint8_t mark);

// LE CARRÉ D'UNE PISTE : plat, centré sur l'ancre, tourné sur lui-même.
//
// Sa demi-diagonale est le rayon à l'ancre, et ses quatre coins sont à la MÊME
// hauteur — c'est une figure couchée, pas un billboard.
//
// Rend RIEN quand la piste est éteinte, quand son alpha n'est pas strictement
// positif, ou quand sa marque dépasse les quatre teintes : l'aiguillage du
// client n'a pas de cas par défaut, et y retomber inventerait une figure.
//
// ⚠ SES COINS SONT ABSOLUS : l'ancre porte déjà la position du monde, comme
// pour le trait gainé.
[[nodiscard]] std::optional<EffectWorldQuad> ComposeGroundGustQuad(
    const EffectTrack& track, const EffectOwnerTint& owner = {});

// LE MOUVEMENT DU SOUS-TYPE 119 : une piste file, trois la suivent en écho.
//
// ⚠ TOUT EST DERRIÈRE L'ÉTAT DE LA PISTE ZÉRO. Elle seule a un compteur, un
// alpha et un cap ; les trois autres ne sont que ce qu'elle portait trois, six
// et neuf tics plus tôt. Les faire vivre chacune pour soi donnerait quatre
// bourrasques au lieu d'une et de sa traîne.
//
// ⚠ ET LE CAP NE SE REPREND PAS : le client le calcule au premier tic utile et
// s'y tient. Le recalculer à chaque tic ferait virer la bourrasque quand le
// nœud bouge, ce que le client ne fait pas.
//
// ⚠ LES ÉCHOS NE MEURENT PAS AVEC LEUR SOURCE : dès que la piste zéro s'éteint,
// le mouvement sort tout entier et les trois autres gardent leur dernier alpha.
// C'est la vie du nœud qui les efface.
void AdvanceGroundGustTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render

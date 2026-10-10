#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA PILE EN SPIRALE : des carrés empilés qui tournent et rétrécissent.
//
// C'est la figure du sous-type 103. Elle sort par la voie A. Sept identifiants
// l'emploient, tous posés par le même bâtisseur : ce sont les tourbillons.
//
// Chaque étage est un carré posé à plat — quatre coins à quatre-vingt-dix degrés
// sur un cercle —, et d'un étage au suivant la figure monte d'un pas, tourne de
// quarante-cinq degrés et RÉTRÉCIT. C'est ce triple mouvement qui fait la
// spirale, et il tient entièrement dans la figure : le mouvement de piste, lui,
// ne fait tourner que l'ensemble.
//
// ⚠ LE CARRÉ EST GAUCHI D'UNE UNITÉ, et ce n'est pas un arrondi : deux de ses
// coins sont posés un cran plus haut que les deux autres. C'est ce qui donne à
// la pile son allure vrillée plutôt que celle d'un empilement de galettes.
//
// ⚠ ET LE RÉTRÉCISSEMENT RALENTIT. Le pas vaut « deux virgule deux moins un
// cinquième du rang », multiplié par la case de rétrécissement : il décroît donc
// d'un étage à l'autre, et devient NÉGATIF au-delà du onzième — la pile se
// remet alors à s'élargir. Un pas constant donnerait un cône.

// DE COMBIEN LA FIGURE TOURNE d'un étage au suivant.
inline constexpr std::int32_t kSpiralStackTurnPerStep = 45;
// L'ÉCART DES QUATRE COINS sur le cercle.
inline constexpr std::int32_t kSpiralStackCornerDegrees = 90;
// DE COMBIEN LE CARRÉ EST GAUCHI, en unités du monde.
inline constexpr float kSpiralStackWarp = 1.0F;
// LES DEUX NOMBRES DU PAS DE RÉTRÉCISSEMENT.
inline constexpr float kSpiralStackShrinkBase = 2.2F;
inline constexpr float kSpiralStackShrinkDecay = 0.2F;

// LES CASES QUE CETTE FIGURE LIT, et leur sens lui appartient (ADR 0017).
inline constexpr std::size_t kSlotOfSpiralTop = 0;
inline constexpr std::size_t kSlotOfSpiralRise = 1;
inline constexpr std::size_t kMarkOfSpiralSteps = 0;
inline constexpr std::size_t kMarkOfSpiralStyle = 4;

// CE QU'UN STYLE DONNE À UN ÉTAGE.
struct EffectSpiralStackStyle {
  std::array<std::uint8_t, 3> color = {255, 255, 255};
  // Dans la numérotation du seizième paramètre de la voie A : zéro veut dire
  // ADDITION. Voir `EffectParticleBlend`.
  std::int16_t blend_mode = 1;
  // Par combien l'alpha de la piste est DIVISÉ. Deux styles seulement le font,
  // et c'est ainsi qu'ils obtiennent un voile discret.
  std::int16_t alpha_divisor = 1;
  // ⚠ FAUX QUAND LE STYLE NE DESSINE PAS. Le `switch` du client retombe sur un
  // `default` muet ; poser un carré blanc à sa place montrerait ce qu'il cache.
  bool draws = true;
};

// COMBIEN DE COUCHES un style peint, et ce que chacune demande. Un style en peint
// une ; un seul en peint DEUX, l'une par-dessus l'autre.
[[nodiscard]] std::vector<EffectSpiralStackStyle> EffectSpiralStackStylesFor(std::uint8_t style);

// LES QUADS D'UNE PISTE, un par étage et par couche.
//
// Rend un lot VIDE quand la piste est éteinte, quand son alpha est nul, quand
// elle n'a aucun étage, ou quand son style ne dessine pas.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeSpiralStackQuads(
    const EffectTrack& track, glm::vec3 node_origin, const EffectOwnerTint& owner = {});

}  // namespace ro::render

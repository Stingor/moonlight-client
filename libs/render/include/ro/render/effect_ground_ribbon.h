#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE RUBAN POSÉ AU SOL : un trait qui glisse vers une cible.
//
// C'est le dessinateur du sous-type 56. Il sort par la voie A, un quad par piste. Douze
// identifiants l'emploient, répartis sur quatre bâtisseurs — qui sont un seul semis.
//
// Ses quatre coins sont tous à la MÊME HAUTEUR, juste au-dessus du sol, et
// tendus le long du cap que porte l'inclinaison de la piste. Sa demi-largeur
// vient de la clé ; ses deux extrémités de deux cases mesurées depuis le rayon à
// l'ancre.
//
// ⚠ UNE CASE DÉCIDE DU SENS, ET PAR UN SEUIL DE CENT. Quand elle dépasse cent,
// l'extrémité s'écarte dans la direction du cap ; en dessous, le client RETOURNE
// le cap d'un demi-tour ET inverse la soustraction. Traiter les deux pareil
// replierait le ruban sur lui-même pour la moitié des cas, et rien dans une
// capture fixe ne dirait lequel des deux sens est le bon.

// À QUELLE HAUTEUR le ruban se pose, telle que le client l'écrit en dur dans
// son repère qui descend : NÉGATIVE, le ruban flotte juste au-dessus du sol.
// Le dessinateur la ramène au nôtre avec le reste du coin.
inline constexpr float kGroundRibbonHeight = -6.0F;

// LE SEUIL QUI DÉCIDE DU SENS de chaque extrémité.
inline constexpr float kGroundRibbonForwardThreshold = 100.0F;

// LES CASES QUE CETTE FIGURE LIT, et leur sens lui appartient (ADR 0017).
inline constexpr std::size_t kSlotOfRibbonSideways = 0;
inline constexpr std::size_t kSlotOfRibbonNearEnd = 1;
inline constexpr std::size_t kSlotOfRibbonSpan = 2;
inline constexpr std::size_t kSlotOfRibbonStep = 3;
inline constexpr std::size_t kMarkOfRibbonStyle = 0;

// LA MARQUE À PARTIR DE LAQUELLE le rythme d'alpha change, et à partir de
// laquelle le pas de glissement s'accélère. Dix, et c'est la même borne pour les
// deux — le mouvement la lit deux fois.
inline constexpr std::uint8_t kGroundRibbonFastMark = 10;
// Celle à partir de laquelle le ruban S'AJOUTE au lieu de se mélanger.
inline constexpr std::uint8_t kGroundRibbonAddedMark = 30;

// CE QU'UN STYLE DONNE AU RUBAN.
struct EffectGroundRibbonStyle {
  std::array<std::uint8_t, 3> color = {225, 225, 255};
  // Dans la numérotation du seizième paramètre de la voie A : zéro est l'ADDITION.
  std::int16_t blend_mode = 1;
  // ⚠ LES NEUF STYLES ADDITIFS SONT AUSSI APLATIS AU PREMIER PLAN, quad par
  // quad et quel que soit le seau du nœud : voir `EffectQuadDepth`.
  bool flattens_depth = false;
};

// Ce que le style d'une piste demande. Tout style hors des dix-huit connus rend
// le cas sans marque : un bleu pâle qui se mélange.
[[nodiscard]] EffectGroundRibbonStyle EffectGroundRibbonStyleFor(std::uint8_t style);

// LE QUAD D'UNE PISTE, ou rien.
//
// Rend `std::nullopt` quand la piste est éteinte ou que son alpha est nul — ce
// qui est le cas au premier tic, le semis posant l'alpha à zéro.
[[nodiscard]] std::optional<EffectWorldQuad> ComposeGroundRibbonQuad(
    const EffectTrack& track, glm::vec3 node_origin, const EffectOwnerTint& owner = {});

}  // namespace ro::render

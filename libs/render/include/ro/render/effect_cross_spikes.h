#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_fwd.h"
#include "ro/render/effect_node_world.h"

namespace ro::render {

// LA CROIX DE POINTES : quatre triangles dressés sur un carré.
//
// C'est la figure du sous-type 9. Douze identifiants l'emploient — les pics de
// pierre, les murs de glace, les éclats de Frost Diver et de Storm Gust.
//
// Chaque triangle prend DEUX coins voisins du carré et les joint à un SOMMET
// dressé au-dessus du centre. Les quatre se suivent autour du carré, si bien que
// la figure entière est une pyramide creuse — ou une croix, vue de dessus.
//
// ⚠ LE CARRÉ EST AU SOL ET LE SOMMET MONTE EN Y, là où le client les écrit en XY
// et en Z. Son dessinateur pose ses vertex dans un repère que la matrice du nœud
// redresse ensuite d'un quart de tour ; nous replions ce quart de tour dans la
// figure, comme le repère descendant du client l'est ailleurs. Sans cela la
// pyramide se dessine COUCHÉE — le pic de terre sortait du sol à l'horizontale,
// et c'est la comparaison en jeu qui l'a dit.
//
// ⚠ ELLE NE LIT AUCUNE PISTE, à une exception près, et c'est ce qui la rend si
// bon marché : tout ce qu'elle dessine vient des CHAMPS DU NŒUD, que le relevé
// porte depuis le lot 7. Ses neuf bâtisseurs n'écrivent rien dans les pistes —
// huit sur neuf n'y touchent pas un octet.
//
// ⚠ ET SA COORDONNÉE DE TEXTURE AVANCE D'UN TRIANGLE À L'AUTRE : chacun prend
// une bande de l'image deux dixièmes plus loin que le précédent. Les quatre
// faces ne montrent donc pas la même chose, et leur donner la même image
// entière effacerait ce dégradé.

// COMBIEN DE TRIANGLES la croix compte.
inline constexpr std::size_t kCrossSpikeFaces = 4;

// DE COMBIEN la bande de texture avance d'une face à la suivante.
inline constexpr float kCrossSpikeTextureStep = 0.2F;

// LA MARQUE DE PISTE QUI CHANGE TOUT, et la seule que cette figure lise. Un
// bâtisseur sur neuf la pose ; les huit autres laissent la piste vierge.
inline constexpr std::uint8_t kCrossSpikeDrawnColor = 3;

// CE QU'UNE CROIX DEMANDE POUR SE DESSINER.
struct EffectCrossSpikeRequest {
  // Le demi-côté du carré au sol, et la hauteur du sommet.
  float half_side = 0.0F;
  float apex_height = 0.0F;
  // La teinte que le nœud porte, alpha compris.
  std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  // ⚠ VRAI quand la piste porte la marque à part : le client TIRE alors une
  // couleur au sort, peint en ADDITION et soumet ses triangles dans une autre
  // file. Le champ dit que le cas existe ; c'est l'appelant qui tire.
  bool draws_its_own_color = false;
};

// LES QUATRE FACES, dans l'ordre où le client les soumet.
//
// `orientation` place la croix : le client la tire d'une matrice que le nœud
// porte et que nous ne portons pas encore — l'appelant la donne, comme pour les
// autres figures du monde.
//
// Rend un lot VIDE quand le demi-côté est nul : une croix sans côté n'a pas
// d'aire, et en soumettre quatre paierait un lot de rendu pour rien.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeCrossSpikeQuads(
    const EffectCrossSpikeRequest& request, glm::vec3 node_origin,
    const EffectRingOrientation& orientation = kEffectRingUpright);

// LA COULEUR QUE LA CROIX À PART TIRE À CHAQUE IMAGE : un bleu plein et opaque,
// rouge puis vert tirés chacun sur onze valeurs au-dessus de quarante, dans cet
// ordre. Voir `EffectCrossSpikeRequest::draws_its_own_color`.
[[nodiscard]] std::array<std::uint8_t, 4> DrawCrossSpikeOwnColor(std::uint32_t& generator);

// CE QU'UN NŒUD DEMANDE, lu dans ses champs.
[[nodiscard]] EffectCrossSpikeRequest EffectCrossSpikeRequestOf(const EffectNode& node,
                                                                std::uint8_t track_mark);

// LA MÊME, POUR UNE IMAGE : la croix à part y tire sa couleur dans `generator`.
[[nodiscard]] EffectCrossSpikeRequest EffectCrossSpikeRequestOf(const EffectNode& node,
                                                                std::uint8_t track_mark,
                                                                std::uint32_t& generator);

}  // namespace ro::render

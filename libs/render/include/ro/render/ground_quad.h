#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"

namespace ro::render {

// UN QUAD POSÉ SUR LE SOL : une case de la grille, peinte par-dessus le
// terrain qu'elle épouse.
//
// C'est ainsi que le jeu marque le sol, et jamais autrement : curseur de
// destination, trace de navigation, case de l'homoncule posent tous un quad
// par cellule. Aucun
// n'est un sprite — un sprite fait face à la caméra, une marque de sol est
// couchée dedans.
//
// Description PURE, comme les quads d'effet : aucune texture n'est chargée
// ici, aucun contexte graphique n'est touché. C'est ce qui permet de vérifier
// la géométrie sans carte graphique.

// L'ORDRE DES COINS, et il vient du client :
//
//     2-----3     cellule y + 1        v = 1
//     |     |
//     0-----1     cellule y            v = 0
//
//    u = 0  u = 1
//
// L'indice 0 est le coin de plus petites coordonnées de CELLULE, le 3 celui de
// plus grandes. Ce n'est pas un choix : c'est la disposition des quatre
// hauteurs d'une case GAT (voir GatCell), et les coins doivent y répondre un à
// un sous peine de gauchir le quad sur le moindre relief.
struct GroundQuad {
  std::array<glm::vec3, 4> corners = {};
  std::array<glm::vec2, 4> texture_uv = {};
  std::array<std::uint8_t, 4> color = {255, 255, 255, 255};
};

// De combien un quad de sol passe DEVANT le sol, en profondeur normalisée.
//
// Le quad et le terrain occupent le même plan : sans écart, le tampon de
// profondeur tranche au hasard d'un pixel à l'autre et la marque se crible de
// trous qui bougent avec la caméra.
//
// Le client retranche un trente-deux-millième à la profondeur d'écran de
// chacun des quatre coins. Sa profondeur va de zéro à un, celle d'OpenGL de
// moins un à un : d'où le doublement, qui rend le même écart réel.
inline constexpr float kGroundQuadDepthBias = 2.0F / 32768.0F;

// Le quad d'UNE cellule de la grille, dans le repère du monde.
//
// ⚠ LES HAUTEURS VIENNENT DE LA GRILLE, pas du terrain visible. C'est le sol
// sur lequel on MARCHE, et les deux diffèrent là où la carte fait passer
// au-dessus du décor — un pont, une arche. Le client fait de même : son
// emprise vient du .gnd, ses quatre hauteurs du .gat.
//
// `fill` est la PART DE LA CASE que le quad occupe, depuis son centre : un
// laisse la cellule entière, et moins creuse un joint tout autour. C'est ce qui
// sépare visuellement deux cases voisines d'une même zone, là où des carreaux
// jointifs feraient un seul aplat.
//
// ⚠ LES QUATRE HAUTEURS RESTENT CELLES DES COINS DE LA CASE, non interpolées :
// un carreau rétréci garde donc l'inclinaison de sa cellule, et non celle du
// terrain sous ses nouveaux coins. L'écart ne se voit que sur une forte pente,
// et l'interpoler demanderait au quad de connaître ses voisines.
//
// Rend nullopt hors de la grille.
[[nodiscard]] std::optional<GroundQuad> ComposeGroundCellQuad(
    const formats::GndMap& gnd, const formats::GatMap& gat, std::int32_t cell_x,
    std::int32_t cell_y, const std::array<std::uint8_t, 4>& color, float fill = 1.0F);

}  // namespace ro::render

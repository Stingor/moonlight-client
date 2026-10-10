#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"

namespace ro::render {

// LA LUMIÈRE QUE LA CARTE A PEINTE, relue sous les pieds d'un acteur.
//
// Le sol reçoit sa lightmap texel par texel ; un sprite, lui, n'en reçoit
// qu'UNE couleur par cellule, qui multiplie la sienne. C'est ce qui assombrit un
// joueur passé sous un arbre et le dore sous une lanterne.
//
// Une cellule par CASE DE MARCHE — deux par carreau de sol dans chaque sens —,
// et trois facteurs par cellule, rouge, vert et bleu, qu'un blanc laisse
// intacts.
struct GroundLight {
  std::int32_t width = 0;
  std::int32_t height = 0;
  // Rangée par rangée, `width` cellules chacune.
  std::vector<std::array<float, 3>> factors;

  // Les facteurs d'une cellule. ⚠ BLANC HORS DE LA CARTE, et non noir : un
  // acteur qui déborde d'une case ne doit pas s'éteindre.
  [[nodiscard]] std::array<float, 3> At(std::int32_t cell_x, std::int32_t cell_y) const;

  // Ceux de la cellule qui contient un point de la grille. ⚠ UNE CELLULE
  // ENTIÈRE DÉSIGNE SON CENTRE : 10,4 et 9,6 sont tous deux dans la dixième.
  [[nodiscard]] std::array<float, 3> Under(float cell_x, float cell_y) const;
};

// Calcule la lumière de chaque cellule.
//
// Une cellule lit CINQ texels de lightmap, le sien et ses quatre voisins en
// croix, et en fait la moyenne : l'ombre compte pour sa part sur 255, la
// couleur posée s'y AJOUTE sur 128. Si un canal dépasse un, les trois sont
// ramenés d'autant — la teinte se garde, seule la force plafonne.
//
// 🔴 UNE CELLULE PERCHÉE GARDE LE BLANC. Quand la case de marche passe au-dessus
// du sol de plus de deux unités et demie — un pont, un toit —, la lightmap du
// dessous ne la concerne pas : l'ombre d'une rive ne doit pas noircir qui
// traverse le pont au-dessus.
[[nodiscard]] GroundLight BuildGroundLight(const formats::GndMap& gnd, const formats::GatMap& gat);

// Une teinte d'acteur prise dans cette lumière : le rouge, le vert et le bleu
// multipliés par les facteurs de sa cellule, l'alpha intact.
//
// `cold` échange le rouge et le bleu de la lumière, comme le fait la lumière
// froide sur le sol : un sprite doit rester sous la même lumière que le sol où
// il se tient.
[[nodiscard]] std::array<std::uint8_t, 4> LitTint(std::array<std::uint8_t, 4> tint,
                                                  const std::array<float, 3>& factors, bool cold);

}  // namespace ro::render

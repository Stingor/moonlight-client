#include "ro/render/ground_quad.h"

#include <algorithm>
#include <cstddef>

#include "ro/render/terrain_mesh.h"

namespace ro::render {

std::optional<GroundQuad> ComposeGroundCellQuad(const formats::GndMap& gnd,
                                                const formats::GatMap& gat, std::int32_t cell_x,
                                                std::int32_t cell_y,
                                                const std::array<std::uint8_t, 4>& color,
                                                float fill) {
  if (!gat.InBounds(cell_x, cell_y)) {
    return std::nullopt;
  }
  // Le centre passe par la MÊME conversion que celle qui pose les acteurs. La
  // recalculer ici la laisserait dériver, et la marque désignerait alors une
  // case pendant que le personnage en viserait une autre.
  const std::array<float, 3> center =
      CellPositionAt(gnd, static_cast<float>(cell_x), static_cast<float>(cell_y));
  // La demi-case, moins le joint que `fill` demande. Voir sa déclaration : la
  // part se prend depuis le CENTRE, et se borne pour qu'un réglage aberrant
  // venu d'un fichier de configuration ne retourne pas le quad.
  const float kHalfCell = kWorldUnitsPerCell / 2.0F * std::clamp(fill, 0.0F, 1.0F);

  const std::array<float, 4>& height = gat.at(cell_x, cell_y).corner_height;
  GroundQuad quad;
  quad.color = color;
  for (std::size_t corner = 0; corner < quad.corners.size(); ++corner) {
    const bool at_greater_x = corner % 2 == 1;
    const bool at_greater_cell_y = corner >= 2;
    const float toward_x = at_greater_x ? kHalfCell : -kHalfCell;
    // UNE CELLULE DE PLUS, C'EST UN Z DE MOINS : même inversion que pour les
    // rangées de cubes du terrain, et la manquer retournerait le quad d'avant
    // en arrière — invisible sur un sol plat, faux dès la première pente.
    const float toward_z = at_greater_cell_y ? -kHalfCell : kHalfCell;
    quad.corners[corner] = glm::vec3{center[0] + toward_x,
                                     // L'axe vertical du monde monte, celui du fichier descend.
                                     -height[corner], center[2] + toward_z};
    quad.texture_uv[corner] =
        glm::vec2{at_greater_x ? 1.0F : 0.0F, at_greater_cell_y ? 1.0F : 0.0F};
  }
  return quad;
}

}  // namespace ro::render

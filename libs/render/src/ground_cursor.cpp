#include "ro/render/ground_cursor.h"

#include "ro/render/cursor.h"

namespace ro::render {

std::optional<GroundQuad> ComposeGroundCursor(const formats::GndMap& gnd,
                                              const formats::GatMap& gat, std::int32_t cell_x,
                                              std::int32_t cell_y) {
  if (!gat.InBounds(cell_x, cell_y)) {
    return std::nullopt;
  }
  // La MÊME règle que celle qui décide de la forme du curseur de souris : une
  // case interdite montre le panneau d'interdiction, et ne montre pas de cadre.
  // Les séparer permettrait au client d'annoncer un pas et de le refuser du
  // même geste.
  if (CellRefusesWalk(gat.at(cell_x, cell_y).type)) {
    return std::nullopt;
  }
  return ComposeGroundCellQuad(gnd, gat, cell_x, cell_y, kGroundCursorColor);
}

}  // namespace ro::render

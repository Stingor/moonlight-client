#include "ro/render/ground_cursor.h"

#include "ro/render/cursor.h"

namespace ro::render {

std::optional<GroundQuad> ComposeGroundCursor(const formats::GndMap& gnd,
                                              const formats::GatMap& gat, std::int32_t cell_x,
                                              std::int32_t cell_y) {
  if (!gat.InBounds(cell_x, cell_y)) {
    return std::nullopt;
  }
  // Une case interdite montre le panneau d'interdiction, et ne montre pas de
  // cadre : annoncer un pas et le refuser du même geste serait mentir. Ici, c'est
  // le type du FICHIER qui refuse ; ce que la grille de marche bloque en plus —
  // une case du document d'édition, un mur de script — est écarté par l'appelant,
  // qui tient cette grille.
  if (CellRefusesWalk(gat.at(cell_x, cell_y).type)) {
    return std::nullopt;
  }
  return ComposeGroundCellQuad(gnd, gat, cell_x, cell_y, kGroundCursorColor);
}

}  // namespace ro::render

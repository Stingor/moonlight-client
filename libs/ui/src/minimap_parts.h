#pragma once

#include <array>

#include <imgui.h>

#include "ro/ui/minimap_window.h"

namespace ro::ui {

// Les quatre pointes d'un losange, dans le sens des aiguilles d'une montre.
[[nodiscard]] inline std::array<ImVec2, 4> Diamond(ImVec2 center, float half) {
  return {ImVec2(center.x, center.y - half), ImVec2(center.x + half, center.y),
          ImVec2(center.x, center.y + half), ImVec2(center.x - half, center.y)};
}

// LE CADRAGE D'UNE IMAGE DE LA MINIMAP : où tombe la carte à l'écran, et
// quelle portion on en voit.
//
// u part de la gauche ; v part du HAUT, et la ligne du haut de l'image est la
// cellule de plus grand Y — d'où l'inversion de l'axe vertical partout ici.
struct MinimapFrame {
  // Le carré réservé à la carte.
  ImVec2 origin;
  float side = 0.0F;
  // Le rectangle où l'image est peinte, dans ce carré.
  ImVec2 low;
  ImVec2 high;
  float draw_width = 0.0F;
  float draw_height = 0.0F;
  // La portion visible de la carte, en fraction de sa taille.
  MinimapSpan u;
  MinimapSpan v;
  // Le joueur, en fraction de la carte.
  float player_u = 0.5F;
  float player_v = 0.5F;
  // La taille de la carte est connue : les marqueurs se situent.
  bool located = false;
  int cells_width = 0;
  int cells_height = 0;
};

[[nodiscard]] MinimapFrame FrameMinimap(const MinimapView& view, float zoom, ImVec2 origin,
                                        float side);

// La cellule à l'écran, DANS la portion visible ; faux quand elle en sort — un
// coéquipier hors du cadrage ne s'écrase pas contre le bord.
[[nodiscard]] bool CellOnMinimap(const MinimapFrame& frame, int cell_x, int cell_y, ImVec2* at);
// La même, sans test de cadrage : une ligne brisée garde ainsi la pente exacte
// de ses segments d'entrée et de sortie, et la découpe se fait au dessin.
[[nodiscard]] ImVec2 CellOnMinimapUnbounded(const MinimapFrame& frame, float cell_x, float cell_y);
// L'inverse exact, zoom compris : la cellule sous un point de l'écran.
[[nodiscard]] MinimapCell MinimapCellAt(const MinimapFrame& frame, ImVec2 point);

// LES COÉQUIPIERS DU CHAMP DE BATAILLE, en losanges ; interactive, le survol
// dit le nom. `half` est la demi-taille de la flèche du joueur.
void DrawBattleMates(ImDrawList* into, const MinimapFrame& frame, const MinimapView& view,
                     float half, bool interactive);

// L'image, les marqueurs, l'itinéraire et le joueur, dans cet ordre. Rend dans
// `answer` le guidage demandé par un clic sur une commodité.
void DrawMinimapLayers(const MinimapFrame& frame, const MinimapView& view,
                       const MinimapSettings& settings, bool interactive, MinimapAnswer& answer);

}  // namespace ro::ui

// Les coéquipiers du champ de bataille sur la minimap. Voir `minimap_parts.h`.

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include <imgui.h>

#include "ro/ui/skin_tooltip.h"

#include "minimap_parts.h"

namespace ro::ui {
namespace {

// LE LOSANGE D'UN COÉQUIPIER, en part de la demi-étendue de la flèche, avec son
// plancher en pixels : un peu plus grand que le carré d'un membre de groupe,
// pour se lire d'un coup d'œil au milieu d'une mêlée.
constexpr float kBattleMateShare = 0.7F;
constexpr float kBattleMateLeast = 3.0F;
// 🔴 CERNÉ DE SOMBRE, là où le groupe et la guilde sont cernés de blanc : la
// forme ET le liseré distinguent un coéquipier de camp d'un membre de groupe
// qui serait de la même couleur.
constexpr ImU32 kBattleMateOutline = IM_COL32(20, 20, 20, 230);
constexpr float kBattleMateOutlineWidth = 1.0F;

// Le point est-il DANS le losange ? La distance de Manhattan au centre le dit.
[[nodiscard]] bool InsideDiamond(ImVec2 point, ImVec2 center, float half) {
  return std::fabs(point.x - center.x) + std::fabs(point.y - center.y) <= half;
}

}  // namespace

void DrawBattleMates(ImDrawList* into, const MinimapFrame& frame, const MinimapView& view,
                     float half, bool interactive) {
  const float size = std::max(half * kBattleMateShare, kBattleMateLeast);
  // 🔴 LE SURVOL SANS ITEM : un coéquipier ne se clique pas, et un bouton
  // invisible volerait au corps de la minimap son glisser et son clic droit.
  // ⚠ ET SEULEMENT SI RIEN D'AUTRE N'EST SURVOLÉ : une commodité ou une tombe
  // sous la souris garde sa propre bulle.
  const bool hovering = interactive && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered();
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  // Plusieurs coéquipiers sur la même case : la bulle les nomme tous.
  std::string names;
  for (const MinimapBattleMate& mate : view.battle_mates) {
    ImVec2 at;
    if (!CellOnMinimap(frame, mate.cell.x, mate.cell.y, &at)) {
      continue;
    }
    const std::array<ImVec2, 4> shape = Diamond(at, size);
    into->AddConvexPolyFilled(shape.data(), static_cast<int>(shape.size()), mate.color);
    into->AddPolyline(shape.data(), static_cast<int>(shape.size()), kBattleMateOutline,
                      ImDrawFlags_Closed, kBattleMateOutlineWidth);
    if (hovering && !mate.name.empty() && InsideDiamond(mouse, at, size)) {
      if (!names.empty()) {
        names.push_back('\n');
      }
      names.append(mate.name);
    }
  }
  if (!names.empty()) {
    SkinTooltipFormat("%s", names.c_str());
  }
}

}  // namespace ro::ui

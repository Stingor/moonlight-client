#include "thumbnail_grid.h"

#include <cstddef>
#include <cstdio>

#include "ro/ui/skin_text.h"

namespace ro::ui {
namespace {

constexpr float kCellRounding = 3.0F;
constexpr float kNumberInset = 2.0F;
constexpr float kNumberLift = 1.0F;
constexpr float kChosenBorder = 2.0F;
constexpr float kHoverBorder = 1.5F;
constexpr ImU32 kCellBackground = IM_COL32(0, 0, 0, 40);
constexpr ImU32 kChosenBackground = IM_COL32(255, 220, 140, 55);
constexpr ImU32 kChosenBorderColor = IM_COL32(255, 210, 110, 255);
constexpr ImU32 kHoverBorderColor = IM_COL32(200, 210, 235, 160);
// Le numéro reste lisible sur un sprite clair grâce à son ombre portée.
constexpr ImU32 kNumberColor = IM_COL32(255, 255, 255, 235);
constexpr ImU32 kNumberShadow = IM_COL32(0, 0, 0, 200);

}  // namespace

ThumbnailChoice DrawThumbnailGrid(const ThumbnailGrid& grid) {
  ThumbnailChoice choice;
  ImDrawList* into = ImGui::GetWindowDrawList();
  for (int value = grid.first; value <= grid.last; ++value) {
    if ((value - grid.first) % grid.columns != 0) {
      ImGui::SameLine();
    }
    ImGui::PushID(value);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const ImVec2 to(at.x + grid.cell, at.y + grid.cell);
    const bool mine = grid.chosen == value;
    into->AddRectFilled(at, to, mine ? kChosenBackground : kCellBackground, kCellRounding);

    // ⚠ LA GARDE DE VISIBILITÉ D'ABORD : une grille de cinq cent cinquante-trois
    // cases n'en montre qu'une centaine, et composer les autres coûterait des
    // textures que personne ne regarde.
    if (ImGui::IsRectVisible(ImVec2(grid.cell, grid.cell))) {
      if (choice.seen_count == 0) {
        choice.seen_first = value;
      }
      choice.seen_count = value - choice.seen_first + 1;
      const auto rank = static_cast<std::size_t>(value - grid.thumbnails_first);
      if (value >= grid.thumbnails_first && rank < grid.thumbnails.size() &&
          grid.thumbnails[rank].usable()) {
        const SkinImage& image = grid.thumbnails[rank];
        const ImVec2 image_at(at.x + (grid.cell - image.width) * 0.5F,
                              at.y + (grid.cell - image.height) * 0.5F);
        into->AddImage(image.id, image_at,
                       ImVec2(image_at.x + image.width, image_at.y + image.height));
      }
    }

    char number[8] = {};
    std::snprintf(number, sizeof(number), "%d", value);
    const ImVec2 measured = ImGui::CalcTextSize(number);
    AddTextRelief(into, ImVec2(at.x + kNumberInset, at.y + grid.cell - measured.y - kNumberLift),
                  kNumberColor, number, kNumberShadow, ImVec2(1.0F, 1.0F));

    ImGui::InvisibleButton("c", ImVec2(grid.cell, grid.cell));
    if (ImGui::IsItemClicked()) {
      choice.picked = value;
    }
    // Le premier clic du double a déjà posé la valeur juste au-dessus ;
    // celui-ci n'ajoute que le congé. On repose quand même le numéro : rien ne
    // garantit que les deux clics ont visé la même case.
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
      choice.picked = value;
      choice.close = true;
    }
    if (mine) {
      into->AddRect(at, to, kChosenBorderColor, kCellRounding, 0, kChosenBorder);
    } else if (ImGui::IsItemHovered()) {
      into->AddRect(at, to, kHoverBorderColor, kCellRounding, 0, kHoverBorder);
    }
    ImGui::PopID();
  }
  if (choice.seen_count == 0) {
    choice.seen_first = grid.first;
  }
  return choice;
}

}  // namespace ro::ui

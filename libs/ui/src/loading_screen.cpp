#include "ro/ui/loading_screen.h"

namespace ro::ui {

void DrawLoadingScreen(const char* text) {
  // ⚠ SUR LA LISTE DE FOND, et non dans une fenêtre : une fenêtre se déplace,
  // prend le focus et se range dans le fichier d'ImGui. Cet écran n'est rien de
  // tout cela, et il doit couvrir la totalité du cadre.
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  ImDrawList* list = ImGui::GetBackgroundDrawList();
  const ImVec2 corner(screen->Pos.x + screen->Size.x, screen->Pos.y + screen->Size.y);
  list->AddRectFilled(screen->Pos, corner, kLoadingBackground);
  if (text == nullptr || text[0] == '\0') {
    return;
  }
  const ImVec2 size = ImGui::CalcTextSize(text);
  const ImVec2 at(screen->Pos.x + (screen->Size.x - size.x) * 0.5F,
                  screen->Pos.y + (screen->Size.y - size.y) * 0.5F);
  list->AddText(at, kLoadingTextColor, text);
}

}  // namespace ro::ui

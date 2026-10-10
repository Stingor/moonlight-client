#include "ro/ui/patch_screen.h"

#include <algorithm>

#include <imgui.h>

#include "ro/ui/loading_screen.h"

namespace ro::ui {
namespace {

// La largeur du panneau, rapportée à celle de l'écran.
constexpr float kPanelWidthShare = 0.5F;
constexpr float kPanelMinWidthPx = 360.0F;
constexpr float kCentered = 0.5F;

constexpr ImGuiWindowFlags kPanelFlags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground;

}  // namespace

PatchScreenAnswers DrawPatchScreen(const PatchScreenView& view) {
  DrawLoadingScreen("");
  PatchScreenAnswers answers;
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  if (view.backdrop.usable()) {
    ImGui::GetBackgroundDrawList()->AddImage(
        view.backdrop.id, screen->Pos,
        ImVec2(screen->Pos.x + screen->Size.x, screen->Pos.y + screen->Size.y));
  }
  const float panel_width = std::max(screen->Size.x * kPanelWidthShare, kPanelMinWidthPx);
  ImGui::SetNextWindowPos(ImVec2(screen->Pos.x + screen->Size.x * kCentered,
                                 screen->Pos.y + screen->Size.y * kCentered),
                          ImGuiCond_Always, ImVec2(kCentered, kCentered));
  ImGui::SetNextWindowSize(ImVec2(panel_width, 0.0F));
  if (ImGui::Begin("##patch", nullptr, kPanelFlags)) {
    ImGui::PushStyleColor(ImGuiCol_Text, kLoadingTextColor);
    ImGui::TextUnformatted(view.headline);
    ImGui::PopStyleColor();
    ImGui::Spacing();
    if (!view.failed) {
      // Indéterminé : une barre qui avance d'elle-même, pour dire que rien
      // n'est figé.
      const float fraction =
          view.fraction >= 0.0F ? view.fraction : -1.0F * static_cast<float>(ImGui::GetTime());
      ImGui::ProgressBar(fraction, ImVec2(-1.0F, 0.0F), "");
    }
    ImGui::TextWrapped("%s", view.detail);
    if (view.failed) {
      ImGui::Spacing();
      answers.retry = ImGui::Button(view.retry_label);
      ImGui::SameLine();
      answers.play_anyway = ImGui::Button(view.play_label);
      ImGui::SameLine();
      answers.quit = ImGui::Button(view.quit_label);
    }
  }
  ImGui::End();
  return answers;
}

}  // namespace ro::ui

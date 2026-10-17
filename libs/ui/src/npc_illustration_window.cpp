#include "ro/ui/npc_illustration_window.h"

#include "ro/ui/skinned_window.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// ⚠ DES IDENTIFIANTS, JAMAIS TRADUITS : ImGui range la position d'une fenêtre
// sous son nom, et une langue changée en ferait une autre fenêtre.
constexpr const char* kBottomId = "##npc_illustration_bottom";
constexpr const char* kWindowedId = "###npc_illustration_window";
constexpr const char* kFloatingId = "##npc_illustration_floating";
constexpr const char* kCloseButtonId = "X##npc_illustration_close";

// Le bouton de fermeture de l'illustration sans cadre : à ce retrait du bord
// droit et du haut, en pixels d'interface — la place que lui donne le client.
constexpr float kCloseInsetRight = 16.0F;
constexpr float kCloseInsetTop = 3.0F;

[[nodiscard]] bool IsBottom(IllustrationAnchor anchor) {
  return anchor == IllustrationAnchor::kBottomLeft || anchor == IllustrationAnchor::kBottomMiddle ||
         anchor == IllustrationAnchor::kBottomRight;
}

}  // namespace

ImVec2 IllustrationTopLeft(IllustrationAnchor anchor, ImVec2 image_size, ImVec2 screen_origin,
                           ImVec2 screen_size, float scale) {
  float center_x = screen_origin.x + screen_size.x * 0.5F;
  if (anchor == IllustrationAnchor::kBottomLeft) {
    center_x = screen_origin.x + kIllustrationSideCenterPixels * scale;
  } else if (anchor == IllustrationAnchor::kBottomRight) {
    center_x = screen_origin.x + screen_size.x - kIllustrationSideCenterPixels * scale;
  }
  const float top = IsBottom(anchor) ? screen_origin.y + screen_size.y - image_size.y
                                     : screen_origin.y + (screen_size.y - image_size.y) * 0.5F;
  return ImVec2(center_x - image_size.x * 0.5F, top);
}

NpcIllustrationAction DrawNpcIllustration(const NpcIllustrationView& view, const SkinArt& art) {
  NpcIllustrationAction action;
  if (!view.image.usable()) {
    return action;
  }
  const float scale = Scale();
  const ImVec2 size(view.image.width * scale, view.image.height * scale);
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  const ImVec2 corner =
      IllustrationTopLeft(view.anchor, size, screen->WorkPos, screen->WorkSize, scale);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  if (IsBottom(view.anchor)) {
    // 🔴 `NoInputs` : une illustration du bas se tient DEVANT le monde sans le
    // cacher à la souris. On clique à travers, comme chez le client d'origine.
    ImGui::SetNextWindowPos(corner, ImGuiCond_Always);
    if (ImGui::Begin(kBottomId, nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoFocusOnAppearing)) {
      ImGui::Image(view.image.id, size);
    }
    ImGui::End();
  } else if (view.anchor == IllustrationAnchor::kCenteredWindow) {
    // Au milieu À SON APPARITION, puis là où le joueur la pose.
    ImGui::SetNextWindowPos(corner, ImGuiCond_Appearing);
    bool open = true;
    const bool begun =
        BeginSkinnedWindow(kWindowedId, &open, art, SkinnedTitle::kBar,
                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse |
                               ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings);
    if (begun) {
      ImGui::Image(view.image.id, size);
    }
    EndSkinnedWindow();
    action.closed = !open;
  } else {
    ImGui::SetNextWindowPos(corner, ImGuiCond_Appearing);
    if (ImGui::Begin(kFloatingId, nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                         ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Image(view.image.id, size);
      ImGui::SetCursorPos(ImVec2(size.x - kCloseInsetRight * scale, kCloseInsetTop * scale));
      action.closed = ImGui::SmallButton(kCloseButtonId);
    }
    ImGui::End();
  }
  ImGui::PopStyleVar(2);
  return action;
}

}  // namespace ro::ui

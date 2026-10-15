// Le cadre « panneau de description » : papier et pièces `sysbox`, autour d'un
// rectangle quelconque ou de la fenêtre en cours. Voir `skinned_window.h`.

#include <imgui_internal.h>

#include "ro/ui/skin_art.h"
#include "ro/ui/skinned_window.h"

namespace ro::ui {
namespace {

// L'échelle courante de l'interface : elle décide de la taille des pièces.
[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

// Blitte une pièce du cadre au papier dans un rectangle, ou rien sans l'art.
void Blit(ImDrawList* into, const SkinArt& art, const SkinImage& piece, ImVec2 from, ImVec2 to) {
  if (piece.usable()) {
    PaintFrameOn(into, piece.id, from, to, art.look().paper);
  }
}

}  // namespace

float DescPanelEdge(const SkinArt& art) {
  return art.Get(render::SkinPiece::kFrameLeft).width * Scale();
}

void DrawDescPanelFrame(ImDrawList* into, const SkinArt& art, ImVec2 top_left, ImVec2 bottom_right,
                        bool fill_bg) {
  if (into == nullptr || bottom_right.x <= top_left.x || bottom_right.y <= top_left.y) {
    return;
  }
  const float edge = DescPanelEdge(art);
  // 🔴 LE CORPS NE PASSE PAS SOUS LES PIÈCES : il s'arrête à leur bord intérieur.
  // Les coins sont arrondis par leur image, dont l'extérieur est transparent ;
  // un aplat étendu jusqu'au bord remplirait cet extérieur et rendrait les
  // coins carrés.
  if (fill_bg && bottom_right.x - top_left.x > 2.0F * edge &&
      bottom_right.y - top_left.y > 2.0F * edge) {
    into->AddRectFilled(ImVec2(top_left.x + edge, top_left.y + edge),
                        ImVec2(bottom_right.x - edge, bottom_right.y - edge), art.look().paper);
  }
  if (edge <= 0.0F) {
    return;
  }
  const float left = top_left.x;
  const float right = bottom_right.x;
  const float top = top_left.y;
  const float bottom = bottom_right.y;
  // ⚠ CROISÉ AVEC LE DÉCOUPAGE EN COURS : voir l'en-tête.
  into->PushClipRect(top_left, bottom_right, true);
  Blit(into, art, art.Get(render::SkinPiece::kFrameTopLeft), ImVec2(left, top),
       ImVec2(left + edge, top + edge));
  Blit(into, art, art.Get(render::SkinPiece::kFrameTopRight), ImVec2(right - edge, top),
       ImVec2(right, top + edge));
  Blit(into, art, art.Get(render::SkinPiece::kFrameBottomLeft), ImVec2(left, bottom - edge),
       ImVec2(left + edge, bottom));
  Blit(into, art, art.Get(render::SkinPiece::kFrameBottomRight),
       ImVec2(right - edge, bottom - edge), ImVec2(right, bottom));
  Blit(into, art, art.Get(render::SkinPiece::kFrameTopMiddle), ImVec2(left + edge, top),
       ImVec2(right - edge, top + edge));
  Blit(into, art, art.Get(render::SkinPiece::kFrameBottomMiddle),
       ImVec2(left + edge, bottom - edge), ImVec2(right - edge, bottom));
  Blit(into, art, art.Get(render::SkinPiece::kFrameLeft), ImVec2(left, top + edge),
       ImVec2(left + edge, bottom - edge));
  Blit(into, art, art.Get(render::SkinPiece::kFrameRight), ImVec2(right - edge, top + edge),
       ImVec2(right, bottom - edge));
  into->PopClipRect();
}

void DrawDescPanelFrameAroundWindow(ImDrawList* into, const SkinArt& art, bool fill_bg) {
  const ImGuiWindow* const window = ImGui::GetCurrentWindowRead();
  if (into == nullptr || window == nullptr) {
    return;
  }
  into->PushClipRect(window->OuterRectClipped.Min, window->OuterRectClipped.Max, false);
  DrawDescPanelFrame(into, art, window->Pos,
                     ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y),
                     fill_bg);
  into->PopClipRect();
}

}  // namespace ro::ui

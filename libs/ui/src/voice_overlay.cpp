#include "ro/ui/voice_overlay.h"

#include <cmath>
#include <span>

#include <imgui.h>

#include "ui_scale.h"

namespace ro::ui {
namespace {

// Le fond d'une ligne : sombre et translucide, pour que le monde reste lisible
// dessous ; le texte blanc sur une ombre noire, comme les bandeaux.
constexpr ImU32 kLineBackground = IM_COL32(16, 18, 24, 170);
constexpr ImU32 kLineText = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kNoticeText = IM_COL32(255, 214, 120, 255);
constexpr ImU32 kShadow = IM_COL32(0, 0, 0, 255);
// La pastille de qui parle : verte, plus vive pour soi.
constexpr ImU32 kSpeakingDot = IM_COL32(90, 200, 110, 255);
constexpr ImU32 kSelfDot = IM_COL32(120, 255, 140, 255);
constexpr float kRounding = 3.0F;
constexpr float kPaddingPixels = 4.0F;
constexpr float kLineGapPixels = 3.0F;
constexpr float kDotRadiusPixels = 4.0F;
constexpr float kShadowOffsetPixels = 1.0F;

// Une ligne à la hauteur `y`, rend la hauteur de la suivante.
float DrawLine(ImDrawList* into, float y, const std::string& text, ImU32 color, ImU32 dot) {
  const float scale = Scale();
  const float padding = kPaddingPixels * scale;
  const float radius = kDotRadiusPixels * scale;
  const ImVec2 size = ImGui::CalcTextSize(text.c_str());
  const float x = std::trunc(kVoiceOverlayLeftPixels * scale);
  const float dot_width = dot != 0 ? radius * 2.0F + padding : 0.0F;
  into->AddRectFilled(ImVec2(x, y),
                      ImVec2(x + padding * 2.0F + dot_width + size.x, y + padding * 2.0F + size.y),
                      kLineBackground, kRounding * scale);
  if (dot != 0) {
    into->AddCircleFilled(ImVec2(x + padding + radius, y + padding + size.y * 0.5F), radius, dot);
  }
  const ImVec2 at(x + padding + dot_width, y + padding);
  const float shadow = kShadowOffsetPixels * scale;
  into->AddText(ImVec2(at.x + shadow, at.y + shadow), kShadow, text.c_str());
  into->AddText(at, color, text.c_str());
  return y + padding * 2.0F + size.y + kLineGapPixels * scale;
}

}  // namespace

void DrawVoiceOverlay(const VoiceOverlayState& state) {
  if (state.speaking.empty() && state.self_talking.empty() && state.notice.empty()) {
    return;
  }
  ImDrawList* into = ImGui::GetForegroundDrawList();
  float y = std::trunc(ImGui::GetIO().DisplaySize.y * kVoiceOverlayTopRatio);
  if (!state.notice.empty()) {
    y = DrawLine(into, y, state.notice, kNoticeText, 0);
  }
  if (!state.self_talking.empty()) {
    y = DrawLine(into, y, state.self_talking, kLineText, kSelfDot);
  }
  for (const std::string& name : state.speaking) {
    y = DrawLine(into, y, name, kLineText, kSpeakingDot);
  }
}

void DrawVoiceMarks(std::span<const VoiceMarkView> marks, const SkinImage& icon) {
  if (marks.empty() || !icon.usable()) {
    return;
  }
  const float scale = Scale();
  const float side = std::round(kVoiceMarkPixels * scale);
  const float lift = kVoiceMarkLiftPixels * scale;
  ImDrawList* into = ImGui::GetBackgroundDrawList();
  for (const VoiceMarkView& mark : marks) {
    // Arrondie au pixel : une icône à cheval sur deux pixels se brouille.
    const ImVec2 min(std::round(mark.head.x - side * 0.5F), std::round(mark.head.y - lift - side));
    into->AddImage(icon.id, min, ImVec2(min.x + side, min.y + side));
  }
}

}  // namespace ro::ui

#include "ro/ui/voice_overlay.h"

#include <cmath>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/ui/skin_widgets.h"

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

// ── LA BARRE ──
// Un bouton d'icône et la hauteur d'une pastille, à l'échelle un de la barre.
constexpr float kBarButtonPixels = 26.0F;
constexpr float kBarIconPadPixels = 3.0F;
constexpr float kPillPadPixels = 8.0F;
constexpr float kDefaultBorderPixels = 2.0F;
constexpr float kSlashPixels = 2.5F;
// Une pastille écoutée est pleine, une autre éteinte ; celle où part sa propre
// voix est verte ; le canal par défaut est cerclé d'or.
constexpr ImU32 kPillListened = IM_COL32(70, 110, 190, 230);
constexpr ImU32 kPillIdle = IM_COL32(60, 62, 70, 200);
constexpr ImU32 kPillTalking = IM_COL32(60, 160, 80, 240);
constexpr ImU32 kPillHover = IM_COL32(255, 255, 255, 40);
constexpr ImU32 kDefaultBorder = IM_COL32(240, 200, 90, 255);
constexpr ImU32 kIdleText = IM_COL32(170, 172, 180, 255);
constexpr ImU32 kSlashColor = IM_COL32(230, 60, 60, 255);
constexpr ImU32 kButtonOn = IM_COL32(150, 40, 40, 220);
constexpr ImU32 kNoBorder = IM_COL32(0, 0, 0, 0);
// L'opacité d'une barre estompée, sur celle qu'on a réglée.
constexpr float kFadedOpacity = 0.35F;

// Un bouton d'icône : l'image, barrée de rouge quand il est enclenché. Rend
// vrai au clic.
bool IconButton(const char* id, const SkinImage& icon, bool engaged, const std::string& tip,
                float side) {
  const bool pressed = ImGui::InvisibleButton(id, ImVec2(side, side));
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  ImDrawList* into = ImGui::GetWindowDrawList();
  const float rounding = kRounding * Scale();
  into->AddRectFilled(min, max, ImGui::GetColorU32(engaged ? kButtonOn : kPillIdle), rounding);
  if (ImGui::IsItemHovered()) {
    into->AddRectFilled(min, max, ImGui::GetColorU32(kPillHover), rounding);
    SkinTooltip(tip.c_str());
  }
  const float pad = kBarIconPadPixels * Scale();
  if (icon.usable()) {
    into->AddImage(icon.id, ImVec2(min.x + pad, min.y + pad), ImVec2(max.x - pad, max.y - pad),
                   ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F), ImGui::GetColorU32(IM_COL32_WHITE));
  }
  if (engaged) {
    into->AddLine(ImVec2(min.x + pad, max.y - pad), ImVec2(max.x - pad, min.y + pad),
                  ImGui::GetColorU32(kSlashColor), kSlashPixels * Scale());
  }
  return pressed;
}

// Une pastille de canal. Rend ce qu'on y a fait.
void ChannelPill(const VoiceBarChannel& channel, int rank, float height, VoiceBarAnswer& answer) {
  const float pad = kPillPadPixels * Scale();
  const ImVec2 text = ImGui::CalcTextSize(channel.name.c_str());
  ImGui::PushID(rank);
  ImGui::InvisibleButton("##canal", ImVec2(text.x + pad * 2.0F, height),
                         ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  ImGui::PopID();
  if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) answer.toggle_listen = rank;
  if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) answer.make_default = rank;
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  ImDrawList* into = ImGui::GetWindowDrawList();
  const float rounding = height * 0.5F;
  const ImU32 fill = channel.talking ? kPillTalking : channel.listened ? kPillListened : kPillIdle;
  into->AddRectFilled(min, max, ImGui::GetColorU32(fill), rounding);
  if (ImGui::IsItemHovered()) {
    into->AddRectFilled(min, max, ImGui::GetColorU32(kPillHover), rounding);
    SkinTooltip(channel.tip.c_str());
  }
  if (channel.is_default) {
    into->AddRect(min, max, ImGui::GetColorU32(kDefaultBorder), rounding, 0,
                  kDefaultBorderPixels * Scale());
  }
  into->AddText(ImVec2(min.x + pad, min.y + (height - text.y) * 0.5F),
                ImGui::GetColorU32(channel.listened || channel.talking ? kLineText : kIdleText),
                channel.name.c_str());
}

// Qui parle, dans la barre : une pastille et un nom par ligne.
void BarSpeaker(const std::string& name, ImU32 dot) {
  const float radius = kDotRadiusPixels * Scale();
  const ImVec2 at = ImGui::GetCursorScreenPos();
  const float line = ImGui::GetTextLineHeight();
  ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(at.x + radius, at.y + line * 0.5F), radius,
                                              ImGui::GetColorU32(dot));
  ImGui::SetCursorScreenPos(ImVec2(at.x + radius * 2.0F + kPaddingPixels * Scale(), at.y));
  ImGui::TextUnformatted(name.c_str());
}

void DrawVoiceBar(VoiceOverlayState& state) {
  const VoiceBarView& bar = state.bar;
  const bool someone = !state.speaking.empty() || !state.self_talking.empty();
  // ⚠ LE SURVOL EST CELUI DE L'IMAGE D'AVANT : l'opacité se pose avant que la
  // fenêtre ne sache si la souris est dessus.
  ImGuiStorage* const storage = ImGui::GetStateStorage();
  const ImGuiID hovered_key = ImHashStr(kVoiceBarWindowId);
  const bool was_hovered = storage->GetBool(hovered_key, false);
  const bool faded = bar.fade_idle && !someone && !was_hovered && state.notice.empty();
  const float opacity = bar.opacity * (faded ? kFadedOpacity : 1.0F);

  const ImGuiIO& io = ImGui::GetIO();
  ImGui::SetNextWindowPos(ImVec2(std::trunc(kVoiceOverlayLeftPixels * Scale()),
                                 std::trunc(io.DisplaySize.y * kVoiceOverlayTopRatio)),
                          ImGuiCond_FirstUseEver);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, opacity);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kRounding * Scale());
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(kPaddingPixels * Scale(), kPaddingPixels * Scale()));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, kLineBackground);
  ImGui::PushStyleColor(ImGuiCol_Border, kNoBorder);
  ImGui::PushStyleColor(ImGuiCol_Text, kLineText);
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_AlwaysAutoResize |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                 (bar.locked ? ImGuiWindowFlags_NoMove : ImGuiWindowFlags_None);
  if (ImGui::Begin(kVoiceBarWindowId, nullptr, flags)) {
    ImGui::SetWindowFontScale(bar.scale);
    const float side = std::round(kBarButtonPixels * Scale() * bar.scale);
    const auto next = [&bar] {
      if (!bar.vertical) ImGui::SameLine();
    };
    if (IconButton("##micro", bar.mic_icon, bar.self_muted, bar.mute_tip, side)) {
      state.bar_answer.toggle_mute = true;
    }
    next();
    if (IconButton("##sourdine", bar.speaker_icon, bar.deafened, bar.deafen_tip, side)) {
      state.bar_answer.toggle_deafen = true;
    }
    for (std::size_t rank = 0; rank < bar.channels.size(); ++rank) {
      next();
      ChannelPill(bar.channels[rank], static_cast<int>(rank), side, state.bar_answer);
    }
    if (!state.notice.empty()) {
      ImGui::PushStyleColor(ImGuiCol_Text, kNoticeText);
      ImGui::TextUnformatted(state.notice.c_str());
      ImGui::PopStyleColor();
    }
    if (bar.speakers) {
      if (!state.self_talking.empty()) BarSpeaker(state.self_talking, kSelfDot);
      for (const std::string& name : state.speaking) BarSpeaker(name, kSpeakingDot);
    }
  }
  const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
  ImGui::End();
  ImGui::PopStyleColor(3);
  ImGui::PopStyleVar(3);
  storage->SetBool(hovered_key, hovered);
}

}  // namespace

void DrawVoiceOverlay(VoiceOverlayState& state) {
  state.bar_answer = VoiceBarAnswer();
  if (state.bar.shown) {
    DrawVoiceBar(state);
    return;
  }
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

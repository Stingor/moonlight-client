#include "ro/ui/hud_caption.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace ro::ui {
namespace {

// L'ombre d'un texte de cadre, en part de sa taille, et jamais moins d'un pixel.
constexpr float kShadowShare = 0.08F;
constexpr float kLeastShadow = 1.0F;
constexpr ImU32 kCaptionShadow = IM_COL32(0, 0, 0, 200);

constexpr float kHalf = 0.5F;
constexpr std::size_t kClippedRoom = 512;
// Trois points ASCII : l'ellipse typographique n'a pas de glyphe dans la police.
constexpr char kEllipsis[] = "...";

}  // namespace

void PaintHudCaption(ImDrawList& into, ImVec2 top_left, ImVec2 bottom_right, const char* text,
                     ImU32 color, float pixels) {
  ImFont* font = ImGui::GetFont();
  if (font == nullptr || text == nullptr || text[0] == '\0') {
    return;
  }
  ImVec2 size = font->CalcTextSizeA(pixels, FLT_MAX, 0.0F, text);
  std::array<char, kClippedRoom> clipped{};
  const float room = (bottom_right.x - top_left.x) - kHudCaptionMargin;
  if (room > 0.0F && size.x > room) {
    const float ellipsis_width = font->CalcTextSizeA(pixels, FLT_MAX, 0.0F, kEllipsis).x;
    const char* remaining = nullptr;
    // ImGui coupe sur une frontière de caractère : pas de demi-glyphe UTF-8.
    font->CalcTextSizeA(pixels, std::max(0.0F, room - ellipsis_width), 0.0F, text, nullptr,
                        &remaining);
    std::size_t kept = remaining != nullptr ? static_cast<std::size_t>(remaining - text) : 0;
    kept = std::min(kept, clipped.size() - sizeof(kEllipsis));
    std::memcpy(clipped.data(), text, kept);
    std::memcpy(clipped.data() + kept, kEllipsis, sizeof(kEllipsis));
    text = clipped.data();
    size = font->CalcTextSizeA(pixels, FLT_MAX, 0.0F, text);
  }
  const ImVec2 at((top_left.x + bottom_right.x - size.x) * kHalf,
                  (top_left.y + bottom_right.y - size.y) * kHalf);
  const float shadow = std::max(kLeastShadow, std::floor(pixels * kShadowShare));
  into.AddText(font, pixels, ImVec2(at.x + shadow, at.y + shadow), kCaptionShadow, text);
  into.AddText(font, pixels, at, color, text);
}

}  // namespace ro::ui

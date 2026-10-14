#include "ro/ui/map_sign_banner.h"

#include <cfloat>
#include <cmath>

namespace ro::ui {
namespace {

constexpr std::uint8_t kOpaque = 255;

// Le milieu d'une longueur.
constexpr float kHalf = 0.5F;

// Un texte centré sur l'écran, blanc, à l'opacité du bandeau, posé au pixel.
void DrawCentered(ImDrawList* into, ImFont* font, std::string_view text, float size, float top,
                  float screen_x, std::uint8_t alpha) {
  if (text.empty()) {
    return;
  }
  const char* begin = text.data();
  const char* end = begin + text.size();
  const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0F, begin, end);
  into->AddText(font, size, ImVec2(std::trunc((screen_x - extent.x) * kHalf), top),
                IM_COL32(255, 255, 255, alpha), begin, end);
}

}  // namespace

MapSignLayout MapSignLayoutOf(ImVec2 screen, ImVec2 image) {
  MapSignLayout layout;
  const float band_top = std::trunc(screen.y * kMapSignTopShare);
  const float band_height = std::trunc(screen.y * kMapSignHeightShare);
  layout.scale = image.y > 0.0F ? band_height / image.y : 1.0F;
  layout.image_size = ImVec2(std::trunc(image.x * layout.scale), band_height);
  layout.image_top_left = ImVec2(std::trunc((screen.x - layout.image_size.x) * kHalf), band_top);
  layout.subtitle_size = std::trunc(kMapSignSubtitleSize * layout.scale) + kMapSignCellGrowthPx;
  layout.subtitle_top = band_top + std::trunc(kMapSignSubtitleTop * layout.scale);
  layout.title_size = std::trunc(kMapSignTitleSize * layout.scale) + kMapSignCellGrowthPx;
  layout.title_top = band_top + std::trunc(kMapSignTitleTop * layout.scale);
  return layout;
}

std::optional<std::uint8_t> MapSignAlpha(std::uint32_t elapsed_ms) {
  if (elapsed_ms <= kMapSignShownMs) {
    return kOpaque;
  }
  const std::uint32_t fading = elapsed_ms - kMapSignShownMs;
  if (fading >= kMapSignFadeMs) {
    return std::nullopt;
  }
  return static_cast<std::uint8_t>(kMapSignFadeMs - fading);
}

void DrawMapSign(const MapSignView& sign, ImFont* font) {
  const std::optional<std::uint8_t> alpha = MapSignAlpha(sign.elapsed_ms);
  if (!alpha.has_value() || !sign.background.usable()) {
    return;
  }
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  const MapSignLayout layout =
      MapSignLayoutOf(screen, ImVec2(sign.background.width, sign.background.height));
  // ⚠ AU PREMIER PLAN : le client le pose après toutes ses fenêtres.
  ImDrawList* into = ImGui::GetForegroundDrawList();
  const ImVec2 bottom_right(layout.image_top_left.x + layout.image_size.x,
                            layout.image_top_left.y + layout.image_size.y);
  into->AddImage(sign.background.id, layout.image_top_left, bottom_right, ImVec2(0.0F, 0.0F),
                 ImVec2(1.0F, 1.0F), IM_COL32(255, 255, 255, *alpha));
  ImFont* drawing = font != nullptr ? font : ImGui::GetFont();
  DrawCentered(into, drawing, sign.subtitle, layout.subtitle_size, layout.subtitle_top, screen.x,
               *alpha);
  DrawCentered(into, drawing, sign.title, layout.title_size, layout.title_top, screen.x, *alpha);
}

}  // namespace ro::ui

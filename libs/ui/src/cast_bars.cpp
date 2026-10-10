#include "ro/ui/cast_bars.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/entity_bars.h"
#include "ro/ui/gauge_fill.h"

namespace ro::ui {
namespace {

// En deçà d'un pixel, la part pleine ne se peint pas : un trait d'un demi-pixel
// arrondi fait une tache, pas une barre.
constexpr float kLeastFillPixels = 1.0F;
// L'épaisseur du cadre, en pixels. La part pleine se tient à l'intérieur.
constexpr float kBorderPixels = 1.0F;

// L'étiquette : blanche, en relief noir d'un pixel en bas à droite.
constexpr ImU32 kCaptionColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kCaptionRelief = IM_COL32(0, 0, 0, 210);
constexpr ImVec2 kCaptionReliefOffset(1.0F, 1.0F);
// Le cadre, noir à deux tiers.
constexpr ImU32 kBorderColor = IM_COL32(0, 0, 0, 170);
// Ce qui sépare l'étiquette de la barre : un pixel au-dessus, trois à droite.
constexpr float kCaptionLiftPixels = 1.0F;
constexpr float kCaptionAsidePixels = 3.0F;
// Les bornes de la taille du texte, en part de celle de l'interface.
constexpr float kSmallestCaptionShare = 0.5F;
constexpr float kLargestCaptionShare = 2.0F;
// Un dixième de seconde, et l'arrondi au plus proche.
constexpr std::uint32_t kTenthMs = 100;
constexpr std::uint32_t kHalfTenthMs = 50;
constexpr std::uint32_t kTenthsPerSecond = 10;
constexpr int kPercentWhole = 100;
// Le cadre du client : un pixel tout autour du creux.
constexpr float kClientFramePixels = 1.0F;

[[nodiscard]] ImU32 Faded(ImU32 color, float share) {
  const auto alpha = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFFU);
  const auto kept = static_cast<ImU32>(std::lround(alpha * std::clamp(share, 0.0F, 1.0F)));
  return (color & ~IM_COL32_A_MASK) | (kept << IM_COL32_A_SHIFT);
}

[[nodiscard]] std::uint32_t RemainingMs(const CastBarView& what) {
  return what.total_ms > what.elapsed_ms ? what.total_ms - what.elapsed_ms : 0;
}

// LA BARRE DU CLIENT : ce qu'il peint, sans un pixel de plus.
void PaintClientBar(ImDrawList& into, const CastBarView& what) {
  const ClientCastBarRects rects = ClientCastBarRectsOf(what);
  into.AddRectFilled(rects.frame_min, rects.frame_max, kClientBarBorder);
  into.AddRectFilled(rects.trough_min, rects.trough_max, kClientBarBackground);
  if (rects.fill_max.x > rects.fill_min.x) {
    into.AddRectFilled(rects.fill_min, rects.fill_max, kClientBarFill);
  }
}

// LA BARRE QUI LA REMPLACE, puis son étiquette.
void PaintReplacedBar(ImDrawList& into, const CastBarView& what, const CastBarLook& look) {
  const float opacity = std::clamp(look.opacity, kSmallestCastBarOpacity, 1.0F);
  // ⚠ SUR LE PIXEL ENTIER : une barre posée entre deux pixels se peint en fondu.
  const float width =
      std::round(std::clamp(look.width_pixels, kSmallestCastBarWidth, kLargestCastBarWidth));
  const float height =
      std::round(std::clamp(look.height_pixels, kSmallestCastBarHeight, kLargestCastBarHeight));
  const float rounding = std::clamp(look.rounding, 0.0F, kLargestCastBarRounding);
  const ImVec2 top_left(std::round(what.head.x - width * 0.5F),
                        std::round(what.head.y + look.offset_pixels));
  const ImVec2 bottom_right(top_left.x + width, top_left.y + height);

  into.AddRectFilled(top_left, bottom_right, Faded(look.background, opacity), rounding);
  // ⚠ LA PART PLEINE DANS L'INTÉRIEUR DU CADRE, découpée dans sa forme arrondie :
  // voir `gauge_fill.h`. Le front qui avance reste une arête franche, sans quoi
  // il « fond » à mesure qu'il progresse, et ne suit la courbe qu'au bout.
  const RoundedBox interior =
      GaugeInterior(top_left, bottom_right, rounding, look.border ? kBorderPixels : 0.0F);
  const float share = CastBarProgress(what);
  if ((interior.max.x - interior.min.x) * share >= kLeastFillPixels) {
    PaintGaugeFill(into, interior, share, GaugeGrowth::kRightward, Faded(look.fill, opacity));
  }
  if (look.border) {
    into.AddRect(top_left, bottom_right, Faded(kBorderColor, opacity), rounding, kBorderPixels);
  }

  const std::string caption = CastBarCaption(look, what);
  ImFont* font = ImGui::GetFont();
  if (caption.empty() || font == nullptr) {
    return;
  }
  // ⚠ UNE TAILLE ENTIÈRE : ImGui cuit ses glyphes à la taille arrondie puis les
  // étire, et un texte étiré de quelques pour cent se brouille.
  const float pixels =
      std::max(kSmallestBakedFontPixels,
               std::round(ImGui::GetFontSize() * std::clamp(look.font_scale, kSmallestCaptionShare,
                                                            kLargestCaptionShare)));
  const ImVec2 size = font->CalcTextSizeA(pixels, FLT_MAX, 0.0F, caption.c_str());
  // 🔴 SOUS UNE BULLE, L'ÉTIQUETTE PASSE À DROITE DE LA BARRE. La bulle occupe
  // la place juste au-dessus, au pixel près — même ancrage, elle vers le haut,
  // la barre vers le bas. Écrire là donnait deux textes superposés ; à droite,
  // la place est toujours libre et se lit comme un chronomètre.
  const ImVec2 at =
      what.balloon
          ? ImVec2(bottom_right.x + kCaptionAsidePixels, top_left.y + (height - size.y) * 0.5F)
          : ImVec2(what.head.x - size.x * 0.5F, top_left.y - size.y - kCaptionLiftPixels);
  const ImVec2 relief(at.x + kCaptionReliefOffset.x, at.y + kCaptionReliefOffset.y);
  into.AddText(font, pixels, relief, Faded(kCaptionRelief, opacity), caption.c_str());
  into.AddText(font, pixels, at, Faded(kCaptionColor, opacity), caption.c_str());
}

}  // namespace

CastBarName CastBarNameOf(int stored) {
  if (stored < 0 || stored >= static_cast<int>(CastBarName::kCount)) {
    return CastBarName::kIfFree;
  }
  return static_cast<CastBarName>(stored);
}

bool ShowsCastBar(const CastBarsLook& look, const CastBarView& what) {
  const auto rank = static_cast<std::size_t>(what.kind);
  return what.total_ms != 0 && rank < look.kinds.size() && look.kinds[rank].shown;
}

float CastBarProgress(const CastBarView& what) {
  if (what.total_ms == 0) {
    return 0.0F;
  }
  const std::uint32_t elapsed = std::min(what.elapsed_ms, what.total_ms);
  return static_cast<float>(elapsed) / static_cast<float>(what.total_ms);
}

ClientCastBarRects ClientCastBarRectsOf(const CastBarView& what) {
  ClientCastBarRects rects;
  // Le coin haut-gauche tombe sur le pixel, comme la fenêtre du client.
  const float left = std::floor(what.head.x) - kClientCastBarWidthPixels * 0.5F;
  const float top = std::floor(what.head.y);
  rects.frame_min = ImVec2(left, top);
  rects.frame_max = ImVec2(left + kClientCastBarWidthPixels, top + kClientCastBarHeightPixels);
  rects.trough_min = ImVec2(left + kClientFramePixels, top + kClientFramePixels);
  rects.trough_max =
      ImVec2(rects.frame_max.x - kClientFramePixels, rects.frame_max.y - kClientFramePixels);
  rects.fill_min = rects.trough_min;
  rects.fill_max = rects.trough_min;
  if (what.total_ms == 0) {
    return rects;
  }
  const std::uint32_t elapsed = std::min(what.elapsed_ms, what.total_ms);
  const auto percent =
      static_cast<int>(static_cast<std::uint64_t>(kPercentWhole) * elapsed / what.total_ms);
  const float span =
      kClientCastBarWidthPixels * static_cast<float>(percent) / static_cast<float>(kPercentWhole) -
      2.0F * kClientFramePixels;
  const float filled = std::trunc(span);
  if (filled > 0.0F) {
    rects.fill_max = ImVec2(rects.fill_min.x + filled, rects.trough_max.y);
  }
  return rects;
}

std::string CastSecondsText(std::uint32_t remaining_ms) {
  const std::uint32_t tenths = (remaining_ms + kHalfTenthMs) / kTenthMs;
  std::array<char, 32> text{};
  std::snprintf(text.data(), text.size(), core::Tr("%u,%u s"), tenths / kTenthsPerSecond,
                tenths % kTenthsPerSecond);
  return std::string(text.data());
}

std::string CastBarCaption(const CastBarLook& look, const CastBarView& what) {
  const bool named =
      look.name == CastBarName::kAlways || (look.name == CastBarName::kIfFree && !what.balloon);
  std::string caption;
  if (named && !what.skill_name.empty()) {
    caption = what.skill_name;
  }
  if (look.time_left) {
    if (!caption.empty()) {
      caption += "  ";
    }
    caption += CastSecondsText(RemainingMs(what));
  }
  return caption;
}

void PaintCastBar(ImDrawList& into, const CastBarView& what, const CastBarsLook& look) {
  const auto rank = static_cast<std::size_t>(what.kind);
  if (look.replaced && rank < look.kinds.size()) {
    PaintReplacedBar(into, what, look.kinds[rank]);
  } else {
    PaintClientBar(into, what);
  }
}

void DrawCastBars(std::span<const CastBarView> bars, const CastBarsLook& look) {
  ImDrawList* into = ImGui::GetBackgroundDrawList();
  if (into == nullptr) {
    return;
  }
  for (const CastBarView& what : bars) {
    if (ShowsCastBar(look, what)) {
      PaintCastBar(*into, what, look);
    }
  }
}

}  // namespace ro::ui

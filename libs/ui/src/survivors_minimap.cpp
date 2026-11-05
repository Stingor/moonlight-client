#include "ro/ui/survivors_minimap.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ro::ui {
namespace {

// Le fond et le bord du carré.
constexpr ImU32 kBack = IM_COL32(10, 14, 20, 170);
constexpr ImU32 kEdge = IM_COL32(255, 255, 255, 110);
constexpr ImU32 kScreenFrame = IM_COL32(220, 220, 230, 140);
// Les symboles : un point par cible, cerclé de noir pour se lire sur l'herbe.
constexpr float kDotRadius = 3.0F;
constexpr float kDotOutline = 1.0F;
constexpr ImU32 kOutline = IM_COL32(0, 0, 0, 200);
constexpr ImU32 kPlantColor = IM_COL32(80, 220, 90, 255);
constexpr ImU32 kChestColor = IM_COL32(190, 90, 255, 255);
constexpr ImU32 kCardColor = IM_COL32(90, 160, 255, 255);
constexpr ImU32 kItemColor = IM_COL32(250, 220, 70, 255);
constexpr ImU32 kEmperiumColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kMiniBossColor = IM_COL32(255, 150, 40, 255);
constexpr ImU32 kAllyColor = IM_COL32(90, 200, 255, 255);
constexpr ImU32 kBossColor = IM_COL32(235, 45, 45, 255);
// LE CRÂNE D'UN BOSS : une tête, deux orbites, une mâchoire.
constexpr float kSkullRadius = 5.0F;
constexpr float kSkullEyeOffsetX = 2.0F;
constexpr float kSkullEyeOffsetY = 0.5F;
constexpr float kSkullEyeRadius = 1.2F;
constexpr float kSkullJawHalfWidth = 2.5F;
constexpr float kSkullJawTop = 3.0F;
constexpr float kSkullJawBottom = 6.5F;
constexpr ImU32 kSkullHole = IM_COL32(0, 0, 0, 230);
// UN SYMBOLE SUR LE BORD reste entier : l'échelle se compte sur le carré
// rentré d'autant.
constexpr float kEdgeInset = kSkullRadius + kDotOutline;
// Le boss bat : son opacité va et vient sur cette période.
constexpr float kBossPulseMs = 800.0F;
constexpr float kBossPulseLowest = 0.45F;
// LA FLÈCHE DU HÉROS : un triangle blanc tourné vers sa marche.
constexpr float kHeroArrowLength = 7.0F;
constexpr float kHeroArrowHalfWidth = 4.5F;
constexpr float kHeroArrowBack = 3.0F;
constexpr ImU32 kHeroColor = IM_COL32(255, 255, 255, 255);
constexpr float kHeadingSteps = 64.0F;
constexpr float kOpaque = 255.0F;

[[nodiscard]] ImU32 ColorOf(SurvivorsMinimapMark mark) noexcept {
  switch (mark) {
    case SurvivorsMinimapMark::kPlant:
      return kPlantColor;
    case SurvivorsMinimapMark::kChest:
      return kChestColor;
    case SurvivorsMinimapMark::kCard:
      return kCardColor;
    case SurvivorsMinimapMark::kItem:
      return kItemColor;
    case SurvivorsMinimapMark::kEmperium:
      return kEmperiumColor;
    case SurvivorsMinimapMark::kMiniBoss:
      return kMiniBossColor;
    case SurvivorsMinimapMark::kBoss:
      return kBossColor;
    case SurvivorsMinimapMark::kAlly:
      return kAllyColor;
  }
  return kItemColor;
}

[[nodiscard]] ImU32 WithAlpha(ImU32 color, float share) noexcept {
  const auto alpha = static_cast<ImU32>(std::lround(std::clamp(share, 0.0F, 1.0F) * kOpaque));
  return (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
}

void DrawSkull(ImDrawList& draw, ImVec2 at, ImU32 color) {
  draw.AddCircleFilled(at, kSkullRadius + kDotOutline, kOutline);
  draw.AddRectFilled(ImVec2(at.x - kSkullJawHalfWidth - kDotOutline, at.y + kSkullJawTop),
                     ImVec2(at.x + kSkullJawHalfWidth + kDotOutline, at.y + kSkullJawBottom),
                     kOutline);
  draw.AddCircleFilled(at, kSkullRadius, color);
  draw.AddRectFilled(ImVec2(at.x - kSkullJawHalfWidth, at.y + kSkullJawTop),
                     ImVec2(at.x + kSkullJawHalfWidth, at.y + kSkullJawBottom - kDotOutline),
                     color);
  draw.AddCircleFilled(ImVec2(at.x - kSkullEyeOffsetX, at.y - kSkullEyeOffsetY), kSkullEyeRadius,
                       kSkullHole);
  draw.AddCircleFilled(ImVec2(at.x + kSkullEyeOffsetX, at.y - kSkullEyeOffsetY), kSkullEyeRadius,
                       kSkullHole);
}

void DrawHero(ImDrawList& draw, ImVec2 center, std::uint8_t facing) {
  // 0 au nord, sens trigonométrique : vers l'ouest au quart de tour. L'écran
  // descend vers le sud.
  const float angle = static_cast<float>(facing) * 2.0F * std::numbers::pi_v<float> / kHeadingSteps;
  const ImVec2 ahead(-std::sin(angle), -std::cos(angle));
  const ImVec2 side(-ahead.y, ahead.x);
  const ImVec2 tip(center.x + ahead.x * kHeroArrowLength, center.y + ahead.y * kHeroArrowLength);
  const ImVec2 back(center.x - ahead.x * kHeroArrowBack, center.y - ahead.y * kHeroArrowBack);
  const ImVec2 left(back.x + side.x * kHeroArrowHalfWidth, back.y + side.y * kHeroArrowHalfWidth);
  const ImVec2 right(back.x - side.x * kHeroArrowHalfWidth, back.y - side.y * kHeroArrowHalfWidth);
  draw.AddTriangleFilled(tip, left, right, kHeroColor);
  draw.AddTriangle(tip, left, right, kOutline, kDotOutline);
}

}  // namespace

SurvivorsMinimapPlace PlaceOnSurvivorsMinimap(ImVec2 center, float half_px,
                                              ImVec2 offset_cells) noexcept {
  const float scale = half_px / kSurvivorsMinimapRadiusCells;
  float east_px = offset_cells.x * scale;
  float south_px = -offset_cells.y * scale;
  const float reach = std::max(std::abs(east_px), std::abs(south_px));
  const bool on_edge = reach > half_px;
  if (on_edge) {
    east_px *= half_px / reach;
    south_px *= half_px / reach;
  }
  return SurvivorsMinimapPlace{.at = ImVec2(center.x + east_px, center.y + south_px),
                               .on_edge = on_edge};
}

float DrawSurvivorsMinimap(ImDrawList& draw, ImVec2 top_right, const SurvivorsMinimapView& view,
                           std::uint64_t clock_ms) {
  const ImVec2 corner_min(top_right.x - kSurvivorsMinimapSidePx, top_right.y);
  const ImVec2 corner_max(top_right.x, top_right.y + kSurvivorsMinimapSidePx);
  const float half = kSurvivorsMinimapSidePx * 0.5F;
  const ImVec2 center(corner_min.x + half, corner_min.y + half);
  const float inner_half = half - kEdgeInset;
  draw.AddRectFilled(corner_min, corner_max, kBack);
  draw.PushClipRect(corner_min, corner_max, true);
  if (view.screen_known) {
    std::array<ImVec2, 4> frame{};
    for (std::size_t corner = 0; corner < frame.size(); ++corner) {
      frame[corner] =
          PlaceOnSurvivorsMinimap(center, inner_half, view.screen_corners_cells[corner]).at;
    }
    draw.AddPolyline(frame.data(), static_cast<int>(frame.size()), kScreenFrame, ImDrawFlags_Closed,
                     kDotOutline);
  }
  const float pulse_phase = std::fmod(static_cast<float>(clock_ms), kBossPulseMs) / kBossPulseMs *
                            2.0F * std::numbers::pi_v<float>;
  const float boss_share =
      kBossPulseLowest + (1.0F - kBossPulseLowest) * 0.5F * (1.0F + std::cos(pulse_phase));
  for (const SurvivorsMinimapDot& dot : view.dots) {
    const ImVec2 at = PlaceOnSurvivorsMinimap(center, inner_half, dot.offset_cells).at;
    const ImU32 color = ColorOf(dot.mark);
    switch (dot.mark) {
      case SurvivorsMinimapMark::kBoss:
        DrawSkull(draw, at, WithAlpha(color, boss_share));
        break;
      case SurvivorsMinimapMark::kMiniBoss:
        DrawSkull(draw, at, color);
        break;
      case SurvivorsMinimapMark::kAlly:
      case SurvivorsMinimapMark::kPlant:
      case SurvivorsMinimapMark::kChest:
      case SurvivorsMinimapMark::kCard:
      case SurvivorsMinimapMark::kItem:
      case SurvivorsMinimapMark::kEmperium:
        draw.AddCircleFilled(at, kDotRadius + kDotOutline, kOutline);
        draw.AddCircleFilled(at, kDotRadius, color);
        break;
    }
  }
  DrawHero(draw, center, view.hero_facing);
  draw.PopClipRect();
  draw.AddRect(corner_min, corner_max, kEdge);
  return corner_max.y;
}

}  // namespace ro::ui

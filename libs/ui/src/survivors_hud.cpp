#include "ro/ui/survivors_hud.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/ui/entity_bars.h"

namespace ro::ui {
namespace {

constexpr std::uint64_t kMsPerSecond = 1000;
constexpr std::uint64_t kSecondsPerMinute = 60;

// La barre d'expérience, collée en haut de l'écran.
constexpr float kExpBarHeight = 10.0F;
constexpr ImU32 kExpBarBack = IM_COL32(20, 24, 40, 200);
constexpr ImU32 kExpBarFill = IM_COL32(90, 150, 255, 255);
// La marge de tout ce qui se pose sous la barre.
constexpr float kMargin = 8.0F;
// Les cases de skills et de cartes.
constexpr float kSlotSize = 32.0F;
constexpr float kSlotGap = 4.0F;
constexpr ImU32 kSlotBack = IM_COL32(0, 0, 0, 140);
constexpr ImU32 kSlotEdge = IM_COL32(255, 255, 255, 90);
// Les icônes des charges et des vies, sous les compteurs.
constexpr float kChargeIconSide = 20.0F;
// Le texte sur la scène : blanc à l'ombre noire décalée d'un pixel.
constexpr ImU32 kTextColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kShadowColor = IM_COL32(0, 0, 0, 220);
constexpr float kShadowOffset = 1.0F;
constexpr ImU32 kFaintColor = IM_COL32(200, 200, 210, 220);
// La barre du boss : une part de la largeur de l'écran, centrée.
constexpr float kBossBarWidthShare = 0.4F;
constexpr float kBossBarHeight = 10.0F;
constexpr ImU32 kBossBarBack = IM_COL32(40, 10, 10, 210);
constexpr ImU32 kBossBarFill = IM_COL32(220, 40, 40, 255);
// Les flèches de bord : un triangle, rentré du bord pour rester lisible.
constexpr float kArrowInset = 28.0F;
constexpr float kArrowLength = 18.0F;
constexpr float kArrowHalfWidth = 9.0F;
constexpr ImU32 kBossArrow = IM_COL32(230, 50, 50, 255);
constexpr ImU32 kChestArrow = IM_COL32(90, 170, 255, 255);
constexpr std::size_t kNumberTextSize = 48;
// La vignette de la carte ramassée : sa hauteur à l'écran, la largeur suivant
// l'illustration, et la part de sa vie qu'elle met à entrer, puis à sortir.
constexpr float kCardVignetteHeight = 150.0F;
constexpr float kCardSlideShare = 0.1F;

void ShadowedText(ImDrawList& draw, ImVec2 at, ImU32 color, const char* text) {
  draw.AddText(ImVec2(at.x + kShadowOffset, at.y + kShadowOffset), kShadowColor, text);
  draw.AddText(at, color, text);
}

void DrawBar(ImDrawList& draw, ImVec2 top_left, ImVec2 bottom_right, float fill, ImU32 back,
             ImU32 ink) {
  draw.AddRectFilled(top_left, bottom_right, back);
  const float shown = std::clamp(fill, 0.0F, 1.0F);
  if (shown > 0.0F) {
    const float right = top_left.x + (bottom_right.x - top_left.x) * shown;
    draw.AddRectFilled(top_left, ImVec2(right, bottom_right.y), ink);
  }
}

// Une rangée de cases depuis `corner` ; rend l'abscisse qui suit la dernière.
float DrawSlots(ImDrawList& draw, ImVec2 corner, std::span<const SurvivorsHudSlot> slots) {
  float left = corner.x;
  std::array<char, kNumberTextSize> level = {};
  for (const SurvivorsHudSlot& slot : slots) {
    const ImVec2 top_left(left, corner.y);
    const ImVec2 bottom_right(left + kSlotSize, corner.y + kSlotSize);
    draw.AddRectFilled(top_left, bottom_right, kSlotBack);
    if (slot.icon != 0) {
      draw.AddImage(slot.icon, top_left, bottom_right);
    }
    draw.AddRect(top_left, bottom_right, kSlotEdge);
    std::snprintf(level.data(), level.size(), "%d", slot.level);
    const ImVec2 size = ImGui::CalcTextSize(level.data());
    ShadowedText(draw, ImVec2(bottom_right.x - size.x - 1.0F, bottom_right.y - size.y), kTextColor,
                 level.data());
    left += kSlotSize + kSlotGap;
  }
  return left;
}

// Une rangée d'icônes comptées, alignée à droite sur `right`.
void DrawCountedIcon(ImDrawList& draw, float right, float top, ImTextureID icon, int count) {
  std::array<char, kNumberTextSize> text = {};
  std::snprintf(text.data(), text.size(), "%d", count);
  const ImVec2 size = ImGui::CalcTextSize(text.data());
  const float icon_left = right - size.x - kSlotGap - kChargeIconSide;
  const ImVec2 top_left(icon_left, top);
  const ImVec2 bottom_right(icon_left + kChargeIconSide, top + kChargeIconSide);
  draw.AddRectFilled(top_left, bottom_right, kSlotBack);
  if (icon != 0) draw.AddImage(icon, top_left, bottom_right);
  ShadowedText(draw, ImVec2(right - size.x, top + (kChargeIconSide - size.y) * 0.5F), kTextColor,
               text.data());
}

// Sous les compteurs : une ligne par charge, puis les vies ; rend l'ordonnée
// sous la dernière ligne.
float DrawChargesAndLives(ImDrawList& draw, const SurvivorsHudView& view, float right, float top) {
  for (const SurvivorsHudCharge& charge : view.charges) {
    DrawCountedIcon(draw, right, top, charge.icon, charge.count);
    top += kChargeIconSide + kSlotGap;
  }
  if (view.lives > 0) {
    DrawCountedIcon(draw, right, top, view.life_icon, view.lives);
    top += kChargeIconSide + kSlotGap;
  }
  return top;
}

// La marque discrète de la partie sous le minuteur ; rend la hauteur prise.
float DrawRunMark(ImDrawList& draw, const SurvivorsHudView& view, float middle_x, float top) {
  std::array<char, kNumberTextSize> mark = {};
  if (view.ranked) {
    std::snprintf(mark.data(), mark.size(), "%s", core::Tr("Classée"));
  } else if (view.challenge) {
    std::snprintf(mark.data(), mark.size(), "%s", core::Tr("Défi"));
  } else if (view.cursed_water > 0) {
    std::snprintf(mark.data(), mark.size(), core::Tr("Cursed Water %d"), view.cursed_water);
  } else {
    return 0.0F;
  }
  const ImVec2 size = ImGui::CalcTextSize(mark.data());
  ShadowedText(draw, ImVec2(middle_x - size.x * 0.5F, top), kFaintColor, mark.data());
  return size.y;
}

// Le coin droit : tués, Zeny, messages privés, une ligne chacun, alignés à
// droite ; rend l'ordonnée sous la dernière ligne.
float DrawCounters(ImDrawList& draw, const SurvivorsHudView& view, float right, float top) {
  std::array<char, kNumberTextSize> line = {};
  const float line_height = ImGui::GetTextLineHeight();
  const auto put = [&](ImU32 color) {
    const ImVec2 size = ImGui::CalcTextSize(line.data());
    ShadowedText(draw, ImVec2(right - size.x, top), color, line.data());
    top += line_height;
  };
  std::snprintf(line.data(), line.size(), core::Tr("Tués : %u"), view.kills);
  put(kTextColor);
  std::snprintf(line.data(), line.size(), core::Tr("Zeny : %u"), view.zeny);
  put(kTextColor);
  if (view.unread_whispers > 0) {
    std::snprintf(line.data(), line.size(), core::Tr("Messages privés : %d"), view.unread_whispers);
    put(kFaintColor);
  }
  return top;
}

void DrawArrow(ImDrawList& draw, const EdgeArrowPlace& place, ImU32 color) {
  const float along_x = std::cos(place.angle_radians);
  const float along_y = std::sin(place.angle_radians);
  const ImVec2 back(place.tip.x - along_x * kArrowLength, place.tip.y - along_y * kArrowLength);
  const ImVec2 left(back.x - along_y * kArrowHalfWidth, back.y + along_x * kArrowHalfWidth);
  const ImVec2 right(back.x + along_y * kArrowHalfWidth, back.y - along_x * kArrowHalfWidth);
  draw.AddTriangleFilled(place.tip, left, right, color);
  draw.AddTriangle(place.tip, left, right, kShadowColor);
}

// L'illustration de la carte ramassée, sortie du bord droit selon sa vie,
// posée au-dessus de la marge du bas.
void DrawFoundCard(ImDrawList& draw, const SurvivorsHudCard& card, ImVec2 far) {
  if (card.illustration == 0 || card.size.x <= 0.0F || card.size.y <= 0.0F) return;
  const float slide = SurvivorsCardSlide(card.fraction);
  if (slide <= 0.0F) return;
  const float width = kCardVignetteHeight * card.size.x / card.size.y;
  const ImVec2 top_left(far.x - (width + kMargin) * slide, far.y - kMargin - kCardVignetteHeight);
  const ImVec2 bottom_right(top_left.x + width, top_left.y + kCardVignetteHeight);
  draw.AddRectFilled(top_left, bottom_right, kSlotBack);
  draw.AddImage(card.illustration, top_left, bottom_right);
  draw.AddRect(top_left, bottom_right, kSlotEdge);
}

void DrawHeroGauge(ImDrawList& draw, ImVec2 feet, float fill) {
  const EntityBarLook look;
  const ImVec2 top_left(feet.x - look.width_pixels * 0.5F, feet.y + look.offset_pixels);
  const ImVec2 bottom_right(top_left.x + look.width_pixels, top_left.y + look.height_pixels);
  const float shown = std::clamp(fill, 0.0F, 1.0F);
  PaintEntityBar(draw, top_left, bottom_right, shown, EntityBarFillColor(look, shown), look);
}

}  // namespace

float SurvivorsCardSlide(float fraction) noexcept {
  if (fraction <= 0.0F || fraction >= 1.0F) return 0.0F;
  return std::min({fraction / kCardSlideShare, (1.0F - fraction) / kCardSlideShare, 1.0F});
}

std::array<char, kSurvivorsClockTextSize> SurvivorsClockText(std::uint64_t clock_ms) {
  const std::uint64_t seconds = clock_ms / kMsPerSecond;
  std::array<char, kSurvivorsClockTextSize> text = {};
  std::snprintf(text.data(), text.size(), "%02llu:%02llu",
                static_cast<unsigned long long>(seconds / kSecondsPerMinute),
                static_cast<unsigned long long>(seconds % kSecondsPerMinute));
  return text;
}

std::optional<EdgeArrowPlace> PlaceEdgeArrow(ImVec2 frame_min, ImVec2 frame_max, float inset_px,
                                             ImVec2 target) {
  const ImVec2 low(frame_min.x + inset_px, frame_min.y + inset_px);
  const ImVec2 high(frame_max.x - inset_px, frame_max.y - inset_px);
  if (low.x >= high.x || low.y >= high.y) {
    return std::nullopt;
  }
  if (target.x >= low.x && target.x <= high.x && target.y >= low.y && target.y <= high.y) {
    return std::nullopt;
  }
  const ImVec2 center((low.x + high.x) * 0.5F, (low.y + high.y) * 0.5F);
  const float toward_x = target.x - center.x;
  const float toward_y = target.y - center.y;
  // La plus petite fraction du chemin qui atteint un bord : la droite sort du
  // cadre par ce bord-là.
  float reach = 1.0F;
  if (toward_x != 0.0F) {
    reach = std::min(reach, ((toward_x > 0.0F ? high.x : low.x) - center.x) / toward_x);
  }
  if (toward_y != 0.0F) {
    reach = std::min(reach, ((toward_y > 0.0F ? high.y : low.y) - center.y) / toward_y);
  }
  return EdgeArrowPlace{
      .tip = ImVec2(center.x + toward_x * reach, center.y + toward_y * reach),
      .angle_radians = std::atan2(toward_y, toward_x),
  };
}

void DrawSurvivorsHud(const SurvivorsHudView& view) {
  ImDrawList& draw = *ImGui::GetBackgroundDrawList();
  const ImGuiViewport* screen = ImGui::GetMainViewport();
  const ImVec2 origin = screen->Pos;
  const ImVec2 far(origin.x + screen->Size.x, origin.y + screen->Size.y);

  DrawBar(draw, origin, ImVec2(far.x, origin.y + kExpBarHeight), view.exp_fill, kExpBarBack,
          kExpBarFill);
  std::array<char, kNumberTextSize> level = {};
  std::snprintf(level.data(), level.size(), core::Tr("Niv. %u"), static_cast<unsigned>(view.level));
  const float below_bar = origin.y + kExpBarHeight + kMargin;
  ShadowedText(draw, ImVec2(origin.x + kMargin, below_bar), kTextColor, level.data());

  const std::array<char, kSurvivorsClockTextSize> clock = SurvivorsClockText(view.clock_ms);
  const ImVec2 clock_size = ImGui::CalcTextSize(clock.data());
  const float middle_x = (origin.x + far.x) * 0.5F;
  ShadowedText(draw, ImVec2(middle_x - clock_size.x * 0.5F, below_bar), kTextColor, clock.data());

  const float mark_height = DrawRunMark(draw, view, middle_x, below_bar + clock_size.y);
  if (view.boss_name != nullptr) {
    const float half = screen->Size.x * kBossBarWidthShare * 0.5F;
    const float name_top = below_bar + clock_size.y + mark_height + kMargin;
    const ImVec2 name_size = ImGui::CalcTextSize(view.boss_name);
    ShadowedText(draw, ImVec2(middle_x - name_size.x * 0.5F, name_top), kTextColor, view.boss_name);
    const float bar_top = name_top + name_size.y;
    DrawBar(draw, ImVec2(middle_x - half, bar_top),
            ImVec2(middle_x + half, bar_top + kBossBarHeight), view.boss_fill, kBossBarBack,
            kBossBarFill);
  }

  const float slots_top = below_bar + ImGui::GetTextLineHeight() + kMargin;
  const float after_skills = DrawSlots(draw, ImVec2(origin.x + kMargin, slots_top), view.skills);
  DrawSlots(draw, ImVec2(after_skills + kMargin, slots_top), view.cards);
  const float below_counters = DrawCounters(draw, view, far.x - kMargin, below_bar);
  const float below_charges =
      DrawChargesAndLives(draw, view, far.x - kMargin, below_counters + kSlotGap);
  DrawSurvivorsHudEffects(draw, view.effects, far.x - kMargin, below_charges + kSlotGap);

  for (const SurvivorsEdgeTarget& target : view.edge_targets) {
    if (const std::optional<EdgeArrowPlace> place =
            PlaceEdgeArrow(origin, far, kArrowInset, target.screen_px)) {
      DrawArrow(draw, *place, target.kind == SurvivorsEdgeKind::kBoss ? kBossArrow : kChestArrow);
    }
  }
  if (view.hero_feet_px.has_value()) {
    DrawHeroGauge(draw, *view.hero_feet_px, view.hero_fill);
  }
  // Sans pieds connus, les avis se posent au centre de l'écran, où se tient
  // le héros que la caméra suit.
  DrawSurvivorsHudNotices(draw, view.notices,
                          view.hero_feet_px.value_or(ImVec2(screen->GetCenter())));
  DrawFoundCard(draw, view.found_card, far);
}

}  // namespace ro::ui

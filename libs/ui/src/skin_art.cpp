#include "ro/ui/skin_art.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "ro/core/ink.h"

namespace ro::ui {
namespace {

// Les trois morceaux d'une barre de titre. Sans eux tous, on ne peint rien.
constexpr std::array<render::SkinPiece, 3> kTitleBarPieces = {render::SkinPiece::kTitleBarLeft,
                                                              render::SkinPiece::kTitleBarMiddle,
                                                              render::SkinPiece::kTitleBarRight};

// La plus grande valeur d'un canal.
constexpr unsigned kChannelMax = 255U;

// Une couleur dont la transparence est multipliée par `alpha` — et tronquée,
// comme ImGui le fait des siennes.
[[nodiscard]] ImU32 TimesAlpha(ImU32 color, float alpha) {
  if (alpha >= 1.0F) {
    return color;
  }
  const auto kept = static_cast<ImU32>(
      static_cast<float>((color & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT) * std::max(alpha, 0.0F));
  return (color & ~IM_COL32_A_MASK) | (kept << IM_COL32_A_SHIFT);
}

// LA SÉLECTION SURVOLÉE ET APPUYÉE, en seizièmes de son opacité : 160 rend 110
// et 210, celles du jeu.
constexpr unsigned kSixteenths = 16U;
constexpr unsigned kHoverOpacitySixteenths = 11U;
constexpr unsigned kActiveOpacitySixteenths = 21U;
// Ce qu'un appui retire à chaque canal : 156, 184, 234 deviennent 126, 160, 224.
constexpr ImU32 kSelectionActiveShade = IM_COL32(206, 222, 244, 255);

// Une couleur dont l'opacité passe à tant de seizièmes, plafonnée à l'opaque.
[[nodiscard]] ImU32 WithOpacitySixteenths(ImU32 color, unsigned sixteenths) {
  const unsigned opacity = std::min(
      kChannelMax, ((color & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT) * sixteenths / kSixteenths);
  return (color & ~IM_COL32_A_MASK) | (opacity << IM_COL32_A_SHIFT);
}

}  // namespace

ImU32 BrightnessTint(float brightness) {
  constexpr float kFullChannel = 255.0F;
  const auto channel =
      static_cast<unsigned>(std::clamp(brightness, 0.0F, 1.0F) * kFullChannel + 0.5F);
  return IM_COL32(channel, channel, channel, 0) | IM_COL32_A_MASK;
}

ImU32 MultiplyTint(ImU32 left, ImU32 right) {
  const auto channel = [left, right](unsigned shift) {
    const unsigned product = ((left >> shift) & kChannelMax) * ((right >> shift) & kChannelMax);
    // ⚠ ARRONDI AU PLUS PROCHE : c'est ce qui garde le blanc neutre — 255 fois
    // un canal rend ce canal, et non un cran de moins.
    return ((product + kChannelMax / 2U) / kChannelMax) << shift;
  };
  return channel(IM_COL32_R_SHIFT) | channel(IM_COL32_G_SHIFT) | channel(IM_COL32_B_SHIFT) |
         channel(IM_COL32_A_SHIFT);
}

namespace {

// La luminance relative d'une couleur, de 0 à 1 : ses canaux ramenés à la
// lumière — la courbe du sRGB défaite —, pesés comme l'œil les voit.
[[nodiscard]] float RelativeLuminance(ImU32 color) {
  constexpr float kLinearKnee = 0.04045F;
  constexpr float kLinearSlope = 12.92F;
  constexpr float kCurveOffset = 0.055F;
  constexpr float kCurveScale = 1.055F;
  constexpr float kCurveExponent = 2.4F;
  const auto linear = [](unsigned channel) {
    const float value = static_cast<float>(channel & kChannelMax) / static_cast<float>(kChannelMax);
    return value <= kLinearKnee ? value / kLinearSlope
                                : std::pow((value + kCurveOffset) / kCurveScale, kCurveExponent);
  };
  constexpr float kRedWeight = 0.2126F;
  constexpr float kGreenWeight = 0.7152F;
  constexpr float kBlueWeight = 0.0722F;
  return kRedWeight * linear(color >> IM_COL32_R_SHIFT) +
         kGreenWeight * linear(color >> IM_COL32_G_SHIFT) +
         kBlueWeight * linear(color >> IM_COL32_B_SHIFT);
}

// Une couleur mêlée d'une autre, `share` de celle-ci ; l'alpha reste le sien.
[[nodiscard]] ImU32 MixedToward(ImU32 color, ImU32 target, float share) {
  const auto channel = [&](unsigned shift) {
    const auto from = static_cast<float>((color >> shift) & kChannelMax);
    const auto to = static_cast<float>((target >> shift) & kChannelMax);
    return static_cast<unsigned>(std::lround(from + (to - from) * share)) << shift;
  };
  return channel(IM_COL32_R_SHIFT) | channel(IM_COL32_G_SHIFT) | channel(IM_COL32_B_SHIFT) |
         (color & IM_COL32_A_MASK);
}

// Les pas du mélange : un seizième à la fois, jusqu'au noir ou au blanc pur.
constexpr int kMixSteps = 16;

}  // namespace

float ContrastRatio(ImU32 left, ImU32 right) {
  constexpr float kFlare = 0.05F;
  const float lighter = std::max(RelativeLuminance(left), RelativeLuminance(right));
  const float darker = std::min(RelativeLuminance(left), RelativeLuminance(right));
  return (lighter + kFlare) / (darker + kFlare);
}

ImU32 ReadableOn(ImU32 ink, ImU32 paper, float least_ratio) {
  if (ContrastRatio(ink, paper) >= least_ratio) {
    return ink;
  }
  const ImU32 opposite =
      ContrastRatio(IM_COL32_WHITE, paper) >= ContrastRatio(IM_COL32_BLACK, paper) ? IM_COL32_WHITE
                                                                                   : IM_COL32_BLACK;
  ImU32 mixed = ink;
  for (int step = 1; step <= kMixSteps; ++step) {
    mixed = MixedToward(ink, opposite, static_cast<float>(step) / static_cast<float>(kMixSteps));
    if (ContrastRatio(mixed, paper) >= least_ratio) {
      break;
    }
  }
  return mixed;
}

bool PaperIsDark(ImU32 paper) {
  return ContrastRatio(IM_COL32_WHITE, paper) > ContrastRatio(IM_COL32_BLACK, paper);
}

ImU32 OnPaper(ImU32 color, ImU32 paper) {
  if (!PaperIsDark(paper)) {
    return MultiplyTint(color, paper | IM_COL32_A_MASK);
  }
  const auto channel_of = [color](unsigned shift) {
    return static_cast<std::uint8_t>((color >> shift) & kChannelMax);
  };
  const core::Rgba8 ink = core::InkForDarkPaper({.red = channel_of(IM_COL32_R_SHIFT),
                                                 .green = channel_of(IM_COL32_G_SHIFT),
                                                 .blue = channel_of(IM_COL32_B_SHIFT),
                                                 .alpha = static_cast<std::uint8_t>(kChannelMax)});
  // L'ENCRE POSÉE SUR LE PAPIER, à son opacité.
  const unsigned share = ink.alpha;
  const auto laid = [paper, share](unsigned shift, std::uint8_t over) {
    const unsigned under = (paper >> shift) & kChannelMax;
    return ((under * (kChannelMax - share) + static_cast<unsigned>(over) * share +
             kChannelMax / 2U) /
            kChannelMax)
           << shift;
  };
  return laid(IM_COL32_R_SHIFT, ink.red) | laid(IM_COL32_G_SHIFT, ink.green) |
         laid(IM_COL32_B_SHIFT, ink.blue) | (color & IM_COL32_A_MASK);
}

ImU32 OnBackground(ImU32 color, ImU32 background) {
  return PaperIsDark(background) ? OnPaper(color, background) : color;
}

ImU32 InkOn(ImU32 color, ImU32 background) {
  return ReadableOn(OnBackground(color, background), background);
}

ImU32 SkinLook::Tone(SkinTone tone) const {
  switch (tone) {
    case SkinTone::kFaint:
      return faint_text;
    case SkinTone::kGood:
      return good_text;
    case SkinTone::kBad:
      return bad_text;
    case SkinTone::kWarn:
      return warn_text;
    case SkinTone::kInfo:
      return info_text;
    case SkinTone::kText:
      break;
  }
  return text;
}

ImU32 SkinLook::ArtTint() const { return MultiplyTint(art_tint, BrightnessTint(art_brightness)); }

ImU32 SkinLook::SelectionHover() const {
  return WithOpacitySixteenths(selection, kHoverOpacitySixteenths);
}

ImU32 SkinLook::SelectionActive() const {
  return WithOpacitySixteenths(MultiplyTint(selection, kSelectionActiveShade),
                               kActiveOpacitySixteenths);
}

ImU32 PaintedTint(ImU32 tint, bool disabled) {
  const ImGuiStyle& style = ImGui::GetStyle();
  if (!disabled) {
    return TimesAlpha(tint, style.Alpha);
  }
  // Sous `BeginDisabled`, l'alpha du style porte AUSSI l'effacement du grisage :
  // la teinte grisée le remplace, on le retire donc.
  const float undimmed =
      style.DisabledAlpha > 0.0F ? style.Alpha / style.DisabledAlpha : style.Alpha;
  return TimesAlpha(MultiplyTint(tint, kSkinDisabledArtTint), undimmed);
}

bool SkinArt::dresses_windows() const {
  return std::ranges::all_of(kTitleBarPieces,
                             [this](render::SkinPiece piece) { return Get(piece).usable(); });
}

void PaintArt(ImDrawList* into, const SkinArt& art, ImTextureID id, ImVec2 top_left,
              ImVec2 bottom_right) {
  into->AddImage(id, top_left, bottom_right, ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                 PaintedTint(art.look().ArtTint()));
}

void PaintFrameOn(ImDrawList* into, ImTextureID id, ImVec2 top_left, ImVec2 bottom_right,
                  ImU32 fill) {
  into->AddImage(id, top_left, bottom_right, ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                 PaintedTint(fill | IM_COL32_A_MASK));
}

void PaintOnPaper(ImDrawList* into, const SkinImage& image, ImVec2 top_left, ImVec2 bottom_right,
                  ImU32 paper, ImVec2 uv_min, ImVec2 uv_max) {
  if (!image.usable()) {
    return;
  }
  const bool turned = image.dark_paper_id != 0 && PaperIsDark(paper);
  into->AddImage(turned ? image.dark_paper_id : image.id, top_left, bottom_right, uv_min, uv_max,
                 PaintedTint(turned ? IM_COL32_WHITE : paper | IM_COL32_A_MASK));
}

StripRects HorizontalStrip(ImVec2 top_left, ImVec2 bottom_right, float left_width,
                           float right_width) {
  const float span = bottom_right.x - top_left.x;
  // ⚠ LES DEUX BOUTS D'ABORD, ET RÉDUITS S'IL LE FAUT. Une barre plus étroite
  // que ses deux extrémités doit montrer un morceau de chacune, et non un
  // milieu de largeur négative qui se peindrait à l'envers.
  const float left = std::min(left_width, span * 0.5F);
  const float right = std::min(right_width, span - left);
  const float middle_start = top_left.x + left;
  const float middle_end = bottom_right.x - right;

  StripRects rects;
  rects.left = ImVec4(top_left.x, top_left.y, middle_start, bottom_right.y);
  rects.middle =
      ImVec4(middle_start, top_left.y, std::max(middle_start, middle_end), bottom_right.y);
  rects.right =
      ImVec4(std::max(middle_start, middle_end), top_left.y, bottom_right.x, bottom_right.y);
  return rects;
}

NineSliceRects NineSlice(ImVec2 top_left, ImVec2 bottom_right, ImVec2 corner) {
  const float width = bottom_right.x - top_left.x;
  const float height = bottom_right.y - top_left.y;
  // Même règle que pour la bande, sur les deux axes : un cadre plus petit que
  // ses coins montre un bout de chaque coin, et rien entre eux.
  const float cut_x = std::min(corner.x, width * 0.5F);
  const float cut_y = std::min(corner.y, height * 0.5F);

  const std::array<float, 4> xs = {top_left.x, top_left.x + cut_x, bottom_right.x - cut_x,
                                   bottom_right.x};
  const std::array<float, 4> ys = {top_left.y, top_left.y + cut_y, bottom_right.y - cut_y,
                                   bottom_right.y};

  NineSliceRects rects;
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      rects[row * 3 + column] = ImVec4(xs[column], ys[row], xs[column + 1], ys[row + 1]);
    }
  }
  return rects;
}

}  // namespace ro::ui

#include "ro/render/head_button.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "ro/render/sprite_frame.h"
#include "ro/render/sprite_vignette.h"

namespace ro::render {
namespace {

constexpr std::int32_t kChannels = 4;
constexpr std::size_t kAlpha = 3;
constexpr float kOpaque = 255.0F;
constexpr std::uint8_t kOpaqueByte = 255;
constexpr float kHalf = 0.5F;

// LA COUPE D'UN COIN INTÉRIEUR PERD UN PIXEL DE L'IMAGE : un rectangle rentré
// de deux pixels sur chaque bord, coupé comme le sien, laisserait au liseré une
// diagonale plus épaisse que ses côtés.
constexpr std::int32_t kInnerCutTrimPx = 1;
// La marge entre le fond et la tête, en pixels de l'image.
constexpr std::int32_t kPortraitMarginPx = 1;

// LA POSE D'UNE TÊTE QUI REGARDE LE JOUEUR : de face, figée sur sa première
// image — l'attente d'un joueur ne s'anime pas, ses images sont les trois
// orientations du regard.
constexpr std::size_t kFacingViewer = 0;
constexpr float kFrozenMs = 0.0F;
constexpr std::size_t kLookingAhead = 0;

[[nodiscard]] constexpr std::int32_t Px(std::int32_t interface_px) noexcept {
  return interface_px * kHeadButtonDensity;
}

[[nodiscard]] constexpr std::int32_t DropOf(bool pressed) noexcept {
  return pressed ? Px(kHeadButtonPressDrop) : 0;
}

// La face entière, liseré compris.
[[nodiscard]] CutCornerBox FaceArea(bool pressed) noexcept {
  const std::int32_t drop = DropOf(pressed);
  return CutCornerBox{.left = 0,
                      .top = drop,
                      .right = Px(kHeadButtonFaceSide) - 1,
                      .bottom = Px(kHeadButtonFaceSide) - 1 + drop,
                      .cut = Px(kHeadButtonCornerCut)};
}

// Sous le liseré : le filet, puis le fond qui le recouvre en bas et à droite.
[[nodiscard]] CutCornerBox FiletArea(bool pressed) noexcept {
  const CutCornerBox face = FaceArea(pressed);
  const std::int32_t rim = Px(kHeadButtonRim);
  return CutCornerBox{.left = face.left + rim,
                      .top = face.top + rim,
                      .right = face.right - rim,
                      .bottom = face.bottom - rim,
                      .cut = face.cut - kInnerCutTrimPx};
}

[[nodiscard]] CutCornerBox ShadowArea(bool pressed) noexcept {
  CutCornerBox shadow = FaceArea(pressed);
  shadow.left += Px(kHeadButtonShadowRight);
  shadow.right += Px(kHeadButtonShadowRight);
  shadow.top += Px(kHeadButtonShadowDown);
  shadow.bottom += Px(kHeadButtonShadowDown);
  return shadow;
}

void Paint(formats::Image& image, std::int32_t x, std::int32_t y, ButtonColor color) {
  const auto at = static_cast<std::size_t>((y * image.width + x) * kChannels);
  image.pixels[at] = color.red;
  image.pixels[at + 1] = color.green;
  image.pixels[at + 2] = color.blue;
  image.pixels[at + kAlpha] = kOpaqueByte;
}

void Fill(formats::Image& image, const CutCornerBox& box, ButtonColor color) {
  for (std::int32_t y = std::max(box.top, 0); y <= std::min(box.bottom, image.height - 1); ++y) {
    for (std::int32_t x = std::max(box.left, 0); x <= std::min(box.right, image.width - 1); ++x) {
      if (box.Holds(x, y)) {
        Paint(image, x, y, color);
      }
    }
  }
}

// La boîte des pixels visibles d'une image, bornes comprises ; vide sans aucun.
struct VisibleBounds {
  std::int32_t left = 0;
  std::int32_t top = 0;
  std::int32_t right = -1;
  std::int32_t bottom = -1;

  [[nodiscard]] bool empty() const noexcept { return right < left || bottom < top; }
  [[nodiscard]] std::int32_t width() const noexcept { return right - left + 1; }
  [[nodiscard]] std::int32_t height() const noexcept { return bottom - top + 1; }
};

[[nodiscard]] std::uint8_t AlphaAt(const formats::Image& image, std::int32_t x, std::int32_t y) {
  return image.pixels[static_cast<std::size_t>((y * image.width + x) * kChannels) + kAlpha];
}

[[nodiscard]] VisibleBounds VisibleBoundsOf(const formats::Image& image) {
  VisibleBounds bounds{.left = image.width, .top = image.height};
  for (std::int32_t y = 0; y < image.height; ++y) {
    for (std::int32_t x = 0; x < image.width; ++x) {
      if (AlphaAt(image, x, y) == 0) {
        continue;
      }
      bounds.left = std::min(bounds.left, x);
      bounds.right = std::max(bounds.right, x);
      bounds.top = std::min(bounds.top, y);
      bounds.bottom = std::max(bounds.bottom, y);
    }
  }
  return bounds;
}

// La longueur commune de deux intervalles semi-ouverts.
[[nodiscard]] float Overlap(float low, float high, float other_low, float other_high) noexcept {
  return std::max(0.0F, std::min(high, other_high) - std::max(low, other_low));
}

// OÙ LA TÊTE TOMBE : son coin dans l'image, et de combien elle grandit.
struct PortraitPlacement {
  float scale = 0.0F;
  std::int32_t left = 0;
  std::int32_t top = 0;
};

[[nodiscard]] PortraitPlacement PlacePortrait(const VisibleBounds& head, const CutCornerBox& area) {
  const auto room_width = static_cast<float>(area.right - area.left + 1);
  const auto room_height = static_cast<float>(area.bottom - area.top + 1);
  const float scale = std::min(room_width / static_cast<float>(head.width()),
                               room_height / static_cast<float>(head.height()));
  // ⚠ LE COIN TOMBE SUR UN PIXEL ENTIER, arrondi vers le haut-gauche : un
  // centrage au demi-pixel adoucirait tous les bords de la tête, et l'arrondi
  // inverse la pousserait hors de sa place.
  const float spare_width = room_width - (static_cast<float>(head.width()) * scale);
  const float spare_height = room_height - (static_cast<float>(head.height()) * scale);
  return PortraitPlacement{
      .scale = scale,
      .left = area.left + static_cast<std::int32_t>(std::floor(spare_width * kHalf)),
      .top = area.top + static_cast<std::int32_t>(std::floor(spare_height * kHalf))};
}

// La tête, moyennée sur le pixel (x, y) de l'image, en couleur prémultipliée.
struct PremultipliedPixel {
  std::array<float, 3> color{};
  float alpha = 0.0F;
};

[[nodiscard]] PremultipliedPixel SampleHead(const formats::Image& head, const VisibleBounds& bounds,
                                            const PortraitPlacement& place, std::int32_t x,
                                            std::int32_t y) {
  // Le pixel de l'image, ramené en pixels de sprite.
  const float low_x = static_cast<float>(x - place.left) / place.scale;
  const float high_x = static_cast<float>(x + 1 - place.left) / place.scale;
  const float low_y = static_cast<float>(y - place.top) / place.scale;
  const float high_y = static_cast<float>(y + 1 - place.top) / place.scale;
  const float area = (high_x - low_x) * (high_y - low_y);
  PremultipliedPixel sum;
  const auto first_x = std::max(0, static_cast<std::int32_t>(std::floor(low_x)));
  const auto first_y = std::max(0, static_cast<std::int32_t>(std::floor(low_y)));
  const auto last_x = std::min(bounds.width() - 1, static_cast<std::int32_t>(std::ceil(high_x)));
  const auto last_y = std::min(bounds.height() - 1, static_cast<std::int32_t>(std::ceil(high_y)));
  for (std::int32_t source_y = first_y; source_y <= last_y; ++source_y) {
    const float cover_y =
        Overlap(low_y, high_y, static_cast<float>(source_y), static_cast<float>(source_y + 1));
    for (std::int32_t source_x = first_x; source_x <= last_x; ++source_x) {
      const float cover = cover_y * Overlap(low_x, high_x, static_cast<float>(source_x),
                                            static_cast<float>(source_x + 1));
      if (cover <= 0.0F) {
        continue;
      }
      const auto at = static_cast<std::size_t>(
          ((source_y + bounds.top) * head.width + source_x + bounds.left) * kChannels);
      const float alpha = static_cast<float>(head.pixels[at + kAlpha]) / kOpaque;
      for (std::size_t channel = 0; channel < sum.color.size(); ++channel) {
        sum.color[channel] += cover * alpha * static_cast<float>(head.pixels[at + channel]);
      }
      sum.alpha += cover * alpha;
    }
  }
  for (float& channel : sum.color) {
    channel /= area;
  }
  sum.alpha /= area;
  return sum;
}

[[nodiscard]] formats::Image PlaceHead(const formats::Image& head, const VisibleBounds& bounds,
                                       bool pressed) {
  formats::Image image = DrawHeadButtonFrame(pressed);
  const CutCornerBox area = HeadButtonPortraitArea(pressed);
  const PortraitPlacement place = PlacePortrait(bounds, area);
  for (std::int32_t y = area.top; y <= area.bottom; ++y) {
    for (std::int32_t x = area.left; x <= area.right; ++x) {
      if (!area.Holds(x, y)) {
        continue;
      }
      const PremultipliedPixel sample = SampleHead(head, bounds, place, x, y);
      if (sample.alpha <= 0.0F) {
        continue;
      }
      // LA TÊTE PAR-DESSUS LE FOND, qui est opaque : l'image le reste.
      const auto at = static_cast<std::size_t>((y * image.width + x) * kChannels);
      for (std::size_t channel = 0; channel < sample.color.size(); ++channel) {
        const float under = static_cast<float>(image.pixels[at + channel]);
        const float over = sample.color[channel] + (under * (1.0F - sample.alpha));
        image.pixels[at + channel] = static_cast<std::uint8_t>(
            std::clamp(static_cast<int>(std::lround(over)), 0, static_cast<int>(kOpaqueByte)));
      }
    }
  }
  return image;
}

}  // namespace

bool CutCornerBox::Holds(std::int32_t x, std::int32_t y) const noexcept {
  if (x < left || x > right || y < top || y > bottom) {
    return false;
  }
  return (x - left) + (y - top) >= cut && (right - x) + (y - top) >= cut &&
         (x - left) + (bottom - y) >= cut && (right - x) + (bottom - y) >= cut;
}

CutCornerBox HeadButtonFillArea(bool pressed) noexcept {
  CutCornerBox fill = FiletArea(pressed);
  fill.left += Px(kHeadButtonFilet);
  fill.top += Px(kHeadButtonFilet);
  return fill;
}

CutCornerBox HeadButtonPortraitArea(bool pressed) noexcept {
  CutCornerBox area = HeadButtonFillArea(pressed);
  area.left += kPortraitMarginPx;
  area.top += kPortraitMarginPx;
  area.right -= kPortraitMarginPx;
  area.bottom -= kPortraitMarginPx;
  return area;
}

formats::Image DrawHeadButtonFrame(bool pressed) {
  formats::Image image;
  image.width = kHeadButtonWidthPx;
  image.height = kHeadButtonHeightPx;
  image.pixels.assign(static_cast<std::size_t>(image.width * image.height * kChannels), 0);
  Fill(image, ShadowArea(pressed), kHeadButtonShadowColor);
  Fill(image, FaceArea(pressed), kHeadButtonRimColor);
  Fill(image, FiletArea(pressed), pressed ? kHeadButtonPressedFiletColor : kHeadButtonFiletColor);
  Fill(image, HeadButtonFillArea(pressed),
       pressed ? kHeadButtonPressedFillColor : kHeadButtonFillColor);
  return image;
}

HeadButtonImages ComposeHeadButton(const formats::Image& head) {
  if (head.empty() || head.width <= 0 || head.height <= 0 ||
      head.pixels.size() < static_cast<std::size_t>(head.width * head.height * kChannels)) {
    return {};
  }
  const VisibleBounds bounds = VisibleBoundsOf(head);
  if (bounds.empty()) {
    return {};
  }
  return HeadButtonImages{.normal = PlaceHead(head, bounds, /*pressed=*/false),
                          .pressed = PlaceHead(head, bounds, /*pressed=*/true)};
}

formats::Image HeadPortraitOf(ActorWardrobe& wardrobe, const ActorLook& look) {
  ActorOutfit* outfit = wardrobe.HeadOnly(look);
  if (outfit == nullptr) {
    return {};
  }
  const ComposedActor& composed =
      outfit->Composed(kAnimationIdle, kFacingViewer, kFrozenMs, /*loop=*/true, kLookingAhead);
  const OutfitSpan span = MeasurePoseSpan(*outfit, composed);
  if (!span.measured || span.all.empty()) {
    return {};
  }
  // 🔴 À L'ÉCHELLE UN, SUR DES PIXELS ENTIERS : le canevas commence au pixel
  // qui contient le bord haut-gauche de la tête. Un canevas centré au
  // demi-pixel échantillonnerait chaque pixel de sprite entre deux, et la tête
  // arriverait floue avant même d'être agrandie.
  const float left = std::floor(span.all.min.x);
  const float top = std::floor(span.all.min.y);
  VignetteBox box;
  box.width = static_cast<std::int32_t>(std::ceil(span.all.max.x) - left);
  box.height = static_cast<std::int32_t>(std::ceil(span.all.max.y) - top);
  VignetteFraming framing;
  framing.scale = 1.0F;
  framing.origin = glm::vec2(-left, -top);
  return DrawVignette(composed, box, framing);
}

ActorLook HeadLookOf(const StatusHeadKey& key) {
  ActorLook look;
  look.player = true;
  look.job = key.job;
  look.sex = key.sex;
  look.hair = key.hair;
  look.hair_palette = key.hair_palette;
  return look;
}

bool StatusHeadArt::Refresh(const std::optional<StatusHeadKey>& key) {
  if (key == drawn_for_) {
    return false;
  }
  drawn_for_ = key;
  const bool had_images = !images_.empty();
  if (!key.has_value()) {
    images_ = HeadButtonImages{};
    return had_images;
  }
  images_ = ComposeHeadButton(HeadPortraitOf(*wardrobe_, HeadLookOf(*key)));
  ++composed_count_;
  return had_images || !images_.empty();
}

}  // namespace ro::render

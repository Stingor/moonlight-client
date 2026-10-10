#include "ro/render/armed_skill.h"

#include <algorithm>

namespace ro::render {
namespace {

constexpr float kPercentFull = 100.0F;

}  // namespace

std::array<std::uint8_t, 4> ArmedBannerColorBytes(std::uint32_t color) noexcept {
  return {static_cast<std::uint8_t>(color & 0xFFU), static_cast<std::uint8_t>((color >> 8) & 0xFFU),
          static_cast<std::uint8_t>((color >> 16) & 0xFFU),
          static_cast<std::uint8_t>((color >> 24) & 0xFFU)};
}

glm::vec2 ArmedBannerBaseline(const ArmedSkillLook& look, float viewport_width,
                              float viewport_height) noexcept {
  const float height = std::clamp(look.banner_height_percent, kSmallestArmedBannerHeightPercent,
                                  kLargestArmedBannerHeightPercent);
  return {viewport_width / 2.0F, viewport_height * height / kPercentFull};
}

glm::vec2 ArmedLevelDigitAt(const ArmedSkillLook& look, glm::vec2 cursor_px,
                            std::size_t digit_count, std::size_t index) noexcept {
  const float scale =
      std::clamp(look.level_scale, kSmallestArmedLevelScale, kLargestArmedLevelScale);
  const float offset_x = std::clamp(look.level_offset_x_pixels, kSmallestArmedLevelOffsetPixels,
                                    kLargestArmedLevelOffsetPixels);
  const float pitch = kArmedLevelDigitPitchPixels * scale;
  // Le chiffre de poids fort à l'écart réglé, et un pas de plus par rang vers la
  // droite : les unités finissent le nombre.
  const auto last = static_cast<float>(digit_count == 0 ? 0 : digit_count - 1);
  return {cursor_px.x + offset_x + pitch * (last - static_cast<float>(index)), cursor_px.y};
}

}  // namespace ro::render

// La lueur élémentaire. Voir l'en-tête.

#include "ro/render/element_glow.h"

#include <array>
#include <cstdint>

#include "ro/render/actor_layers.h"

namespace ro::render {

std::array<std::uint8_t, 3> ElementHueRgb(ElementHue hue) noexcept {
  switch (hue) {
    case ElementHue::kWater:
      return {70, 150, 255};
    case ElementHue::kEarth:
      return {170, 115, 55};
    case ElementHue::kFire:
      return {255, 140, 40};
    case ElementHue::kWind:
      return {190, 240, 70};
    case ElementHue::kPoison:
      return {80, 220, 80};
    case ElementHue::kHoly:
      return {255, 235, 160};
    case ElementHue::kShadow:
      return {150, 70, 220};
    case ElementHue::kGhost:
      return {205, 225, 255};
    case ElementHue::kUndead:
      return {150, 25, 35};
  }
  return {255, 255, 255};
}

ActorBodyCopy WeaponGlowCopy(ElementHue hue) noexcept {
  const std::array<std::uint8_t, 3> rgb = ElementHueRgb(hue);
  ActorBodyCopy copy;
  copy.tint = {rgb[0], rgb[1], rgb[2], kWeaponGlowAlpha};
  copy.parts = PartBit(ActorPart::kWeapon);
  copy.additive = true;
  copy.in_front = false;
  copy.grow_pixels = kWeaponGlowGrowPixels;
  return copy;
}

std::array<std::uint8_t, 4> ArrowGlowTint(ElementHue hue) noexcept {
  const std::array<std::uint8_t, 3> rgb = ElementHueRgb(hue);
  return {rgb[0], rgb[1], rgb[2], kArrowGlowAlpha};
}

}  // namespace ro::render

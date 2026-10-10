#include "ro/render/vanish_fade.h"

#include <algorithm>
#include <cmath>

namespace ro::render {
namespace {

// L'opacité pleine, celle d'un corps qui ne s'efface pas encore.
constexpr double kOpaqueAlpha = 255.0;

// L'instant, compté depuis l'annonce de la mort, où l'agonie finit.
std::uint32_t AgonyEndMs(float agony_ms) noexcept {
  const float agony = agony_ms > 0.0F ? agony_ms : 0.0F;
  return static_cast<std::uint32_t>(std::lround(agony));
}

// Une durée nulle effacerait le cadavre avant sa première image.
std::uint32_t Bounded(std::uint32_t corpse_fade_ms) noexcept {
  return std::max<std::uint32_t>(corpse_fade_ms, 1);
}

}  // namespace

std::uint8_t VanishAlphaAt(float agony_ms, std::uint32_t since_death_ms,
                           std::uint32_t corpse_fade_ms) noexcept {
  const std::uint32_t agony_end_ms = AgonyEndMs(agony_ms);
  if (since_death_ms <= agony_end_ms) {
    return static_cast<std::uint8_t>(kOpaqueAlpha);
  }
  const std::uint32_t fade_ms = Bounded(corpse_fade_ms);
  const std::uint32_t corpse_age_ms = since_death_ms - agony_end_ms;
  if (corpse_age_ms >= fade_ms) {
    return 0;
  }
  // ⚠ TRONQUÉ, comme le client : le premier cran tombe dès la première
  // milliseconde du cadavre.
  const double alpha = kOpaqueAlpha - kOpaqueAlpha * static_cast<double>(corpse_age_ms) / fade_ms;
  return static_cast<std::uint8_t>(std::clamp(alpha, 0.0, kOpaqueAlpha));
}

std::uint8_t AppearAlphaAt(std::uint32_t since_appeared_ms, std::uint32_t appear_fade_ms) noexcept {
  if (appear_fade_ms == 0 || since_appeared_ms >= appear_fade_ms) {
    return static_cast<std::uint8_t>(kOpaqueAlpha);
  }
  const double from = kAppearFromAlpha;
  const double alpha =
      from + (kOpaqueAlpha - from) * static_cast<double>(since_appeared_ms) / appear_fade_ms;
  return static_cast<std::uint8_t>(std::clamp(alpha, from, kOpaqueAlpha));
}

std::uint8_t LeaveAlphaAt(std::uint32_t since_left_ms, std::uint32_t leave_fade_ms) noexcept {
  if (since_left_ms >= leave_fade_ms) {
    return 0;
  }
  const double alpha =
      kOpaqueAlpha * static_cast<double>(leave_fade_ms - since_left_ms) / leave_fade_ms;
  return static_cast<std::uint8_t>(std::clamp(alpha, 0.0, kOpaqueAlpha));
}

bool VanishIsOver(float agony_ms, std::uint32_t since_death_ms,
                  std::uint32_t corpse_fade_ms) noexcept {
  const std::uint32_t agony_end_ms = AgonyEndMs(agony_ms);
  return since_death_ms > agony_end_ms && since_death_ms - agony_end_ms >= Bounded(corpse_fade_ms);
}

}  // namespace ro::render

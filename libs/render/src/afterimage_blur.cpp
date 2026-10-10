#include "ro/render/afterimage_blur.h"

#include <cmath>
#include <limits>

namespace ro::render {
namespace {

// Les couleurs que les états donnent, rouge, vert, bleu.
constexpr std::array<float, 3> kQuickenYellow = {200.0F, 200.0F, 0.0F};
constexpr std::array<float, 3> kOverthrustPink = {250.0F, 150.0F, 150.0F};
constexpr std::array<float, 3> kEnergyCoatBlue = {180.0F, 180.0F, 250.0F};
constexpr std::array<float, 3> kFlickerColor = {kAfterimageBlurFlicker, kAfterimageBlurFlicker,
                                                kAfterimageBlurFlicker};

// Les états, numérotés comme le serveur les numérote (EFST).
constexpr std::uint16_t kStateTwoHandQuicken = 2;
constexpr std::uint16_t kStateOverthrust = 25;
constexpr std::uint16_t kStateEnergyCoat = 31;
constexpr std::uint16_t kStateSpearQuicken = 68;
constexpr std::uint16_t kStateExplosionSpirits = 86;
constexpr std::uint16_t kStateOneHandQuicken = 161;
constexpr std::uint16_t kStateOverthrustMax = 188;

// Une composante que l'écriture laisse telle quelle : l'entier −1.
constexpr int kChannelUnchanged = -1;
constexpr std::uint32_t kByteMask = 0xFFU;

// Le clignotement : blanc une fois sur quatorze, rose sinon.
constexpr std::uint32_t kFlickerDraws = 14;
constexpr std::uint8_t kFlickerRed = 250;
constexpr std::uint8_t kFlickerWhite = 250;
constexpr std::uint8_t kFlickerPink = 200;
constexpr std::array<std::uint8_t, 3> kPaleYellow = {250, 250, 200};
constexpr std::uint8_t kWhiteChannel = 255;

// La conversion du client : troncature vers zéro, puis l'octet de poids faible
// de l'entier. Rien quand l'entier vaut −1, que l'écriture saute.
std::optional<std::uint8_t> ChannelOf(float value) noexcept {
  // Hors de la portée d'un entier, la conversion rend l'entier « indéfini »,
  // dont l'octet faible est nul.
  if (!(std::fabs(value) < static_cast<float>(std::numeric_limits<std::int32_t>::max()))) {
    return std::uint8_t{0};
  }
  const auto whole = static_cast<std::int32_t>(value);
  if (whole == kChannelUnchanged) {
    return std::nullopt;
  }
  return static_cast<std::uint8_t>(static_cast<std::uint32_t>(whole) & kByteMask);
}

}  // namespace

std::optional<std::array<float, 3>> AfterimageBlurStateColor(std::uint16_t state_id) noexcept {
  switch (state_id) {
    case kStateTwoHandQuicken:
    case kStateOneHandQuicken:
    case kStateSpearQuicken:
      return kQuickenYellow;
    case kStateOverthrust:
    case kStateOverthrustMax:
      return kOverthrustPink;
    case kStateEnergyCoat:
      return kEnergyCoatBlue;
    case kStateExplosionSpirits:
      return kFlickerColor;
    default:
      return std::nullopt;
  }
}

std::array<std::uint8_t, 4> AfterimageBlurCarrierTint(std::array<std::uint8_t, 4> tint,
                                                      const AfterimageBlur& blur,
                                                      std::uint32_t draw) noexcept {
  const std::array<std::int16_t, 4> written = AfterimageBlurWrittenTint(blur, draw);
  // La table range l'alpha en tête ; la teinte peinte, en queue.
  for (std::size_t channel = 0; channel < blur.color.size(); ++channel) {
    if (const std::int16_t value = written[channel + 1]; value != kTintUnchanged) {
      tint[channel] = static_cast<std::uint8_t>(value);
    }
  }
  return tint;
}

std::array<std::int16_t, 4> AfterimageBlurWrittenTint(const AfterimageBlur& blur,
                                                      std::uint32_t draw) noexcept {
  std::array<std::int16_t, 4> written = {kTintUnchanged, kTintUnchanged, kTintUnchanged,
                                         kTintUnchanged};
  if (blur.color[0] == kAfterimageBlurFlicker) {
    const std::uint8_t other = draw % kFlickerDraws == 0 ? kFlickerWhite : kFlickerPink;
    written[1] = kFlickerRed;
    written[2] = other;
    written[3] = other;
    return written;
  }
  if (blur.color[0] == kAfterimageBlurPale) {
    written[1] = kPaleYellow[0];
    written[2] = kPaleYellow[1];
    written[3] = kPaleYellow[2];
    return written;
  }
  for (std::size_t channel = 0; channel < blur.color.size(); ++channel) {
    if (const std::optional<std::uint8_t> value = ChannelOf(blur.color[channel])) {
      written[channel + 1] = *value;
    }
  }
  return written;
}

bool AfterimageBlurSowsAt(const AfterimageBlur& blur, Motion motion) noexcept {
  if (blur.color[0] == kAfterimageBlurFlicker || blur.color[0] == kAfterimageBlurPale) {
    return false;
  }
  if ((blur.age_ticks % kAfterimageBlurAgeWrapTicks) % kAfterimageBlurSowPeriodTicks != 0) {
    return false;
  }
  switch (motion) {
    case Motion::kIdle:
    case Motion::kSit:
    case Motion::kReady:
      return false;
    case Motion::kWalk:
    case Motion::kPickup:
    case Motion::kAttack:
    case Motion::kCast:
    case Motion::kHurt:
    case Motion::kDie:
      return true;
  }
  return false;
}

bool AfterimageBlurSowsSince(const AfterimageBlur& blur,
                             std::optional<std::uint32_t> previous_age_ticks,
                             Motion motion) noexcept {
  if (!previous_age_ticks.has_value() || *previous_age_ticks >= blur.age_ticks) {
    return AfterimageBlurSowsAt(blur, motion);
  }
  // Le semis du tic enjambé le plus récent décide : l'âge courant s'il est un
  // multiple, sinon le dernier multiple franchi depuis l'image précédente.
  const std::uint32_t wrapped_now = blur.age_ticks % kAfterimageBlurAgeWrapTicks;
  const std::uint32_t last_sowing_tick =
      blur.age_ticks - wrapped_now % kAfterimageBlurSowPeriodTicks;
  if (last_sowing_tick <= *previous_age_ticks) {
    return false;
  }
  AfterimageBlur at_sowing = blur;
  at_sowing.age_ticks = last_sowing_tick;
  return AfterimageBlurSowsAt(at_sowing, motion);
}

std::array<std::uint8_t, 4> AfterimageGhostTint(const AfterimageBlur& blur) noexcept {
  std::array<std::uint8_t, 4> tint = {kWhiteChannel, kWhiteChannel, kWhiteChannel,
                                      kAfterimageGhostAlpha};
  for (std::size_t channel = 0; channel < blur.color.size(); ++channel) {
    if (const std::optional<std::uint8_t> value = ChannelOf(blur.color[channel])) {
      tint[channel] = *value;
    }
  }
  return tint;
}

std::optional<std::uint8_t> AfterimageGhostAlphaAt(std::uint32_t age_ticks) noexcept {
  if (age_ticks >= kAfterimageGhostLifeTicks) {
    return std::nullopt;
  }
  return static_cast<std::uint8_t>(kAfterimageGhostAlpha - kAfterimageGhostFadePerTick * age_ticks);
}

}  // namespace ro::render

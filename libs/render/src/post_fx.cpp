#include "ro/render/post_fx.h"

#include <algorithm>

namespace ro::render {
namespace {

float Within(float value, const PostFxRange& range) {
  return std::clamp(value, range.lowest, range.highest);
}

// Le plancher du gamma avant son inversion : une valeur nulle ou négative lue
// d'un fichier ne doit pas donner un exposant infini.
constexpr float kLeastGamma = 0.1F;

// L'horloge du grain se replie sur seize bits de millisecondes.
constexpr std::uint32_t kGrainClockWrapMs = 0xFFFF;
constexpr float kMillisecondsPerSecond = 1000.0F;

}  // namespace

PostFxSettings Clamped(PostFxSettings settings) {
  settings.brightness = Within(settings.brightness, kBrightnessRange);
  settings.contrast = Within(settings.contrast, kContrastRange);
  settings.gamma = Within(settings.gamma, kGammaRange);
  settings.saturation = Within(settings.saturation, kSaturationRange);
  settings.temperature = Within(settings.temperature, kTemperatureRange);
  settings.filter = std::clamp(settings.filter, 0, static_cast<int>(PostFxFilter::kCount) - 1);
  settings.vignette = Within(settings.vignette, kEffectRange);
  settings.grain = Within(settings.grain, kEffectRange);
  settings.aberration = Within(settings.aberration, kEffectRange);
  settings.sharpen = Within(settings.sharpen, kEffectRange);
  settings.fxaa_strength = Within(settings.fxaa_strength, kEffectRange);
  return settings;
}

PostFxSettings PresetSettings(PostFxPreset preset) {
  PostFxSettings settings;
  switch (preset) {
    case PostFxPreset::kNight:
      settings.brightness = -0.06F;
      settings.contrast = 1.08F;
      settings.saturation = 0.85F;
      settings.temperature = -0.25F;
      settings.vignette = 0.35F;
      break;
    case PostFxPreset::kCinema:
      settings.contrast = 1.15F;
      settings.saturation = 1.12F;
      settings.vignette = 0.30F;
      settings.sharpen = 0.30F;
      settings.aberration = 0.20F;
      settings.fxaa = true;
      break;
    case PostFxPreset::kRetro:
      settings.contrast = 1.10F;
      settings.saturation = 1.20F;
      settings.grain = 0.35F;
      settings.vignette = 0.45F;
      break;
    case PostFxPreset::kBlackAndWhite:
      settings.filter = static_cast<int>(PostFxFilter::kGrayscale);
      break;
    case PostFxPreset::kNeutral:
    case PostFxPreset::kCount:
      break;
  }
  settings.enabled = true;
  return settings;
}

ColorPassConstants ColorPassConstantsOf(const PostFxSettings& settings, std::uint32_t now_ms,
                                        int source_width, int source_height) {
  ColorPassConstants constants;
  const float gamma = std::max(settings.gamma, kLeastGamma);
  constants.grade = {settings.brightness, settings.contrast, 1.0F / gamma, settings.saturation};
  constants.tone = {settings.temperature, static_cast<float>(settings.filter), settings.vignette,
                    settings.grain};
  const float seconds = static_cast<float>(now_ms & kGrainClockWrapMs) / kMillisecondsPerSecond;
  constants.effects = {settings.aberration, settings.sharpen, seconds, 0.0F};
  constants.texel = {source_width > 0 ? 1.0F / static_cast<float>(source_width) : 0.0F,
                     source_height > 0 ? 1.0F / static_cast<float>(source_height) : 0.0F};
  return constants;
}

}  // namespace ro::render

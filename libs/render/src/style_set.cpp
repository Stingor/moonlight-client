#include "ro/render/style_set.h"

#include <algorithm>
#include <array>

namespace ro::render {
namespace {

// Les montures dont la peau ne se règle pas : voir `SkinAdjustable`.
//
// Les quatorze autruches, sous tous les métiers et les deux sexes : leur nom
// de fichier commence ainsi.
constexpr std::string_view kOstrichPrefix = "타조";
// Le lion de la Royal Guard en tenue alternative. Le lion ordinaire peint sa
// crinière ailleurs, et garde une peau réglable.
constexpr std::array<std::string_view, 1> kUnadjustableSkinEndings = {
    "costume_1\\사자로얄가드_여_1",
};
constexpr char kPathSeparator = '\\';

}  // namespace

PaletteRecipe LayerRecipe(const LayerDye& dye) {
  PaletteRecipe recipe;
  std::copy_n(dye.adjusts.begin(), kMaxRamps, recipe.ramps.begin());
  return recipe;
}

HueClassAdjusts LayerHueClasses(const LayerDye& dye) {
  HueClassAdjusts adjusts{};
  std::copy_n(dye.adjusts.begin(), kHueClassCount, adjusts.begin());
  return adjusts;
}

std::optional<RampLayer> RampLayerOf(StyleLayer layer) noexcept {
  switch (layer) {
    case StyleLayer::kHead:
      return RampLayer::kHead;
    case StyleLayer::kHeadgear:
      return RampLayer::kHeadgear;
    case StyleLayer::kGarment:
      return RampLayer::kGarment;
    case StyleLayer::kBody:
    case StyleLayer::kSkin:
      break;
  }
  return std::nullopt;
}

const LayerDye* StyleSet::Find(StyleLayer layer, std::uint32_t key) const {
  const auto found = std::ranges::find_if(
      layers, [layer, key](const LayerDye& dye) { return dye.layer == layer && dye.key == key; });
  return found != layers.end() ? &*found : nullptr;
}

bool SkinAdjustable(std::string_view body_sprite_path) {
  const std::size_t separator = body_sprite_path.rfind(kPathSeparator);
  const std::string_view file_name = separator == std::string_view::npos
                                         ? body_sprite_path
                                         : body_sprite_path.substr(separator + 1);
  if (file_name.starts_with(kOstrichPrefix)) {
    return false;
  }
  return std::ranges::none_of(
      kUnadjustableSkinEndings,
      [body_sprite_path](std::string_view ending) { return body_sprite_path.ends_with(ending); });
}

void ApplySkin(formats::Palette& painted, const formats::Palette& base, SkinRace race,
               const RampAdjust& adjust) {
  for (const std::uint8_t index : SkinIndices(race)) {
    if (adjust.neutral()) {
      painted.entries[index] = base.entries[index];
    } else {
      AdjustColor(base.entries[index].data(), adjust, painted.entries[index].data());
    }
  }
}

}  // namespace ro::render

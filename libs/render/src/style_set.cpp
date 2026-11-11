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

PaletteRecipe BodyContinuationRecipe(const LayerDye& dye) {
  PaletteRecipe recipe;
  std::copy_n(dye.adjusts.begin(), kBodyContinuationRamps, recipe.ramps.begin());
  return recipe;
}

HueClassAdjusts LayerHueClasses(const LayerDye& dye) {
  HueClassAdjusts adjusts{};
  std::copy_n(dye.adjusts.begin(), kHueClassCount, adjusts.begin());
  return adjusts;
}

void ApplyBodyMore(formats::Palette& painted, const formats::Palette& base,
                   std::span<const PaletteRamp> v7, std::span<const PaletteRamp> continuation,
                   SkinRace race, const LayerDye& more) {
  // ⚠ LA SUITE PART DE LA BASE, comme la recette : ses rampes ne recouvrent
  // aucune des huit premières, et chacune se calcule sur ses propres index.
  const formats::Palette continued = ApplyRecipe(base, continuation, BodyContinuationRecipe(more));
  PaletteCover cover{};
  Cover(cover, v7);
  for (const PaletteRamp& ramp : continuation) {
    const std::size_t end = std::min<std::size_t>(
        static_cast<std::size_t>(ramp.start) + ramp.length, formats::kPaletteEntryCount);
    for (std::size_t index = ramp.start; index < end; ++index) {
      painted.entries[index] = continued.entries[index];
      cover[index] = true;
    }
  }
  Cover(cover, SkinIndices(race));
  ApplyRest(painted, base, cover, more.rest);
}

void ApplyLayerRest(formats::Palette& painted, const formats::Palette& base,
                    std::span<const PaletteRamp> ramps, SkinRace race, const LayerDye& dye) {
  if (!LayerTakesRest(dye)) {
    return;
  }
  PaletteCover cover{};
  Cover(cover, ramps);
  if (dye.layer == StyleLayer::kHead) {
    Cover(cover, SkinIndices(race));
  }
  ApplyRest(painted, base, cover, dye.rest);
}

bool LayerTakesRest(const LayerDye& dye) noexcept {
  if (dye.layer == StyleLayer::kBodyMore) {
    return dye.profile == kBodyContinuationProfileNumber;
  }
  return dye.profile == kLayerProfileNumber && RampLayerOf(dye.layer).has_value();
}

bool LayerTakesRetouches(const LayerDye& dye) noexcept {
  return LayerTakesRest(dye) ||
         (dye.profile == kHueClassProfileNumber && RampLayerOf(dye.layer).has_value());
}

std::optional<RampLayer> RampLayerOf(StyleLayer layer) noexcept {
  switch (layer) {
    case StyleLayer::kHead:
      return RampLayer::kHead;
    case StyleLayer::kHeadgear:
      return RampLayer::kHeadgear;
    case StyleLayer::kGarment:
      return RampLayer::kGarment;
    // Un compagnon est un sprite à palette embarquée, comme une coiffe : même
    // seuil, aucune peau.
    case StyleLayer::kCompanion:
      return RampLayer::kHeadgear;
    case StyleLayer::kBody:
    case StyleLayer::kSkin:
    case StyleLayer::kBodyMore:
    case StyleLayer::kBodySkin:
      break;
  }
  return std::nullopt;
}

const LayerDye* StyleSet::Find(StyleLayer layer, std::uint32_t key) const {
  const auto found = std::ranges::find_if(
      layers, [layer, key](const LayerDye& dye) { return dye.layer == layer && dye.key == key; });
  return found != layers.end() ? &*found : nullptr;
}

const LayerDye* StyleSet::SkinOf(StyleLayer piece, std::uint32_t body_key) const {
  const auto usable = [](const LayerDye* dye) {
    return dye != nullptr && dye->profile == kLayerProfileNumber ? dye : nullptr;
  };
  if (piece == StyleLayer::kBody) {
    if (const LayerDye* own = usable(Find(StyleLayer::kBodySkin, body_key))) {
      return own;
    }
  }
  return usable(Find(StyleLayer::kSkin, 0));
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

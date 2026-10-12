#include "character/style_recipe.h"

#include <cstddef>

namespace moonclient {

ro::render::PaletteRecipe FromWire(const ro::sim::StyleRecipe& told) {
  ro::render::PaletteRecipe recipe;
  for (std::size_t rank = 0; rank < ro::sim::kStyleRampCount && rank < ro::render::kMaxRamps;
       ++rank) {
    const ro::sim::StyleAdjust& adjust = told.ramps[rank];
    recipe.ramps[rank] = ro::render::RampAdjust{.hue = adjust.hue,
                                                .saturation = adjust.saturation,
                                                .value = adjust.value,
                                                .absolute = adjust.absolute};
  }
  recipe.body_palette = told.body_palette;
  recipe.hair_palette = told.hair_palette;
  recipe.hair_style = told.hair_style;
  return recipe;
}

ro::sim::StyleRecipe ToWire(const ro::render::PaletteRecipe& recipe) {
  ro::sim::StyleRecipe told;
  for (std::size_t rank = 0; rank < ro::sim::kStyleRampCount && rank < ro::render::kMaxRamps;
       ++rank) {
    const ro::render::RampAdjust& adjust = recipe.ramps[rank];
    told.ramps[rank] = ro::sim::StyleAdjust{.hue = adjust.hue,
                                            .saturation = adjust.saturation,
                                            .value = adjust.value,
                                            .absolute = adjust.absolute};
  }
  told.body_palette = recipe.body_palette;
  told.hair_palette = recipe.hair_palette;
  told.hair_style = recipe.hair_style;
  return told;
}

void Impose(const ro::render::PaletteRecipe& recipe, ro::render::ActorLook& look) {
  if (recipe.body_palette > 0) {
    look.body_palette = recipe.body_palette;
    look.body_palette_names_file = true;
  }
  if (recipe.hair_palette > 0) {
    look.hair_palette = recipe.hair_palette;
  }
  if (recipe.hair_style > 0) {
    look.hair = static_cast<std::uint16_t>(recipe.hair_style);
  }
}

bool SameRecipe(const ro::render::PaletteRecipe& left, const ro::render::PaletteRecipe& right) {
  if (left.body_palette != right.body_palette || left.hair_palette != right.hair_palette ||
      left.hair_style != right.hair_style) {
    return false;
  }
  for (std::size_t rank = 0; rank < ro::render::kMaxRamps; ++rank) {
    const ro::render::RampAdjust& a = left.ramps[rank];
    const ro::render::RampAdjust& b = right.ramps[rank];
    if (a.hue != b.hue || a.saturation != b.saturation || a.value != b.value ||
        a.absolute != b.absolute) {
      return false;
    }
  }
  return true;
}

}  // namespace moonclient

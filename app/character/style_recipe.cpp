#include "character/style_recipe.h"

#include <cstddef>
#include <cstdint>

namespace moonclient {
namespace {

// LES TROIS NUMÉROS QUE LA RECETTE IMPOSE, posés sur l'apparence — la même
// règle pour la recette du fil et pour celle du calcul.
//
// ⚠ SEULEMENT CEUX QU'ELLE IMPOSE : moins un — et zéro, que le serveur emploie
// pour la même chose — veut dire « rien d'imposé », et c'est alors ce que le
// personnage porte déjà qui reste. Écraser par la sentinelle rendrait chauve
// de couleur un joueur dont la recette ne parle que de son costume.
//
// 🔴 LA COIFFURE EST LE NUMÉRO ANNONCÉ, pas un numéro de fichier. C'est
// `sd->status.hair` — ce que le styliste change et que le protocole natif
// diffuse —, et `ActorLook::hair` porte exactement la même chose : la
// permutation des premières coupes se fait une seule fois, quand le catalogue
// des sprites cherche le fichier — voir `ro/render/sprite_catalog.h`.
template <typename Recipe>
void ImposeNumbers(const Recipe& recipe, ro::render::ActorLook& look) {
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

}  // namespace

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
  ImposeNumbers(recipe, look);
}

void Impose(const ro::sim::StyleRecipe& recipe, ro::render::ActorLook& look) {
  ImposeNumbers(recipe, look);
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

#include "character/style_recipe.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <tuple>

namespace moonclient {
namespace {

// 🔴 LES DEUX CÔTÉS PORTENT LE MÊME NOMBRE DE RÉGLAGES : il dimensionne la
// trame d'un côté et la recette de l'autre, et une traduction qui s'arrêterait
// au plus court perdrait des rangs sans le dire.
static_assert(ro::sim::kStyleRampCount == ro::render::kMaxRamps);
static_assert(ro::sim::kStyleLayerAdjustCount == ro::render::kLayerAdjustCount);

[[nodiscard]] ro::render::RampAdjust ToAdjust(const ro::sim::StyleAdjust& adjust) {
  return ro::render::RampAdjust{.hue = adjust.hue,
                                .saturation = adjust.saturation,
                                .value = adjust.value,
                                .absolute = adjust.absolute};
}

[[nodiscard]] ro::sim::StyleAdjust ToWireAdjust(const ro::render::RampAdjust& adjust) {
  return ro::sim::StyleAdjust{.hue = adjust.hue,
                              .saturation = adjust.saturation,
                              .value = adjust.value,
                              .absolute = adjust.absolute};
}

[[nodiscard]] bool SameAdjust(const ro::render::RampAdjust& one,
                              const ro::render::RampAdjust& other) {
  return one.hue == other.hue && one.saturation == other.saturation && one.value == other.value &&
         one.absolute == other.absolute;
}

// LA TEINTE DE VÊTEMENT QUE LA RECETTE IMPOSE, posée sur l'apparence.
//
// ⚠ SEULEMENT SI ELLE EN IMPOSE UNE : moins un — et zéro, que le serveur
// emploie pour la même chose — veut dire « rien d'imposé », et c'est alors ce
// que le personnage porte déjà qui reste.
template <typename Recipe>
void ImposeBodyPaletteOf(const Recipe& recipe, ro::render::ActorLook& look) {
  if (recipe.body_palette > 0) {
    look.body_palette = recipe.body_palette;
    look.body_palette_names_file = true;
  }
}

// LES TROIS NUMÉROS QUE LA RECETTE IMPOSE, posés sur l'apparence — la même
// règle pour la recette du fil et pour celle du calcul. Écraser par la
// sentinelle rendrait chauve de couleur un joueur dont la recette ne parle que
// de son costume.
//
// 🔴 LA COIFFURE EST LE NUMÉRO ANNONCÉ, pas un numéro de fichier. C'est
// `sd->status.hair` — ce que le styliste change et que le protocole natif
// diffuse —, et `ActorLook::hair` porte exactement la même chose : la
// permutation des premières coupes se fait une seule fois, quand le catalogue
// des sprites cherche le fichier — voir `ro/render/sprite_catalog.h`.
template <typename Recipe>
void ImposeNumbers(const Recipe& recipe, ro::render::ActorLook& look) {
  ImposeBodyPaletteOf(recipe, look);
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
  for (std::size_t rank = 0; rank < ro::render::kMaxRamps; ++rank) {
    recipe.ramps[rank] = ToAdjust(told.ramps[rank]);
  }
  recipe.body_palette = told.body_palette;
  recipe.hair_palette = told.hair_palette;
  recipe.hair_style = told.hair_style;
  return recipe;
}

ro::sim::StyleRecipe ToWire(const ro::render::PaletteRecipe& recipe) {
  ro::sim::StyleRecipe told;
  for (std::size_t rank = 0; rank < ro::sim::kStyleRampCount; ++rank) {
    told.ramps[rank] = ToWireAdjust(recipe.ramps[rank]);
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

void ImposeBodyPalette(const ro::sim::StyleRecipe& recipe, ro::render::ActorLook& look) {
  ImposeBodyPaletteOf(recipe, look);
}

std::optional<ro::render::LayerDye> FromWire(const ro::sim::StyleLayerRecipe& told) {
  ro::render::LayerDye dye;
  switch (told.layer) {
    case ro::sim::StyleLayerKind::kHead:
      dye.layer = ro::render::StyleLayer::kHead;
      break;
    case ro::sim::StyleLayerKind::kHeadgear:
      dye.layer = ro::render::StyleLayer::kHeadgear;
      break;
    case ro::sim::StyleLayerKind::kGarment:
      dye.layer = ro::render::StyleLayer::kGarment;
      break;
    case ro::sim::StyleLayerKind::kSkin:
      dye.layer = ro::render::StyleLayer::kSkin;
      break;
    case ro::sim::StyleLayerKind::kBodyMore:
      dye.layer = ro::render::StyleLayer::kBodyMore;
      break;
    case ro::sim::StyleLayerKind::kBodySkin:
      dye.layer = ro::render::StyleLayer::kBodySkin;
      break;
    default:
      return std::nullopt;
  }
  dye.key = told.key;
  dye.profile = told.profile;
  for (std::size_t rank = 0; rank < ro::render::kLayerAdjustCount; ++rank) {
    dye.adjusts[rank] = ToAdjust(told.adjusts[rank]);
    dye.rest[rank] = ToAdjust(told.rest[rank]);
  }
  dye.retouches.reserve(told.retouches.size());
  for (const ro::sim::StyleRetouch& retouch : told.retouches) {
    dye.retouches.push_back(ro::render::PaletteRetouch{
        .index = retouch.index, .rgb = {retouch.red, retouch.green, retouch.blue}});
  }
  return dye;
}

ro::sim::StyleLayerRecipe ToWire(const ro::render::LayerDye& dye) {
  ro::sim::StyleLayerRecipe told;
  // ⚠ LES MÊMES NUMÉROS DES DEUX CÔTÉS : le corps, qui n'est pas un calque,
  // n'a pas de teinture à traduire et ne passe jamais par ici.
  told.layer = static_cast<ro::sim::StyleLayerKind>(dye.layer);
  told.key = dye.key;
  told.profile = dye.profile;
  for (std::size_t rank = 0; rank < ro::sim::kStyleLayerAdjustCount; ++rank) {
    told.adjusts[rank] = ToWireAdjust(dye.adjusts[rank]);
    told.rest[rank] = ToWireAdjust(dye.rest[rank]);
  }
  told.retouches.reserve(dye.retouches.size());
  for (const ro::render::PaletteRetouch& retouch : dye.retouches) {
    told.retouches.push_back(ro::sim::StyleRetouch{.index = retouch.index,
                                                   .red = retouch.rgb[0],
                                                   .green = retouch.rgb[1],
                                                   .blue = retouch.rgb[2]});
  }
  return told;
}

std::vector<ro::render::LayerDye> WornDyes(std::span<const ro::sim::StyleLayerRecipe> told,
                                           const ro::render::ActorLook& look) {
  const auto worn = [&look](const ro::render::LayerDye& dye) {
    switch (dye.layer) {
      case ro::render::StyleLayer::kHead:
        return dye.key == ro::render::HeadDyeKey(look.hair, look.hair_palette);
      case ro::render::StyleLayer::kHeadgear:
        return dye.key != 0 && std::ranges::find(look.headgears, dye.key) != look.headgears.end();
      case ro::render::StyleLayer::kGarment:
        return dye.key != 0 && dye.key == look.robe;
      case ro::render::StyleLayer::kSkin:
        return dye.key == 0;
      // ⚠ TOUTES LES SUITES, une par corps habillé au plus : l'apparence ne dit
      // pas quel corps se charge, et la garde-robe choisit la suite par la clé
      // du corps qu'elle peint. De même pour la peau de chaque corps.
      case ro::render::StyleLayer::kBodyMore:
      case ro::render::StyleLayer::kBodySkin:
        return dye.key != 0;
      case ro::render::StyleLayer::kBody:
        break;
    }
    return false;
  };
  std::vector<ro::render::LayerDye> dyes;
  for (const ro::sim::StyleLayerRecipe& each : told) {
    const std::optional<ro::render::LayerDye> dye = FromWire(each);
    if (!dye.has_value() || !worn(*dye)) {
      continue;
    }
    // ⚠ LA PREMIÈRE D'UN COUPLE (CALQUE, CLÉ) L'EMPORTE, comme dans l'ensemble.
    const bool known = std::ranges::any_of(dyes, [&dye](const ro::render::LayerDye& kept) {
      return kept.layer == dye->layer && kept.key == dye->key;
    });
    if (!known) {
      dyes.push_back(*dye);
    }
  }
  std::ranges::sort(dyes, [](const ro::render::LayerDye& left, const ro::render::LayerDye& right) {
    return std::tie(left.layer, left.key) < std::tie(right.layer, right.key);
  });
  return dyes;
}

bool SameDye(const ro::render::LayerDye& left, const ro::render::LayerDye& right) {
  if (left.layer != right.layer || left.key != right.key || left.profile != right.profile) {
    return false;
  }
  for (std::size_t rank = 0; rank < ro::render::kLayerAdjustCount; ++rank) {
    if (!SameAdjust(left.adjusts[rank], right.adjusts[rank]) ||
        !SameAdjust(left.rest[rank], right.rest[rank])) {
      return false;
    }
  }
  return left.retouches == right.retouches;
}

bool SameSet(const ro::render::StyleSet& left, const ro::render::StyleSet& right) {
  if (left.body_palette.has_value() || right.body_palette.has_value()) {
    return false;
  }
  if (left.body.has_value() != right.body.has_value() ||
      (left.body.has_value() && !SameRamps(*left.body, *right.body))) {
    return false;
  }
  return std::ranges::equal(left.layers, right.layers, SameDye);
}

bool SameRecipe(const ro::render::PaletteRecipe& left, const ro::render::PaletteRecipe& right) {
  return left.body_palette == right.body_palette && left.hair_palette == right.hair_palette &&
         left.hair_style == right.hair_style && SameRamps(left, right);
}

bool SameRamps(const ro::render::PaletteRecipe& left, const ro::render::PaletteRecipe& right) {
  return std::ranges::equal(left.ramps, right.ramps, SameAdjust);
}

}  // namespace moonclient

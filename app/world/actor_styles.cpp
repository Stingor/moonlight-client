#include "world/actor_styles.h"

#include <vector>

#include "character/style_recipe.h"

namespace moonclient {
namespace {

// La recette de `sim`, traduite pour le calcul de `render`.
//
// ⚠ DEUX TYPES POUR LA MÊME CHOSE, ET C'EST LE GRAPHE QUI L'IMPOSE : `render`
// ne dépend pas de `sim`, et `net` ne dépend pas de `render`. La même
// frontière que `sim::Actor` face à `render::ActorLook`.
[[nodiscard]] ro::render::PaletteRecipe Translate(const ro::sim::StyleRecipe& told) {
  ro::render::PaletteRecipe recipe;
  for (std::size_t rank = 0; rank < ro::sim::kStyleRampCount && rank < ro::render::kMaxRamps;
       ++rank) {
    const ro::sim::StyleAdjust& adjust = told.ramps[rank];
    recipe.ramps[rank] = ro::render::RampAdjust{.hue = adjust.hue,
                                                .saturation = adjust.saturation,
                                                .value = adjust.value,
                                                .absolute = adjust.absolute};
  }
  return recipe;
}

[[nodiscard]] bool Same(const ro::sim::StyleAdjust& left, const ro::sim::StyleAdjust& right) {
  return left.hue == right.hue && left.saturation == right.saturation &&
         left.value == right.value && left.absolute == right.absolute;
}

// ⚠ CHAMP PAR CHAMP : comparer des champs ne collisionne jamais.
[[nodiscard]] bool Same(const ro::sim::StyleRecipe& left, const ro::sim::StyleRecipe& right) {
  if (left.body_palette != right.body_palette || left.hair_palette != right.hair_palette ||
      left.hair_style != right.hair_style) {
    return false;
  }
  for (std::size_t rank = 0; rank < ro::sim::kStyleRampCount; ++rank) {
    if (!Same(left.ramps[rank], right.ramps[rank])) {
      return false;
    }
  }
  return true;
}

}  // namespace

void ImposeStyleNumbers(std::uint32_t gid, ro::render::ActorLook& look,
                        ro::render::ActorWardrobe& wardrobe, const ro::sim::StyleBook& book) {
  const ro::sim::StyleVariants* variants = book.Find(gid);
  if (variants == nullptr) {
    return;
  }
  const std::uint32_t body_key = wardrobe.BodyKeyFor(look);
  if (const ro::sim::StyleRecipe* wanted = body_key != 0 ? variants->For(body_key) : nullptr) {
    Impose(*wanted, look);
  }
}

void ActorStyles::Dress(std::uint32_t gid, ro::render::ActorLook& look,
                        ro::render::ActorWardrobe& wardrobe, const ro::sim::StyleBook& book) {
  look.style = 0;
  const ro::sim::StyleVariants* variants = book.Find(gid);
  const auto known = applied_.find(gid);
  // ⚠ LA CLÉ NE SE DEMANDE QUE POUR QUI A DES COULEURS : elle charge le sprite
  // du corps pour savoir lequel se charge vraiment, et le faire pour tous les
  // acteurs d'une carte peuplée paierait ce chargement sans rien en tirer.
  //
  // ⚠ ET SUR L'APPARENCE NUE, avant d'imposer quoi que ce soit : c'est le
  // corps qui décide de la variante, et la variante ne doit pas décider du
  // corps.
  const std::uint32_t body_key = variants != nullptr ? wardrobe.BodyKeyFor(look) : 0;
  const ro::sim::StyleRecipe* wanted =
      variants != nullptr && body_key != 0 ? variants->For(body_key) : nullptr;

  if (wanted == nullptr) {
    // Plus de couleurs : on rend l'acteur à son apparence native, et on rend
    // aussi son identifiant.
    if (known != applied_.end()) {
      wardrobe.ForgetStyle(known->second.style);
      applied_.erase(known);
    }
    return;
  }
  // 🔴 LES TROIS NUMÉROS D'ABORD, ET QUOI QU'IL ARRIVE ENSUITE. Ils tiennent
  // debout sans l'identifiant — ce sont des fichiers officiels, que le jeu
  // sait poser seul —, et un refus de la garde-robe plus bas ne doit pas
  // emporter la coiffure avec les réglages.
  Impose(*wanted, look);

  if (known != applied_.end() && known->second.body_key == body_key &&
      Same(known->second.recipe, *wanted)) {
    look.style = known->second.style;
    return;
  }

  // 🔴 UN IDENTIFIANT NEUF, JAMAIS UNE RÉÉCRITURE : voir l'en-tête.
  if (known != applied_.end()) {
    wardrobe.ForgetStyle(known->second.style);
  }
  const std::uint32_t style = wardrobe.NewStyleId();
  if (!wardrobe.SetStyle(style, Translate(*wanted))) {
    // La garde-robe a refusé : on ne prétend pas qu'un style existe. L'acteur
    // garde son apparence native plutôt que d'en porter une qu'on ne sait pas
    // nommer.
    applied_.erase(gid);
    return;
  }
  applied_[gid] = Applied{.style = style, .body_key = body_key, .recipe = *wanted};
  look.style = style;
}

void ActorStyles::Sweep(ro::render::ActorWardrobe& wardrobe, const ro::sim::StyleBook& book) {
  std::vector<std::uint32_t> gone;
  for (const auto& [gid, applied] : applied_) {
    if (book.Find(gid) == nullptr) {
      gone.push_back(gid);
    }
  }
  for (const std::uint32_t gid : gone) {
    wardrobe.ForgetStyle(applied_[gid].style);
    applied_.erase(gid);
  }
}

}  // namespace moonclient

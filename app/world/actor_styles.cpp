#include "world/actor_styles.h"

#include <vector>

#include "character/style_recipe.h"

namespace moonclient {

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
  const std::vector<ro::sim::StyleLayerRecipe>* layers = book.FindLayers(gid);
  const auto known = applied_.find(gid);

  ro::render::StyleSet set;
  if (wanted != nullptr) {
    // 🔴 LA TEINTE D'ABORD, ET QUOI QU'IL ARRIVE ENSUITE. Elle tient debout sans
    // l'identifiant — c'est un fichier officiel, que le jeu sait poser seul —,
    // et un refus de la garde-robe plus bas ne doit pas l'emporter avec les
    // réglages.
    ImposeBodyPalette(*wanted, look);
    set.body = FromWire(*wanted);
  }
  if (layers != nullptr) {
    set.layers = WornDyes(*layers, look);
  }
  if (!set.body.has_value() && set.layers.empty()) {
    // Plus de couleurs : l'acteur reprend son apparence native, et lâche son
    // identifiant.
    if (known != applied_.end()) {
      Release(known->second);
      applied_.erase(known);
    }
    return;
  }
  if (known != applied_.end()) {
    if (const auto worn = interned_.find(known->second);
        worn != interned_.end() && SameSet(worn->second.set, set)) {
      look.style = known->second;
      return;
    }
    Release(known->second);
    applied_.erase(known);
  }
  const std::uint32_t style = Intern(set, wardrobe);
  if (style == 0) {
    // La garde-robe a refusé : on ne prétend pas qu'un style existe. L'acteur
    // garde son apparence native plutôt que d'en porter une qu'on ne sait pas
    // nommer.
    return;
  }
  applied_[gid] = style;
  look.style = style;
}

std::uint32_t ActorStyles::Intern(const ro::render::StyleSet& set,
                                  ro::render::ActorWardrobe& wardrobe) {
  for (auto& [style, interned] : interned_) {
    if (SameSet(interned.set, set)) {
      ++interned.bearers;
      return style;
    }
  }
  // 🔴 UN IDENTIFIANT NEUF, JAMAIS UNE RÉÉCRITURE : voir l'en-tête.
  const std::uint32_t style = wardrobe.NewStyleId();
  if (!wardrobe.SetStyleSet(style, set)) {
    return 0;
  }
  interned_.emplace(style, Interned{.set = set, .bearers = 1});
  return style;
}

void ActorStyles::Release(std::uint32_t style) {
  if (const auto found = interned_.find(style);
      found != interned_.end() && found->second.bearers > 0) {
    --found->second.bearers;
  }
}

std::uint32_t ActorStyles::Worn(std::uint32_t gid) const {
  const auto found = applied_.find(gid);
  return found != applied_.end() ? found->second : 0;
}

void ActorStyles::Sweep(ro::render::ActorWardrobe& wardrobe, const ro::sim::StyleBook& book) {
  for (auto applied = applied_.begin(); applied != applied_.end();) {
    if (book.Find(applied->first) != nullptr || book.FindLayers(applied->first) != nullptr) {
      ++applied;
      continue;
    }
    Release(applied->second);
    applied = applied_.erase(applied);
  }
  for (auto interned = interned_.begin(); interned != interned_.end();) {
    if (interned->second.bearers != 0) {
      ++interned;
      continue;
    }
    wardrobe.ForgetStyle(interned->first);
    interned = interned_.erase(interned);
  }
}

}  // namespace moonclient

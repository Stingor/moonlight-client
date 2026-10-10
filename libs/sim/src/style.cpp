#include "ro/sim/style.h"

namespace ro::sim {

const StyleRecipe* StyleVariants::For(std::uint32_t body_key) const {
  if (const auto found = by_body.find(body_key); found != by_body.end()) {
    return &found->second;
  }
  // Le repli : la variante que le joueur a marquée comme la sienne par défaut.
  // ⚠ SANS LUI, MONTER SUR UNE MONTURE REND L'APPARENCE NATIVE — un corps
  // jamais teint n'a pas de variante, et le joueur se verrait redevenir gris
  // le temps d'un trajet.
  if (fallback_body == 0) {
    return nullptr;
  }
  const auto found = by_body.find(fallback_body);
  return found != by_body.end() ? &found->second : nullptr;
}

const StyleVariants* StyleBook::Find(std::uint32_t gid) const {
  const auto found = styles_.find(gid);
  return found != styles_.end() ? &found->second : nullptr;
}

std::uint32_t StyleBook::RevisionOf(std::uint32_t gid) const {
  const auto found = revisions_.find(gid);
  return found != revisions_.end() ? found->second : 0;
}

void StyleBook::BeginBatchFor(std::uint32_t gid) {
  ++revisions_[gid];
  StyleVariants& variants = styles_[gid];
  variants.by_body.clear();
  variants.fallback_body = 0;
}

void StyleBook::Remember(std::uint32_t gid, std::uint32_t body_key, const StyleRecipe& recipe,
                         bool is_fallback) {
  // ⚠ LA CLÉ NULLE NE DÉSIGNE AUCUN CORPS : c'est la sentinelle « corps
  // inconnu » de `BodySpriteKey`, et la ranger donnerait une variante que
  // personne ne pourrait retrouver.
  if (body_key == 0) {
    return;
  }
  ++revisions_[gid];
  StyleVariants& variants = styles_[gid];
  variants.by_body[body_key] = recipe;
  // La PREMIÈRE variante sert de repli tant qu'aucune n'est marquée : un
  // joueur qui n'a teint qu'un corps doit garder ses couleurs sur les autres.
  if (is_fallback || variants.fallback_body == 0) {
    variants.fallback_body = body_key;
  }
}

void StyleBook::Forget(std::uint32_t gid) {
  ++revisions_[gid];
  styles_.erase(gid);
}

}  // namespace ro::sim

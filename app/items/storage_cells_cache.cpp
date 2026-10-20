// La porte des cases du coffre. Voir l'en-tête.

#include "items/storage_cells_cache.h"

namespace moonclient {

bool StorageCellsCache::NeedsRebuild(const StorageCellsInputs& now) {
  static const std::set<std::uint32_t> kNoFavorites;
  const std::set<std::uint32_t>& favorites =
      now.favorites == nullptr ? kNoFavorites : *now.favorites;
  // ⚠ LES FAVORIS SE COMPARENT PAR CONTENU, pas par adresse ni par compte : la
  // liste se recharge d'un fichier, et un objet neuf garderait l'adresse d'un
  // ancien comme un compte repartirait de zéro.
  const bool same = built_ && now.storage_revision == storage_revision_ && now.tab == tab_ &&
                    now.filter == filter_ && now.sub_category == sub_category_ &&
                    now.facts_generation == facts_generation_ &&
                    now.icons_generation == icons_generation_ &&
                    now.creators_revision == creators_revision_ &&
                    now.language_serial == language_serial_ && favorites == favorites_;
  if (same) {
    return false;
  }
  built_ = true;
  storage_revision_ = now.storage_revision;
  tab_ = now.tab;
  filter_.assign(now.filter);
  sub_category_ = now.sub_category;
  favorites_ = favorites;
  facts_generation_ = now.facts_generation;
  icons_generation_ = now.icons_generation;
  creators_revision_ = now.creators_revision;
  language_serial_ = now.language_serial;
  return true;
}

}  // namespace moonclient

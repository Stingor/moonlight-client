// LA PORTE DES CASES DU COFFRE.
//
// 🔴 CE BANC TIENT LES DEUX PANNES D'UN CACHE, qui ne se voient pas de la même
// façon : refaire à chaque image ne se voit qu'au chronomètre, sur un coffre de
// milliers d'objets ; NE PAS refaire quand il le faut se voit comme une case
// figée — un prix qui n'arrive pas, une étoile qui ne s'allume pas, un filtre
// sans effet. Chaque entrée de la porte est donc éprouvée une à une.

#include "items/storage_cells_cache.h"

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using moonclient::StorageCellsCache;
using moonclient::StorageCellsInputs;

namespace {

[[nodiscard]] StorageCellsInputs Steady(const std::set<std::uint32_t>* favorites,
                                        std::string_view filter) {
  return StorageCellsInputs{
      .storage_revision = 7,
      .tab = ro::ui::ItemTab::kAll,
      .filter = filter,
      .sub_category = -1,
      .favorites = favorites,
      .facts_generation = 3,
      .icons_generation = 1,
      .creators_revision = 2,
      .language_serial = 1,
  };
}

}  // namespace

TEST_CASE("Cases du coffre : refaites une fois, puis gardées", "[app][coffre]") {
  const std::set<std::uint32_t> favorites = {501};
  StorageCellsCache cache;
  CHECK(cache.NeedsRebuild(Steady(&favorites, "pot")));
  // Rien n'a bougé : rien à refaire, image après image.
  CHECK_FALSE(cache.NeedsRebuild(Steady(&favorites, "pot")));
  CHECK_FALSE(cache.NeedsRebuild(Steady(&favorites, "pot")));
}

TEST_CASE("Cases du coffre : chaque entrée qui bouge les fait refaire", "[app][coffre]") {
  const std::set<std::uint32_t> favorites = {501};
  // ⚠ UNE ENTRÉE À LA FOIS, et la porte se reprend sur la valeur stable entre
  // deux : une entrée oubliée par la comparaison ne peut pas se cacher derrière
  // une autre.
  const std::vector<std::function<void(StorageCellsInputs&)>> changes = {
      [](StorageCellsInputs& in) { ++in.storage_revision; },
      [](StorageCellsInputs& in) { in.tab = ro::ui::ItemTab::kCard; },
      [](StorageCellsInputs& in) { in.sub_category = 4; },
      [](StorageCellsInputs& in) { ++in.facts_generation; },
      [](StorageCellsInputs& in) { ++in.icons_generation; },
      [](StorageCellsInputs& in) { ++in.creators_revision; },
      [](StorageCellsInputs& in) { ++in.language_serial; },
  };
  StorageCellsCache cache;
  REQUIRE(cache.NeedsRebuild(Steady(&favorites, "pot")));
  for (std::size_t rank = 0; rank < changes.size(); ++rank) {
    CAPTURE(rank);
    StorageCellsInputs changed = Steady(&favorites, "pot");
    changes[rank](changed);
    CHECK(cache.NeedsRebuild(changed));
    CHECK_FALSE(cache.NeedsRebuild(changed));
    CHECK(cache.NeedsRebuild(Steady(&favorites, "pot")));
  }
}

TEST_CASE("Cases du coffre : la recherche se compare par son texte", "[app][coffre]") {
  // ⚠ LE TEXTE ET NON SON ADRESSE : le champ de saisie garde le même tampon
  // d'une frappe à l'autre, et c'est son contenu qui change.
  const std::set<std::uint32_t> favorites;
  std::string typed = "po";
  StorageCellsCache cache;
  REQUIRE(cache.NeedsRebuild(Steady(&favorites, typed)));
  typed += "t";
  CHECK(cache.NeedsRebuild(Steady(&favorites, typed)));
  CHECK_FALSE(cache.NeedsRebuild(Steady(&favorites, typed)));
}

TEST_CASE("Cases du coffre : une étoile posée les fait refaire", "[app][coffre]") {
  // ⚠ LES FAVORIS SE COMPARENT PAR CONTENU : la liste garde son adresse quand le
  // joueur bascule une étoile, et l'onglet des favoris doit pourtant changer.
  std::set<std::uint32_t> favorites = {501};
  StorageCellsCache cache;
  REQUIRE(cache.NeedsRebuild(Steady(&favorites, "")));
  favorites.insert(607);
  CHECK(cache.NeedsRebuild(Steady(&favorites, "")));
  CHECK_FALSE(cache.NeedsRebuild(Steady(&favorites, "")));

  // Une liste absente vaut une liste vide, et passer de l'une à l'autre ne
  // change rien à ce qui s'affiche.
  favorites.clear();
  REQUIRE(cache.NeedsRebuild(Steady(&favorites, "")));
  CHECK_FALSE(cache.NeedsRebuild(Steady(nullptr, "")));
}

TEST_CASE("Cases du coffre : oublier fait refaire", "[app][coffre]") {
  // C'est ce qui sert quand on confie d'autres forgerons à la vue : la poignée
  // change sans qu'aucun compte ne bouge.
  const std::set<std::uint32_t> favorites;
  StorageCellsCache cache;
  REQUIRE(cache.NeedsRebuild(Steady(&favorites, "")));
  cache.Forget();
  CHECK(cache.NeedsRebuild(Steady(&favorites, "")));
  CHECK_FALSE(cache.NeedsRebuild(Steady(&favorites, "")));
}

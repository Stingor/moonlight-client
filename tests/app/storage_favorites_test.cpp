// LES FAVORIS DU COFFRE, QUI SONT À NOUS.
//
// 🔴 CE BANC TIENT CE QUE LE SERVEUR NE TIENT PAS. Un favori de SAC est un
// drapeau que le serveur retient ; un favori de COFFRE n'existe que chez nous,
// dans un fichier du joueur. Ce qui se vérifie ici, c'est donc la bascule et sa
// clé — l'IDENTIFIANT de l'objet, pas sa place : une pile sortie puis remise
// retrouve son étoile, et deux piles du même objet la portent ensemble.

#include "items/storage_favorites.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using moonclient::StorageFavorites;

TEST_CASE("Favoris du coffre : la bascule va dans les deux sens", "[app][coffre]") {
  StorageFavorites favorites;
  REQUIRE(favorites.empty());

  CHECK(favorites.Toggle(501));
  CHECK(favorites.Has(501));
  CHECK_FALSE(favorites.empty());

  CHECK_FALSE(favorites.Toggle(501));
  CHECK_FALSE(favorites.Has(501));
  CHECK(favorites.empty());
}

TEST_CASE("Favoris du coffre : zéro n'est pas un objet", "[app][coffre]") {
  // ⚠ IL ARRIVE PAR UNE CASE VIDE ou une ligne qu'on vient de vider : le marquer
  // mettrait une étoile sur « rien », qu'aucun geste ne pourrait plus retirer.
  StorageFavorites favorites;
  CHECK_FALSE(favorites.Toggle(0));
  CHECK(favorites.empty());
  favorites.Add(0);
  CHECK(favorites.empty());
}

TEST_CASE("Favoris du coffre : la liste reste dans l'ordre des identifiants", "[app][coffre]") {
  // ⚠ CE N'EST PAS DE L'ESTHÉTIQUE : le fichier se relit et se corrige à la
  // main, et une liste qui se réordonne à chaque écriture rend le diff
  // illisible — c'est ce que le `.gitattributes` du dépôt dénonce ailleurs.
  StorageFavorites favorites;
  favorites.Add(909);
  favorites.Add(501);
  favorites.Add(1201);
  std::vector<std::uint32_t> seen;
  for (const std::uint32_t item_id : favorites.ids()) {
    seen.push_back(item_id);
  }
  CHECK(seen == std::vector<std::uint32_t>{501, 909, 1201});
}

TEST_CASE("Favoris du coffre : marquer deux fois ne double pas l'entrée", "[app][coffre]") {
  StorageFavorites favorites;
  favorites.Add(501);
  favorites.Add(501);
  CHECK(favorites.ids().size() == 1);
}

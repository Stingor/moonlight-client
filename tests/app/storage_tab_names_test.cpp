// LE NOM ET L'ICÔNE QUE LE JOUEUR DONNE À SES COFFRES.
//
// 🔴 CE QUE CE BANC TIENT : les deux réglages d'un même coffre ne se marchent
// pas dessus — renommer ne perd pas l'icône, décorer ne perd pas le nom —, et
// un coffre qui n'a plus rien à dire SORT de la table plutôt que d'y laisser une
// entrée vide. Sans cela, le fichier grossirait d'un coffre à chaque essai de
// renommage abandonné.

#include "items/storage_tab_names.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

using moonclient::StorageTabName;
using moonclient::StorageTabNames;

TEST_CASE("Onglets du coffre : le nom et l'icône se posent chacun de leur côté", "[app][coffre]") {
  StorageTabNames names;
  names.Name(3, "Consommables");
  REQUIRE(names.Of(3) != nullptr);
  CHECK(names.Of(3)->name == "Consommables");
  CHECK(names.Of(3)->icon_item_id == 0);

  // ⚠ DÉCORER NE PERD PAS LE NOM : ce sont deux gestes, et le joueur en fait un
  // puis l'autre.
  names.Decorate(3, 501);
  CHECK(names.Of(3)->name == "Consommables");
  CHECK(names.Of(3)->icon_item_id == 501);

  // ⚠ ET RENOMMER NE PERD PAS L'ICÔNE.
  names.Name(3, "Potions");
  CHECK(names.Of(3)->name == "Potions");
  CHECK(names.Of(3)->icon_item_id == 501);
}

TEST_CASE("Onglets du coffre : un nom vide rend celui du serveur", "[app][coffre]") {
  // 🔴 C'EST AINSI QU'ON ANNULE, et c'est pour cela qu'il n'y a pas de bouton
  // « effacer » : on vide le champ. Mais l'icône, elle, reste — elle n'a rien
  // demandé.
  StorageTabNames names;
  names.Name(1, "Cartes");
  names.Decorate(1, 4001);
  names.Name(1, "");
  REQUIRE(names.Of(1) != nullptr);
  CHECK(names.Of(1)->name.empty());
  CHECK(names.Of(1)->icon_item_id == 4001);
}

TEST_CASE("Onglets du coffre : ce qui ne dit plus rien sort de la table", "[app][coffre]") {
  // ⚠ SANS CELA le fichier garderait une entrée par essai de renommage
  // abandonné, et il se relit à la main.
  StorageTabNames names;
  names.Name(2, "Forge");
  names.Decorate(2, 999);
  REQUIRE_FALSE(names.empty());

  names.Name(2, "");
  CHECK_FALSE(names.empty());  // l'icône parle encore
  names.Decorate(2, 0);
  CHECK(names.empty());
  CHECK(names.Of(2) == nullptr);
}

TEST_CASE("Onglets du coffre : un nom démesuré se borne", "[app][coffre]") {
  // ⚠ IL PEUT VENIR D'UN FICHIER CORRIGÉ À LA MAIN, pas seulement du champ de
  // saisie : un nom sans fin chasserait les boutons voisins hors de la fenêtre.
  StorageTabNames names;
  names.Name(0, std::string(200, 'x'));
  REQUIRE(names.Of(0) != nullptr);
  CHECK(names.Of(0)->name.size() <= 23);

  names.Put(4, StorageTabName{.name = std::string(200, 'y'), .icon_item_id = 7});
  REQUIRE(names.Of(4) != nullptr);
  CHECK(names.Of(4)->name.size() <= 23);
  CHECK(names.Of(4)->icon_item_id == 7);
}

TEST_CASE("Onglets du coffre : la table reste dans l'ordre des numéros", "[app][coffre]") {
  // Le fichier se relit et se corrige à la main : un ordre qui change à chaque
  // écriture rendrait le diff illisible.
  StorageTabNames names;
  names.Name(7, "Sept");
  names.Name(1, "Un");
  names.Name(4, "Quatre");
  std::vector<int> seen;
  for (const auto& [storage_id, told] : names.all()) {
    seen.push_back(static_cast<int>(storage_id));
  }
  CHECK(seen == std::vector<int>{1, 4, 7});
}

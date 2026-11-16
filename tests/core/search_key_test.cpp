// La clé de recherche : la casse et les accents tombent, les ligatures
// s'éclatent, le reste garde son seul repli de casse.

#include "ro/core/search_key.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::core::CompareSearchKeys;
using ro::core::SearchKey;
using ro::core::SearchKeyContains;

TEST_CASE("Clé de recherche : casse et accents latins", "[core][search_key]") {
  CHECK(SearchKey("États") == "etats");
  CHECK(SearchKey("ÉTATS") == SearchKey("etats"));
  CHECK(SearchKey("Ça déçoit") == "ca decoit");
  CHECK(SearchKey("Ñandú") == "nandu");
  CHECK(SearchKey("Łódź") == "lodz");
  CHECK(SearchKey("Ÿ ÿ") == "y y");
  CHECK(SearchKey("Øre") == "ore");
}

TEST_CASE("Clé de recherche : les ligatures s'éclatent", "[core][search_key]") {
  CHECK(SearchKey("l'Œuf") == "l'oeuf");
  CHECK(SearchKey("Æther") == "aether");
  CHECK(SearchKey("Straße") == "strasse");
  CHECK(SearchKey("Ĳssel") == "ijssel");
}

TEST_CASE("Clé de recherche : ce qui ne bouge pas", "[core][search_key]") {
  // Un « e » suivi d'un accent aigu combinant perd l'accent.
  CHECK(SearchKey("e\xCC\x81t\xC3\xA9") == "ete");
  CHECK(SearchKey("× ÷ þ") == "× ÷ þ");
  CHECK(SearchKey("Ж") == "ж");
  CHECK(SearchKey("가나") == "가나");
  // Un octet invalide ressort tel qu'il est entré.
  CHECK(SearchKey("a\xFF"
                  "b") == std::string("a\xFF"
                                      "b"));
}

TEST_CASE("Clé de recherche : l'ordre du dictionnaire", "[core][search_key]") {
  CHECK(SearchKey("États") < SearchKey("Fabrication"));
  CHECK(SearchKey("Écran de veille") < SearchKey("Entités"));
}

TEST_CASE("Clé de recherche : une clé dans une autre", "[core][search_key]") {
  CHECK(SearchKeyContains(SearchKey("Fiche de pet"), SearchKey("PET")));
  CHECK(SearchKeyContains(SearchKey("Échelle de l'interface"), SearchKey("echelle")));
  CHECK(SearchKeyContains(SearchKey("Bulles"), ""));
  CHECK_FALSE(SearchKeyContains(SearchKey("Bulles"), SearchKey("son")));
}

TEST_CASE("Clé de recherche : les nombres se comparent comme des nombres", "[core][search_key]") {
  CHECK(CompareSearchKeys(SearchKey("Barre 1, case 2"), SearchKey("Barre 1, case 10")) < 0);
  CHECK(CompareSearchKeys(SearchKey("Barre 10, case 1"), SearchKey("Barre 9, case 12")) > 0);
  CHECK(CompareSearchKeys(SearchKey("Écran de veille"), SearchKey("Entités")) < 0);
  CHECK(CompareSearchKeys(SearchKey("F2"), SearchKey("f2")) == 0);
  CHECK(CompareSearchKeys("case", "case 1") < 0);
  // À nombre égal, les zéros de tête départagent sans brouiller l'ordre.
  CHECK(CompareSearchKeys("a07", "a7") != 0);
  CHECK(CompareSearchKeys("a07", "a8") < 0);
}

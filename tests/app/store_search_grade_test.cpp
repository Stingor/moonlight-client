// L'EMBLÈME DU GRADE D'UN RÉSULTAT de la recherche d'échoppes : un grade
// positif nomme son image, numérotée par lui ; sans grade, aucune.

#include "game/store_search_grade.h"

#include <catch2/catch_test_macros.hpp>

using moonclient::StoreSearchGradeBadgePath;

TEST_CASE("Recherche d'échoppes : sans grade, aucun emblème", "[app][store_search]") {
  CHECK(StoreSearchGradeBadgePath(0).empty());
}

TEST_CASE("Recherche d'échoppes : un grade positif nomme son emblème", "[app][store_search]") {
  CHECK(StoreSearchGradeBadgePath(1) == "grade_enchant\\grade_icon1.bmp");
  CHECK(StoreSearchGradeBadgePath(4) == "grade_enchant\\grade_icon4.bmp");
}

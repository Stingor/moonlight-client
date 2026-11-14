// Le repli de casse simple : chaque écriture que le jeu montre, puis ce qui ne
// doit PAS bouger — un mot déjà replié, le « ß », un octet invalide.

#include "ro/core/case_fold.h"

#include <cstddef>
#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::core::CaseFolded;
using ro::core::CaseFoldRuns;
using ro::core::EqualsFolded;
using ro::core::FoldedCodePoint;

TEST_CASE("Repli de casse : un point de code par écriture", "[core][case_fold]") {
  CHECK(FoldedCodePoint(U'A') == U'a');
  CHECK(FoldedCodePoint(U'z') == U'z');
  CHECK(FoldedCodePoint(U'É') == U'é');    // É → é
  CHECK(FoldedCodePoint(U'Þ') == U'þ');    // Þ → þ
  CHECK(FoldedCodePoint(U'×') == U'×');    // ×, entre deux plages, ne bouge pas
  CHECK(FoldedCodePoint(U'Ÿ') == U'ÿ');    // Ÿ → ÿ
  CHECK(FoldedCodePoint(U'Ł') == U'ł');    // Ł → ł
  CHECK(FoldedCodePoint(U'ł') == U'ł');    // ł, rang impair d'une plage alternée
  CHECK(FoldedCodePoint(U'Ǆ') == U'ǆ');    // Ǆ → ǆ
  CHECK(FoldedCodePoint(U'ǅ') == U'ǆ');    // ǅ → ǆ
  CHECK(FoldedCodePoint(U'Δ') == U'δ');    // Δ → δ
  CHECK(FoldedCodePoint(U'ς') == U'σ');    // ς → σ
  CHECK(FoldedCodePoint(U'Ж') == U'ж');    // Ж → ж
  CHECK(FoldedCodePoint(U'Ё') == U'ё');    // Ё → ё
  CHECK(FoldedCodePoint(U'Ạ') == U'ạ');    // Ạ → ạ
  CHECK(FoldedCodePoint(U'ẞ') == U'ß');    // ẞ → ß
  CHECK(FoldedCodePoint(U'ß') == U'ß');    // ß reste ß
  CHECK(FoldedCodePoint(U'İ') == U'İ');    // İ n'a pas de repli simple
  CHECK(FoldedCodePoint(U'가') == U'가');  // le coréen n'a pas de casse
}

TEST_CASE("Repli de casse : les plages sont triées et disjointes", "[core][case_fold]") {
  const auto runs = CaseFoldRuns();
  REQUIRE_FALSE(runs.empty());
  for (std::size_t index = 0; index < runs.size(); ++index) {
    CHECK(runs[index].first <= runs[index].last);
    CHECK(runs[index].stride >= 1);
    if (index > 0) {
      CHECK(runs[index - 1].last < runs[index].first);
    }
  }
}

TEST_CASE("Repli de casse : un texte UTF-8", "[core][case_fold]") {
  CHECK(CaseFolded("\xC3\x89LODIE") == "\xC3\xA9lodie");
  CHECK(EqualsFolded("\xC3\x89LODIE", "\xC3\x89lodie"));
  CHECK(EqualsFolded("\xD0\x9D\xD0\x90\xD0\xA2\xD0\x90\xD0\xA8\xD0\x90",
                     "\xD0\xBD\xD0\xB0\xD1\x82\xD0\xB0\xD1\x88\xD0\xB0"));  // НАТАША, наташа
  // ΟΔΥΣΣΕΥΣ, οδυσσευς : le sigma final se replie comme les autres.
  CHECK(EqualsFolded("\xCE\x9F\xCE\x94\xCE\xA5\xCE\xA3\xCE\xA3\xCE\x95\xCE\xA5\xCE\xA3",
                     "\xCE\xBF\xCE\xB4\xCF\x85\xCF\x83\xCF\x83\xCE\xB5\xCF\x85\xCF\x82"));
  // L'accent n'est pas de la casse : « Elodie » n'est pas « Élodie ».
  CHECK_FALSE(EqualsFolded("Elodie", "\xC3\x89lodie"));
  // Un mot sans casse ressort identique.
  const std::string untouched = "salut 123 \xEC\x95\x88\xEB\x85\x95";  // 안녕
  CHECK(CaseFolded(untouched) == untouched);
  CHECK_FALSE(EqualsFolded("STRASSE",
                           "stra\xC3\x9F"
                           "e"));
}

TEST_CASE("Repli de casse : un octet invalide passe tel quel", "[core][case_fold]") {
  // 0xE9 seul : un « é » CP1252 qui n'aurait pas été converti.
  CHECK(CaseFolded("\xE9T\xC3\x89") == "\xE9t\xC3\xA9");
  // Une séquence coupée en fin de texte.
  CHECK(CaseFolded("A\xC3") == "a\xC3");
  // Un vrai U+FFFD reste U+FFFD.
  CHECK(CaseFolded("\xEF\xBF\xBD") == "\xEF\xBF\xBD");
  CHECK(CaseFolded("").empty());
}

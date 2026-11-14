// LES DEUX AFFICHAGES DU CLASSEMENT PVP.
//
// 🔴 CE QUI SE VÉRIFIE ICI EST LA DÉCOUPE, et non le dessin. Les quads
// demandent une fonte que l'installation porte ; ce qu'un essai peut tenir,
// c'est le NOMBRE de glyphes qu'une durée demande et la place de ses
// séparateurs — deux règles qui mentiraient sans bruit.
//
// ⚠ UN AFFICHAGE À TROIS OU QUATRE GLYPHES N'EXISTE PAS. Les tranches de tête
// tombent par PAQUETS de trois, chacune emportant son séparateur : deux, cinq,
// huit ou onze, et rien entre les deux. On ne voit jamais « M:SS ».

#include "ro/render/pvp_hud.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

namespace {

using ro::render::PvpStopwatchGlyphCount;
using ro::render::PvpStopwatchSeparatorAt;

constexpr std::uint32_t kSecond = 1000;
constexpr std::uint32_t kMinute = 60 * kSecond;
constexpr std::uint32_t kHour = 60 * kMinute;
constexpr std::uint32_t kDay = 24 * kHour;

}  // namespace

TEST_CASE("Chronomètre : les tranches de tête tombent par paquets de trois", "[render][pvp]") {
  CHECK(PvpStopwatchGlyphCount(0) == 2);
  CHECK(PvpStopwatchGlyphCount(59 * kSecond) == 2);
  // La minute entière fait apparaître « MM: » d'un coup.
  CHECK(PvpStopwatchGlyphCount(kMinute) == 5);
  CHECK(PvpStopwatchGlyphCount(59 * kMinute + 59 * kSecond) == 5);
  CHECK(PvpStopwatchGlyphCount(kHour) == 8);
  CHECK(PvpStopwatchGlyphCount(23 * kHour) == 8);
  CHECK(PvpStopwatchGlyphCount(kDay) == 11);
}

TEST_CASE("Chronomètre : il n'y a jamais trois ni quatre glyphes", "[render][pvp]") {
  // ⚠ L'ÉPREUVE QUI TIENT LA RÈGLE : un découpage qui laisserait tomber la
  // dizaine des minutes donnerait « M:SS », quatre glyphes, et le client
  // n'affiche jamais cela. On balaie une journée par pas d'une seconde.
  for (std::uint32_t seconds = 0; seconds < 24 * 60 * 60; ++seconds) {
    const std::size_t glyphs = PvpStopwatchGlyphCount(seconds * kSecond);
    const bool valid = glyphs == 2 || glyphs == 5 || glyphs == 8 || glyphs == 11;
    if (!valid) {
      // On ne crie qu'une fois : sans cela un défaut noierait le rapport sous
      // quatre-vingt-six mille lignes.
      FAIL("durée de " << seconds << " s : " << glyphs << " glyphes");
    }
  }
  SUCCEED();
}

TEST_CASE("Chronomètre : les séparateurs sont aux rangs 2, 5 et 8", "[render][pvp]") {
  // 🔴 CE SONT EUX QUI EXPLIQUENT LES PAQUETS DE TROIS. Le client laisse ces
  // cases vides dans son tableau de chiffres, précisément parce que le
  // deux-points les remplace.
  CHECK_FALSE(PvpStopwatchSeparatorAt(0));
  CHECK_FALSE(PvpStopwatchSeparatorAt(1));
  CHECK(PvpStopwatchSeparatorAt(2));
  CHECK_FALSE(PvpStopwatchSeparatorAt(3));
  CHECK_FALSE(PvpStopwatchSeparatorAt(4));
  CHECK(PvpStopwatchSeparatorAt(5));
  CHECK_FALSE(PvpStopwatchSeparatorAt(6));
  CHECK_FALSE(PvpStopwatchSeparatorAt(7));
  CHECK(PvpStopwatchSeparatorAt(8));
  CHECK_FALSE(PvpStopwatchSeparatorAt(9));
  CHECK_FALSE(PvpStopwatchSeparatorAt(10));
}

TEST_CASE("Chronomètre : chaque affichage finit sur un chiffre", "[render][pvp]") {
  // ⚠ UN AFFICHAGE QUI SE TERMINERAIT PAR UN SÉPARATEUR — « 02: » — dirait que
  // la découpe a perdu une tranche. Les quatre longueurs possibles doivent
  // toutes tomber sur un chiffre.
  for (const std::size_t glyphs :
       {std::size_t{2}, std::size_t{5}, std::size_t{8}, std::size_t{11}}) {
    CHECK_FALSE(PvpStopwatchSeparatorAt(glyphs - 1));
  }
}

// LA RECHERCHE DU LECTEUR DE MUSIQUE : par numéro ou par carte, sans égard à
// la casse.

#include "ro/ui/music_player_window.h"

#include <catch2/catch_test_macros.hpp>

using ro::ui::MusicPlayerRow;
using ro::ui::MusicRowMatches;

TEST_CASE("Lecteur de musique : une recherche vide garde tout", "[ui][musique]") {
  CHECK(MusicRowMatches(MusicPlayerRow{.label = "08", .maps = "prontera"}, ""));
  CHECK(MusicRowMatches(MusicPlayerRow{.label = "200", .maps = ""}, ""));
}

TEST_CASE("Lecteur de musique : le numéro et les cartes se cherchent", "[ui][musique]") {
  const MusicPlayerRow row{.label = "08", .maps = "prontera, prt_in"};
  CHECK(MusicRowMatches(row, "08"));
  CHECK(MusicRowMatches(row, "prt_in"));
  // ⚠ SANS ÉGARD À LA CASSE : le joueur tape comme il veut.
  CHECK(MusicRowMatches(row, "PRONTERA"));
  CHECK_FALSE(MusicRowMatches(row, "geffen"));
  CHECK_FALSE(MusicRowMatches(row, "09"));
}

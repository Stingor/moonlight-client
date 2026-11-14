// LE CATALOGUE DU LECTEUR DE MUSIQUE : les fichiers du dossier, rangés par
// numéro, et les cartes de chacun.

#include "world/music_catalogue.h"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using moonclient::CatalogueMusic;
using moonclient::MusicTrack;
using moonclient::SameMusic;
using ro::formats::BgmTable;

TEST_CASE("Catalogue des musiques : la casse et le séparateur ne comptent pas", "[app][musique]") {
  CHECK(SameMusic("bgm\\08.mp3", "BGM/08.MP3"));
  CHECK_FALSE(SameMusic("bgm\\08.mp3", "bgm\\80.mp3"));
  CHECK_FALSE(SameMusic("bgm\\08.mp3", "bgm\\08.mp3x"));
}

TEST_CASE("Catalogue des musiques : rangé par numéro, « 9 » avant « 10 »", "[app][musique]") {
  const std::vector<std::string> files = {"10.mp3", "theme.mp3", "9.mp3", "100.MP3", "01.mp3"};
  const std::vector<MusicTrack> tracks = CatalogueMusic(files, BgmTable());
  REQUIRE(tracks.size() == 5);
  CHECK(tracks[0].label == "01");
  CHECK(tracks[1].label == "9");
  CHECK(tracks[2].label == "10");
  CHECK(tracks[3].label == "100");
  // Un nom qui n'est pas un numéro vient après les numéros.
  CHECK(tracks[4].label == "theme");
  // Le nom joué est celui que la table du jeu écrirait.
  CHECK(tracks[1].game_path == "bgm\\9.mp3");
}

TEST_CASE("Catalogue des musiques : ce qui ne se joue pas n'y entre pas", "[app][musique]") {
  const std::vector<std::string> files = {"08.mp3", "liste.txt", "desktop.ini"};
  const std::vector<MusicTrack> tracks = CatalogueMusic(files, BgmTable());
  REQUIRE(tracks.size() == 1);
  CHECK(tracks[0].label == "08");
}

TEST_CASE("Catalogue des musiques : chaque morceau nomme ses cartes", "[app][musique]") {
  const BgmTable table(
      {{"prontera", "bgm\\08.mp3"}, {"prt_in", "BGM\\08.mp3"}, {"geffen", "bgm\\13.mp3"}});
  const std::vector<std::string> files = {"08.mp3", "13.mp3", "99.mp3"};
  const std::vector<MusicTrack> tracks = CatalogueMusic(files, table);
  REQUIRE(tracks.size() == 3);
  CHECK(tracks[0].maps == std::vector<std::string>{"prontera", "prt_in"});
  CHECK(tracks[1].maps == std::vector<std::string>{"geffen"});
  // ⚠ UN MORCEAU QU'AUCUNE CARTE NE DÉCLARE RESTE PROPOSÉ : c'est même ce qu'on
  // vient chercher au lecteur.
  CHECK(tracks[2].maps.empty());
}

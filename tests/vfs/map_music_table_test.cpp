// LA MUSIQUE DES CARTES, lue dans sa table convertie.

#include "ro/vfs/map_music_table.h"

#include <string_view>

#include <catch2/catch_test_macros.hpp>

using ro::vfs::ParseMapMusic;

namespace {

// Ce que la conversion tire du fichier que lisait l'ancien lecteur :
//
//   // une entete
//   //prontera.rsw#bgm\\55.mp3#      une entrée désactivée par un commentaire
//   prontera.rsw#bgm\\08.mp3#
//   geffen.rsw#bgm\\13.mp3#
//   geffen.rsw#bgm\\99.mp3#         la dernière ligne l'emporte
//   sans separateur, un_seul.rsw#bgm\\01.mp3, vide.rsw##, #bgm\\02.mp3#
//   bonne.rsw#bgm\\03.mp3#
constexpr std::string_view kConverted =
    "version: 1\n"
    "maps:\n"
    "  bonne: bgm\\03.mp3\n"
    "  geffen: bgm\\99.mp3\n"
    "  prontera: bgm\\08.mp3\n";

}  // namespace

TEST_CASE("Table des musiques : une carte donne son morceau", "[vfs][bgm]") {
  auto table = ParseMapMusic(kConverted);
  REQUIRE(table.has_value());
  CHECK(table->size() == 3);
  // Les antislashs, doublés dans le fichier d'origine, sont simples.
  CHECK(table->TrackFor("prontera") == "bgm\\08.mp3");
  CHECK(table->TrackFor("geffen") == "bgm\\99.mp3");
  CHECK(table->TrackFor("bonne") == "bgm\\03.mp3");
  // La casse ne compte pas : les noms de carte s'écrivent en minuscules, mais
  // rien ne l'impose au serveur.
  CHECK(table->TrackFor("PRONTERA") == "bgm\\08.mp3");
  CHECK(table->TrackFor("morocc").empty());
}

TEST_CASE("Table des musiques : une table mal formée est refusée", "[vfs][bgm]") {
  CHECK_FALSE(ParseMapMusic("version: 1\nmaps: [prontera]\n").has_value());
  CHECK_FALSE(ParseMapMusic("version: 2\nmaps: {}\n").has_value());
  CHECK_FALSE(ParseMapMusic("maps: {}\n").has_value());
}

// LES RÉPLIQUES du barde et de la danseuse, lues dans leur table convertie.

#include "ro/vfs/talk_lines.h"

#include <string_view>

#include <catch2/catch_test_macros.hpp>

using ro::vfs::kFrostJokeLinesPath;
using ro::vfs::ParseTalkLines;

namespace {

// Ce que la conversion tire de la forme exacte des deux fichiers du jeu — un
// TITRE, puis des répliques qui commencent par une tabulation, le tout séparé
// par des retours chariot suivis d'un saut de ligne :
//
//   FROST JOKE\r\n\tMe..me..mememe...\r\n\toooooohHHHHHoooHOHOHOH\r\n\tKyaaaa----!\r\n
constexpr std::string_view kConverted =
    "version: 1\n"
    "title: FROST JOKE\n"
    "lines:\n"
    "- Me..me..mememe...\n"
    "- oooooohHHHHHoooHOHOHOH\n"
    "- Kyaaaa----!\n";

}  // namespace

TEST_CASE("Répliques : le titre ne se dit jamais, la réplique un est la deuxième ligne",
          "[vfs][talk]") {
  // ⚠ LE COMPTEUR DU CLIENT NE S'INCRÉMENTE QU'AU PREMIER RETOUR CHARIOT : la
  // réplique numéro un est donc la DEUXIÈME ligne du fichier. Compter depuis
  // zéro ferait dire « FROST JOKE » au personnage une fois sur cent dix-sept.
  auto talk = ParseTalkLines(kConverted, kFrostJokeLinesPath);
  REQUIRE(talk.has_value());
  CHECK(talk->Line(1) == "Me..me..mememe...");
  CHECK(talk->Line(2) == "oooooohHHHHHoooHOHOHOH");
  CHECK(talk->Line(3) == "Kyaaaa----!");
  // Le zéro ne désigne rien : c'est le titre, que le client n'atteint pas.
  CHECK(talk->Line(0).empty());
}

TEST_CASE("Répliques : un numéro hors de la table ne dit rien", "[vfs][talk]") {
  // ⚠ ET CE CAS ARRIVE VRAIMENT : les deux nombres de répliques sont dans
  // l'EXÉCUTABLE, pas dans les tables, et rien ne garantit qu'ils s'accordent.
  auto talk = ParseTalkLines(kConverted, kFrostJokeLinesPath);
  REQUIRE(talk.has_value());
  CHECK(talk->Line(4).empty());
  CHECK(talk->Line(9999).empty());
  auto empty = ParseTalkLines("version: 1\ntitle: ''\nlines: []\n", kFrostJokeLinesPath);
  REQUIRE(empty.has_value());
  CHECK(empty->Line(1).empty());
}

TEST_CASE("Répliques : une table mal formée est refusée", "[vfs][talk]") {
  CHECK_FALSE(ParseTalkLines("version: 1\ntitle: x\n", kFrostJokeLinesPath).has_value());
  CHECK_FALSE(ParseTalkLines("version: 1\nlines: {a: b}\n", kFrostJokeLinesPath).has_value());
  CHECK_FALSE(ParseTalkLines("version: 1\nlines: [[a]]\n", kFrostJokeLinesPath).has_value());
}

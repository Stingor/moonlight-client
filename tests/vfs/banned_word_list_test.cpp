// LES MOTS QUE LE CLIENT MASQUE, lus dans leur table convertie.

#include "ro/vfs/banned_word_list.h"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::vfs::ParseBannedWordList;

TEST_CASE("Mots interdits : la liste garde son ordre et les espaces de ses mots", "[vfs][banned]") {
  // Ce que la conversion tire de « Alpha\r\n// un commentaire#qui continue\r\n
  // beta#gamma#\r\n \t\r\n delta\r\nperdu\ndernier » : un mot finit à un retour
  // chariot ou à un dièse, un saut de ligne seul efface ce qui le précède.
  auto words = ParseBannedWordList("version: 1\nwords:\n- Alpha\n- beta\n- gamma\n- ' delta'\n");
  REQUIRE(words.has_value());
  CHECK(*words == std::vector<std::string>{"Alpha", "beta", "gamma", " delta"});
}

TEST_CASE("Mots interdits : une liste vide est une liste", "[vfs][banned]") {
  auto words = ParseBannedWordList("version: 1\nwords: []\n");
  REQUIRE(words.has_value());
  CHECK(words->empty());
}

TEST_CASE("Mots interdits : une table mal formée est refusée", "[vfs][banned]") {
  CHECK_FALSE(ParseBannedWordList("version: 1\nwords: {a: b}\n").has_value());
  CHECK_FALSE(ParseBannedWordList("version: 1\n").has_value());
  CHECK_FALSE(ParseBannedWordList("").has_value());
}

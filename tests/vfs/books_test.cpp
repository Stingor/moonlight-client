// LES LIVRES DE L'INSTALLATION : la table qui dit lesquels, et le texte, dont la
// première ligne peut être la couleur du fond.

#include "ro/vfs/books.h"

#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;

TEST_CASE("Livres : la couleur de fond se lit en tête, et seulement là", "[vfs][livres]") {
  const ro::vfs::Book book =
      ro::vfs::ParseBook("%faf0e6\r\n^000088Titre^000000\r\n\r\n%pas une couleur\r\n");
  REQUIRE(book.background.has_value());
  CHECK(book.background->red == 0xFA);
  CHECK(book.background->green == 0xF0);
  CHECK(book.background->blue == 0xE6);
  REQUIRE(book.lines.size() == 3);
  CHECK(book.lines[0] == "^000088Titre^000000");
  CHECK(book.lines[1].empty());
  CHECK(book.lines[2] == "%pas une couleur");
  // Sans marque : pas de fond, et la première ligne est du texte.
  const ro::vfs::Book plain = ro::vfs::ParseBook("Il etait une fois");
  CHECK_FALSE(plain.background.has_value());
  REQUIRE(plain.lines.size() == 1);
}

TEST_CASE("Livres : la liste convertie se lit, et refuse ce qui n'en est pas une",
          "[vfs][livres]") {
  // Ce que la conversion tire de « 11000#\r\n11001#\r\n\r\n7127#\r\n ».
  const auto list = ro::vfs::ParseBookList("version: 1\nbooks:\n- 7127\n- 11000\n- 11001\n");
  REQUIRE(list.has_value());
  CHECK(list->item_ids.size() == 3);
  CHECK(list->Has(7127));
  CHECK(list->Has(11001));
  CHECK_FALSE(list->Has(501));
  CHECK(ro::vfs::ParseBookList("version: 1\nbooks: []\n")->item_ids.empty());
  CHECK_FALSE(ro::vfs::ParseBookList("version: 1\nbooks: [-1]\n").has_value());
  CHECK_FALSE(ro::vfs::ParseBookList("version: 1\nbooks: {11000: x}\n").has_value());
  CHECK_FALSE(ro::vfs::ParseBookList("version: 2\nbooks: []\n").has_value());
}

TEST_CASE("Livres : la table et le texte, lus dans l'installation", "[vfs][livres]") {
  TempDirectory client;
  client.WriteText("data/yaml_files/book_items.yml",
                   "version: 1\nbooks:\n- 7127\n- 11000\n- 11001\n");
  client.WriteText("data/book/11000.txt", "%112233\r\nPage une\r\n");
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  const auto list = ro::vfs::ReadBookList(*vfs);
  REQUIRE(list.has_value());
  CHECK(list->Has(11000));
  CHECK(list->Has(7127));
  CHECK_FALSE(list->Has(501));
  const auto book = ro::vfs::ReadBook(*vfs, 11000);
  REQUIRE(book.has_value());
  REQUIRE(book->lines.size() == 1);
  CHECK(book->lines[0] == "Page une");
  CHECK_FALSE(ro::vfs::ReadBook(*vfs, 11001).has_value());
}

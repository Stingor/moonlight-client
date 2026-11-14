// LES SIGNETS DES LIVRES sur le disque : par personnage, tous dans un fichier,
// écrits à la fermeture ou en fin de partie, et relus à l'ouverture quand le
// réglage « Livres : ouvrir à la première page » est éteint.

#include "items/book_bookmark_file.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/book_window.h"
#include "ro/vfs/vfs.h"

#include "items/book_reader.h"
#include "support/temp_directory.h"

namespace {

using moonclient::BookBookmarkFilePath;
using moonclient::BookBookmarks;
using moonclient::BookReader;
using moonclient::LoadBookBookmarks;
using moonclient::SaveBookBookmarks;
using ro::test_support::TempDirectory;

constexpr std::uint32_t kMine = 150000;
constexpr std::uint32_t kOther = 150001;
constexpr std::uint32_t kBook = 11000;
constexpr std::uint32_t kOtherBook = 11001;
constexpr std::uint32_t kUnknownBook = 11999;

[[nodiscard]] std::string Contents(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

[[nodiscard]] std::filesystem::path OursRelative() {
  return std::filesystem::path("savedata") / "book_bookmarks.yml";
}

// Un livre de quelques pages, dans l'arborescence du client.
void WriteBook(const TempDirectory& client, std::uint32_t item_id) {
  std::string text = "%faf0e6\r\n";
  constexpr int kLines = 40;
  for (int line = 0; line < kLines; ++line) {
    text += "Ligne " + std::to_string(line) + "\r\n";
  }
  client.WriteText("data/book/" + std::to_string(item_id) + ".txt", text);
}

}  // namespace

TEST_CASE("Signets des livres : ce qui s'écrit se relit, par personnage", "[app][livres]") {
  const TempDirectory folder;
  REQUIRE(SaveBookBookmarks(folder.path(), kMine, BookBookmarks{{kBook, 3}, {kOtherBook, 2}}));
  REQUIRE(SaveBookBookmarks(folder.path(), kOther, BookBookmarks{{kBook, 5}}));

  const BookBookmarks mine = LoadBookBookmarks(folder.path(), kMine);
  CHECK(mine == BookBookmarks{{kBook, 3}, {kOtherBook, 2}});
  CHECK(LoadBookBookmarks(folder.path(), kOther) == BookBookmarks{{kBook, 5}});
  // Un livre jamais refermé n'a pas de signet.
  CHECK_FALSE(mine.contains(kUnknownBook));
  CHECK(BookBookmarkFilePath(folder.path()) == folder.path() / OursRelative());

  SECTION("réécrire un personnage garde les autres, à leur place") {
    REQUIRE(SaveBookBookmarks(folder.path(), kMine, BookBookmarks{{kBook, 7}}));
    CHECK(LoadBookBookmarks(folder.path(), kMine) == BookBookmarks{{kBook, 7}});
    CHECK(LoadBookBookmarks(folder.path(), kOther) == BookBookmarks{{kBook, 5}});
    const std::string text = Contents(BookBookmarkFilePath(folder.path()));
    CHECK(text.find("page: 7") < text.find("page: 5"));
  }
}

TEST_CASE("Signets des livres : un fichier absent ne rend aucun signet", "[app][livres]") {
  const TempDirectory folder;
  CHECK(LoadBookBookmarks(folder.path(), kMine).empty());
}

TEST_CASE("Signets des livres : un fichier illisible se tolère et ne se réécrit pas",
          "[app][livres]") {
  // Repartir d'une liste vide effacerait les signets des autres personnages.
  const TempDirectory folder;
  const std::string broken = "book_bookmarks: [ {cid: 150000, id: \"11000\n";
  folder.WriteText(OursRelative(), broken);
  CHECK(LoadBookBookmarks(folder.path(), kMine).empty());
  CHECK_FALSE(SaveBookBookmarks(folder.path(), kMine, BookBookmarks{{kBook, 2}}));
  CHECK(Contents(BookBookmarkFilePath(folder.path())) == broken);
}

TEST_CASE("Signets des livres : une entrée incomplète se saute", "[app][livres]") {
  const TempDirectory folder;
  folder.WriteText(OursRelative(),
                   "book_bookmarks:\n"
                   "  - cid: 150000\n"
                   "    page: 4\n"
                   "  - cid: 150000\n"
                   "    id: 11001\n"
                   "    page: 0\n"
                   "  - cid: 150000\n"
                   "    id: 11000\n"
                   "    page: abc\n"
                   "  - 12\n"
                   "  - cid: 150000\n"
                   "    id: 11000\n"
                   "    page: 6\n");
  CHECK(LoadBookBookmarks(folder.path(), kMine) == BookBookmarks{{kBook, 6}});

  SECTION("et une racine qui n'est pas une table ne casse rien") {
    folder.WriteText(OursRelative(), "- 1\n- 2\n");
    CHECK(LoadBookBookmarks(folder.path(), kMine).empty());
  }
}

TEST_CASE("Livre : le signet survit à la session quand le réglage est éteint", "[app][livres]") {
  const TempDirectory client;
  WriteBook(client, kBook);
  WriteBook(client, kUnknownBook);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  constexpr int kClosedAt = 3;

  {
    BookReader reader;
    reader.See(&*vfs, nullptr, nullptr);
    reader.RememberFor(client.path(), kMine);
    ro::ui::BookWindowState window;
    reader.Open(kBook, window, /*first_page=*/false);
    CHECK(window.page == 1);
    reader.NoteClosed(kBook, kClosedAt);
  }

  BookReader next_session;
  next_session.See(&*vfs, nullptr, nullptr);
  next_session.RememberFor(client.path(), kMine);
  ro::ui::BookWindowState window;
  next_session.Open(kBook, window, /*first_page=*/false);
  CHECK(window.page == kClosedAt);
  // Refermée : le même livre ouvert ne se rouvre pas.
  window.open = false;

  SECTION("le réglage allumé ouvre à la première page") {
    next_session.Open(kBook, window, /*first_page=*/true);
    CHECK(window.page == 1);
  }
  SECTION("un livre sans signet s'ouvre à la première page") {
    next_session.Open(kUnknownBook, window, /*first_page=*/false);
    CHECK(window.page == 1);
  }
  SECTION("un autre personnage n'hérite pas du signet") {
    BookReader other;
    other.See(&*vfs, nullptr, nullptr);
    other.RememberFor(client.path(), kOther);
    other.Open(kBook, window, /*first_page=*/false);
    CHECK(window.page == 1);
  }
}

TEST_CASE("Livre : la fin de partie retient la page d'un livre encore ouvert", "[app][livres]") {
  const TempDirectory client;
  WriteBook(client, kBook);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  constexpr int kReadingAt = 2;

  BookReader reader;
  reader.See(&*vfs, nullptr, nullptr);
  reader.RememberFor(client.path(), kMine);
  ro::ui::BookWindowState window;
  reader.Open(kBook, window, /*first_page=*/false);
  REQUIRE(window.open);

  SECTION("livre ouvert : la page s'écrit") {
    window.page = kReadingAt;
    reader.NoteLeaving(window);
    CHECK(LoadBookBookmarks(client.path(), kMine) == BookBookmarks{{kBook, kReadingAt}});
  }
  SECTION("livre refermé : rien ne s'écrit") {
    window.page = kReadingAt;
    window.open = false;
    reader.NoteLeaving(window);
    CHECK_FALSE(std::filesystem::exists(BookBookmarkFilePath(client.path())));
  }
}

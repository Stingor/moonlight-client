// LA LECTURE À VOIX HAUTE : une ligne quand plus de quatre secondes ont passé,
// la première à l'image suivante, les lignes vides sautées — et chacune
// s'écrit dans la conversation du seul lecteur, sans orateur.

#include "items/book_reader.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"
#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using moonclient::BookReader;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint32_t kBook = 11000;
constexpr std::uint32_t kOtherBook = 11001;
constexpr std::uint32_t kStart = 100000;
constexpr std::uint32_t kStepMs = 4000;

}  // namespace

TEST_CASE("Livre : la récitation s'écrit pour le seul lecteur", "[app][livres]") {
  TempDirectory client;
  client.WriteText("data/book/11000.txt", "%faf0e6\r\nUne\r\n\r\nDeux\r\n");
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  ro::sim::World world;
  world.set_player_name("Lecteur");

  BookReader reader;
  reader.See(&*vfs, nullptr, nullptr);
  ro::ui::BookWindowState window;
  reader.StartRecital(kBook, window, /*first_page=*/true, kStart);
  CHECK(reader.reciting());
  CHECK_FALSE(window.open);
  // La première ligne part à l'image suivante.
  CHECK(reader.Tick(world, window, kStart + 1));
  CHECK(reader.recital_line() == "Une");
  REQUIRE(world.chat_lines().size() == 1);
  const ro::sim::ChatLine& said = world.chat_lines().back();
  CHECK(said.text == "Une");
  CHECK(said.kind == ro::sim::ChatKind::kServer);
  CHECK(said.type == ro::sim::ChatType::kPublic);
  CHECK(said.speaker_id == 0);
  CHECK(said.color == 0xFFE4E1);
  // Rien tant que quatre secondes pleines n'ont pas passé.
  CHECK_FALSE(reader.Tick(world, window, kStart + 1 + kStepMs));
  CHECK(world.chat_lines().size() == 1);
  // La ligne vide se saute : « Deux » vient ensuite.
  CHECK(reader.Tick(world, window, kStart + 2 + kStepMs));
  CHECK(reader.recital_line() == "Deux");
  CHECK(world.chat_lines().back().text == "Deux");
  // La fin du livre arrête tout.
  CHECK_FALSE(reader.Tick(world, window, kStart + 3 + 2 * kStepMs));
  CHECK_FALSE(reader.reciting());
  CHECK(reader.recital_line().empty());
  CHECK(world.chat_lines().size() == 2);
}

TEST_CASE("Livre : « Lecture auto » pendant une récitation l'arrête, « Lire » aussi",
          "[app][livres]") {
  TempDirectory client;
  client.WriteText("data/book/11000.txt", "Une\r\nDeux\r\n");
  client.WriteText("data/book/11001.txt", "Autre\r\n");
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  ro::sim::World world;
  BookReader reader;
  reader.See(&*vfs, nullptr, nullptr);
  ro::ui::BookWindowState window;

  reader.StartRecital(kBook, window, /*first_page=*/true, kStart);
  CHECK(reader.Tick(world, window, kStart + 1));
  // Le bouton bascule, quel que soit le livre désigné.
  reader.StartRecital(kOtherBook, window, /*first_page=*/true, kStart + 2);
  CHECK_FALSE(reader.reciting());
  CHECK(reader.recital_line().empty());
  CHECK_FALSE(reader.Tick(world, window, kStart + 3 + kStepMs));
  CHECK(world.chat_lines().size() == 1);

  // « Lire » coupe la récitation et ouvre la fenêtre.
  reader.StartRecital(kBook, window, /*first_page=*/true, kStart + 10);
  CHECK(reader.reciting());
  reader.Open(kBook, window, /*first_page=*/true);
  CHECK_FALSE(reader.reciting());
  CHECK(window.open);
  CHECK(window.page == 1);
  // Le même livre déjà sous les yeux : « Lire » ne touche à rien.
  window.page = 2;
  reader.Open(kBook, window, /*first_page=*/true);
  CHECK(window.page == 2);
  // Le livre ouvert se récite depuis sa page, et sa fenêtre se cache.
  reader.StartRecital(kBook, window, /*first_page=*/true, kStart + 20);
  CHECK(reader.reciting());
  CHECK_FALSE(window.open);
  reader.Open(kBook, window, /*first_page=*/true);
  CHECK(window.page == 2);
}

TEST_CASE("Livre : la récitation dit les lignes que la fenêtre replie", "[app][livres]") {
  TempDirectory client;
  client.WriteText("data/book/11000.txt", "Une phrase longue\r\n\r\nFin\r\n");
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  ro::sim::World world;
  BookReader reader;
  reader.See(&*vfs, nullptr, nullptr);
  ro::ui::BookWindowState window;
  constexpr float kMeasuredWidth = 120.0F;

  SECTION("une largeur mesurée attend le repli de ce livre") {
    window.rows_width = kMeasuredWidth;
    window.rows_for = kOtherBook;
    reader.StartRecital(kBook, window, /*first_page=*/true, kStart);
    CHECK_FALSE(reader.Tick(world, window, kStart + 1));
    CHECK(world.chat_lines().empty());
    CHECK(reader.reciting());
  }
  SECTION("les lignes repliées, marques de couleur ôtées, la vide sautée") {
    reader.StartRecital(kBook, window, /*first_page=*/true, kStart);
    window.rows_width = kMeasuredWidth;
    window.rows_for = kBook;
    window.rows = {"^000000Une phrase", "^ff0000longue", "^000000", "^000000Fin"};
    CHECK(reader.Tick(world, window, kStart + 1));
    CHECK(reader.recital_line() == "Une phrase");
    CHECK(reader.Tick(world, window, kStart + 2 + kStepMs));
    CHECK(reader.recital_line() == "longue");
    CHECK(reader.Tick(world, window, kStart + 3 + 2 * kStepMs));
    CHECK(reader.recital_line() == "Fin");
    REQUIRE(world.chat_lines().size() == 3);
    CHECK(world.chat_lines().back().text == "Fin");
  }
}

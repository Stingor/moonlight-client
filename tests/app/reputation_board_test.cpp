// LA RÉPUTATION côté jeu : le livre lu dans sa table, l'icône qui ouvre la
// fenêtre, le script qui l'ouvre sur un groupe en désignant une faction, et les
// factions qui ne se montrent qu'avec des points.

#include "world/reputation_board.h"

#include <cstdint>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/character_ledger.h"
#include "ro/ui/reputation_window.h"

#include "world/reputation_book.h"

using moonclient::ReputationBoard;
using moonclient::ReputationBook;
using moonclient::ReputationShown;
using moonclient::ReputationWindows;
using ro::sim::CharacterLedger;

namespace {

constexpr std::string_view kBookYaml = R"(version: 1
mode: Prerenewal
factions:
  - id: 4
    name: "Isgard"
    least: -3000
    most: 3000
    shown: with_points
  - id: 3
    name: "Grey Wolf Village"
    least: -5000
    most: 5000
    shown: always
  - id: 7
    name: "Hidden"
    least: 0
    most: 1000
    shown: never
groups:
  - id: 2
    name: "Arunafeltz"
    factions: [3, 4]
)";

constexpr std::uint64_t kWolfId = 3;
constexpr std::uint64_t kIsgardId = 4;
constexpr std::uint64_t kArunafeltzId = 2;

[[nodiscard]] ReputationBook Book() {
  auto book = ReputationBook::Parse(kBookYaml);
  REQUIRE(book.has_value());
  return *book;
}

}  // namespace

TEST_CASE("Réputation : le livre se lit, factions rangées par numéro", "[app][reputation]") {
  const ReputationBook book = Book();
  CHECK(book.loaded());
  REQUIRE(book.factions().size() == 3);
  CHECK(book.factions()[0].faction_id == kWolfId);
  const moonclient::ReputationFaction* isgard = book.FindFaction(kIsgardId);
  REQUIRE(isgard != nullptr);
  CHECK(isgard->shown == ReputationShown::kWithPoints);
  CHECK(isgard->least == -3000);
  CHECK(book.FindFaction(99) == nullptr);
  REQUIRE(book.groups().size() == 1);
  CHECK(book.groups()[0].faction_ids.size() == 2);

  CHECK_FALSE(ReputationBook::Parse("version: 9\n").has_value());
  CHECK_FALSE(ReputationBook{}.loaded());
}

TEST_CASE("Réputation : l'icône ouvre, l'attente des points se dit", "[app][reputation]") {
  const ReputationBook book = Book();
  CharacterLedger ledger;
  ReputationWindows windows;
  ReputationBoard board;
  board.Advance(windows, ledger, book);
  CHECK_FALSE(windows.window.open);
  REQUIRE(windows.view != nullptr);

  moonclient::ToggleReputationWindow(windows);
  board.Advance(windows, ledger, book);
  CHECK(windows.window.open);
  CHECK(windows.view->awaiting_points);

  ledger.SetReputation(kWolfId, 1200);
  ledger.MarkReputationReceived();
  board.Advance(windows, ledger, book);
  CHECK_FALSE(windows.view->awaiting_points);
  // Isgard attend d'avoir des points ; la faction cachée ne se montre jamais.
  REQUIRE(windows.view->rows.size() == 1);
  CHECK(windows.view->rows[0].name == "Grey Wolf Village");
  CHECK(windows.view->rows[0].points == 1200);

  ledger.SetReputation(kIsgardId, -50);
  board.Advance(windows, ledger, book);
  CHECK(windows.view->rows.size() == 2);

  moonclient::ToggleReputationWindow(windows);
  CHECK_FALSE(windows.window.open);
}

TEST_CASE("Réputation : le script ouvre un groupe et désigne une faction", "[app][reputation]") {
  const ReputationBook book = Book();
  CharacterLedger ledger;
  ledger.SetReputation(kWolfId, 2000);
  ledger.SetReputation(kIsgardId, 500);
  ledger.MarkReputationReceived();
  ReputationWindows windows;
  windows.window.search = {'z', 'z', '\0'};
  ReputationBoard board;
  ledger.AskReputationOpening({.group_id = kArunafeltzId, .faction_id = kWolfId});
  board.Advance(windows, ledger, book);
  CHECK(windows.window.open);
  CHECK(windows.window.group == 1);
  // La recherche se vide : la faction désignée doit être dans la liste.
  CHECK(windows.window.search[0] == '\0');
  REQUIRE(windows.view->rows.size() == 2);
  CHECK(windows.view->rows[0].highlighted);
  CHECK_FALSE(windows.view->rows[1].highlighted);
  CHECK(windows.view->group_points == 2500);

  // Le joueur change de groupe : la faction cesse d'être désignée.
  windows.window.group = ro::ui::kReputationAllGroups;
  board.Advance(windows, ledger, book);
  windows.window.group = 1;
  board.Advance(windows, ledger, book);
  CHECK_FALSE(windows.view->rows[0].highlighted);
}

TEST_CASE("Réputation : la recherche filtre, sans tenir compte de la casse", "[app][reputation]") {
  const ReputationBook book = Book();
  CharacterLedger ledger;
  ledger.SetReputation(kIsgardId, 10);
  ledger.MarkReputationReceived();
  ReputationWindows windows;
  windows.window.open = true;
  windows.window.search = {'w', 'O', 'l', 'f', '\0'};
  ReputationBoard board;
  board.Advance(windows, ledger, book);
  REQUIRE(windows.view->rows.size() == 1);
  CHECK(windows.view->rows[0].name == "Grey Wolf Village");
}

TEST_CASE("Réputation : un groupe inconnu du livre ouvre « toutes »", "[app][reputation]") {
  const ReputationBook book = Book();
  CharacterLedger ledger;
  ReputationWindows windows;
  ReputationBoard board;
  ledger.AskReputationOpening({.group_id = 42, .faction_id = 0});
  board.Advance(windows, ledger, book);
  CHECK(windows.window.open);
  CHECK(windows.window.group == ro::ui::kReputationAllGroups);
}

TEST_CASE("Réputation : sans livre, la vue le dit", "[app][reputation]") {
  CharacterLedger ledger;
  ReputationWindows windows;
  windows.window.open = true;
  ReputationBoard board;
  board.Advance(windows, ledger, ReputationBook{});
  CHECK(windows.view->book_missing);
}

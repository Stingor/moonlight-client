// LES FICHIERS DU MODE SURVIVORS : le journal de la dernière partie, et la fin
// d'une partie interrompue, montrée une seule fois.

#include "survivors/survivors_file.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/survivors_journal.h"
#include "ro/sim/survivors_state.h"

#include "support/temp_directory.h"

using moonclient::DecodeSurvivorsEnding;
using moonclient::EncodeSurvivorsEnding;
using moonclient::kInterruptedEndingFileName;
using moonclient::kLastRunFileName;
using moonclient::SurvivorsEnding;
using moonclient::SurvivorsSavePath;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint64_t kSeed = 20261024;
constexpr std::uint8_t kWestHeading = 16;
constexpr std::uint32_t kTurnStep = 40;
constexpr std::uint32_t kFinalStep = 900;

[[nodiscard]] SurvivorsEnding SampleEnding() {
  SurvivorsEnding ending;
  ending.class_id = 1;
  ending.tally = ro::sim::SurvivorsTally{.survived_steps = kFinalStep,
                                         .kills = 12,
                                         .boss_kills = 0,
                                         .kill_points = 30,
                                         .level = 4,
                                         .exp_total = 77,
                                         .hp_left = -3,
                                         .damage_dealt = 456,
                                         .gems_gathered = 9,
                                         .foes_alive = 33,
                                         .outcome = ro::sim::SurvivorsOutcome::kAbandoned,
                                         .elite_kills = 0,
                                         .chests_opened = 1,
                                         .evolutions = 0};
  ending.recap.kills_by_foe = {{.id = 1002, .value = 7}, {.id = 1063, .value = 5}};
  ending.recap.damage_by_skill = {{.id = 19, .value = 400}, {.id = 13, .value = 56}};
  ending.recap.cards_found = {4072, 4005};
  ending.recap.chills = 321;
  ending.cards = {{.card_item_id = 4001, .level = 2}};
  ending.zeny = 250;
  ending.board = moonclient::SurvivorsRunBoard{.kind = moonclient::SurvivorsBoardKind::kChallenge,
                                               .week = {.year = 2026, .week = 43}};
  return ending;
}

constexpr std::size_t kVersionAt = 4;
// Ce que la version 3 ajoute après les glaçages : coop u8, évolution du héros u8.
constexpr std::size_t kThirdVersionTail = 1 + 1;

// LA FIN D'UNE VERSION 2, telle que l'écrivait le client d'avant la coop au
// fichier : celle de `ending`, sans ce que la version 3 ajoute.
[[nodiscard]] std::vector<std::uint8_t> SecondVersionBytes(const SurvivorsEnding& ending) {
  std::vector<std::uint8_t> bytes = EncodeSurvivorsEnding(ending);
  bytes.resize(bytes.size() - kThirdVersionTail);
  bytes[kVersionAt] = 2;
  bytes[kVersionAt + 1] = 0;
  return bytes;
}

// LA FIN D'UNE VERSION 1, telle que l'écrivait le client d'avant le carnet :
// celle de `ending`, sans tableau, sans carte tombée ni glaçage.
[[nodiscard]] std::vector<std::uint8_t> FirstVersionBytes(SurvivorsEnding ending) {
  // Ce que la version 2 ajoute après les Zeny, sans carte tombée : tableau
  // u8, année i32, semaine u8, nombre u16, glaçages u64.
  constexpr std::size_t kSecondVersionTail = 1 + 4 + 1 + 2 + 8;
  ending.recap.cards_found.clear();
  std::vector<std::uint8_t> bytes = SecondVersionBytes(ending);
  bytes.resize(bytes.size() - kSecondVersionTail);
  bytes[kVersionAt] = 1;
  bytes[kVersionAt + 1] = 0;
  return bytes;
}

[[nodiscard]] ro::sim::SurvivorsJournal SampleJournal() {
  return ro::sim::SurvivorsJournal{
      .ruleset_version = 2,
      .start = {.seed = kSeed, .class_id = 1, .arena_id = 1, .first_skill_id = 19, .loadout = {}},
      .entries = {{.step = kTurnStep,
                   .kind = ro::sim::JournalKind::kHeading,
                   .value = kWestHeading},
                  {.step = kFinalStep, .kind = ro::sim::JournalKind::kAbandon, .value = 0}},
      .final_step = kFinalStep};
}

}  // namespace

TEST_CASE("SurvivorsEnding : écrite puis relue à l'identique", "[app][survivors]") {
  const SurvivorsEnding ending = SampleEnding();
  const std::vector<std::uint8_t> bytes = EncodeSurvivorsEnding(ending);
  const auto decoded = DecodeSurvivorsEnding(bytes);
  REQUIRE(decoded.has_value());
  CHECK(*decoded == ending);
}

TEST_CASE("SurvivorsEnding : le bilan d'une coop garde son équipe et le héros de ce joueur",
          "[app][survivors]") {
  SurvivorsEnding ending = SampleEnding();
  ending.tally.evolutions = 1;
  ending.board = moonclient::SurvivorsRunBoard{.coop = true};
  SECTION("l'allié a évolué, pas ce héros") { ending.own_evolved = false; }
  SECTION("ce héros a évolué") { ending.own_evolved = true; }
  const auto decoded = DecodeSurvivorsEnding(EncodeSurvivorsEnding(ending));
  REQUIRE(decoded.has_value());
  CHECK(decoded->board.coop);
  CHECK(decoded->own_evolved == ending.own_evolved);
  CHECK(*decoded == ending);
}

TEST_CASE("SurvivorsEnding : une fin de version 2 se relit hors coop", "[app][survivors]") {
  SurvivorsEnding ending = SampleEnding();
  ending.board.coop = true;
  ending.own_evolved = true;
  const auto decoded = DecodeSurvivorsEnding(SecondVersionBytes(ending));
  REQUIRE(decoded.has_value());
  CHECK(*decoded == SampleEnding());
}

TEST_CASE("SurvivorsEnding : une fin de version 1 se relit en partie libre", "[app][survivors]") {
  const auto decoded = DecodeSurvivorsEnding(FirstVersionBytes(SampleEnding()));
  REQUIRE(decoded.has_value());
  SurvivorsEnding expected = SampleEnding();
  expected.board = {};
  expected.recap.cards_found.clear();
  expected.recap.chills = 0;
  CHECK(*decoded == expected);
  // Une version que ce client ne connaît pas se refuse.
  std::vector<std::uint8_t> later = EncodeSurvivorsEnding(SampleEnding());
  later[kVersionAt] = 4;
  CHECK(DecodeSurvivorsEnding(later).error().code == ro::core::ErrorCode::kUnsupportedVersion);
}

TEST_CASE("SurvivorsEnding : les octets abîmés sont refusés", "[app][survivors]") {
  const std::vector<std::uint8_t> bytes = EncodeSurvivorsEnding(SampleEnding());
  // Tronquée à chaque longueur : jamais lue.
  for (std::size_t size = 0; size < bytes.size(); ++size) {
    CHECK_FALSE(DecodeSurvivorsEnding(std::span(bytes.data(), size)).has_value());
  }
  std::vector<std::uint8_t> longer = bytes;
  longer.push_back(0);
  CHECK(DecodeSurvivorsEnding(longer).error().code == ro::core::ErrorCode::kCorruptData);
  std::vector<std::uint8_t> signed_wrong = bytes;
  signed_wrong[0] = 'X';
  CHECK(DecodeSurvivorsEnding(signed_wrong).error().code == ro::core::ErrorCode::kBadSignature);
  // Le tableau se lit avant l'année, la semaine, les deux cartes tombées, les
  // glaçages et les marques de coop : un tableau inconnu se refuse.
  constexpr std::size_t kBoardFromEnd = 1 + 4 + 1 + 2 + 2 * 2 + 8 + kThirdVersionTail;
  std::vector<std::uint8_t> unknown_board = bytes;
  unknown_board[unknown_board.size() - kBoardFromEnd] = 7;
  CHECK(DecodeSurvivorsEnding(unknown_board).error().code == ro::core::ErrorCode::kCorruptData);
}

TEST_CASE("Fichiers survivors : la partie interrompue se montre une seule fois",
          "[app][survivors]") {
  const TempDirectory root;
  CHECK_FALSE(moonclient::TakeInterruptedEnding(root.path()).has_value());
  const SurvivorsEnding ending = SampleEnding();
  REQUIRE(moonclient::SaveInterruptedRun(root.path(), SampleJournal(), ending).has_value());
  CHECK(std::filesystem::exists(SurvivorsSavePath(root.path(), kLastRunFileName)));
  const auto journal =
      moonclient::ReadJournalFile(SurvivorsSavePath(root.path(), kLastRunFileName));
  REQUIRE(journal.has_value());
  CHECK(*journal == SampleJournal());

  const std::optional<SurvivorsEnding> taken = moonclient::TakeInterruptedEnding(root.path());
  REQUIRE(taken.has_value());
  CHECK(*taken == ending);
  CHECK_FALSE(moonclient::TakeInterruptedEnding(root.path()).has_value());
  // Le journal, lui, reste.
  CHECK(std::filesystem::exists(SurvivorsSavePath(root.path(), kLastRunFileName)));
}

TEST_CASE("Fichiers survivors : une fin illisible se retire sans se montrer", "[app][survivors]") {
  const TempDirectory root;
  const std::vector<std::uint8_t> broken{'M', 'S', 'V', 'E'};
  root.WriteFile(std::filesystem::path("savedata") / "survivors" / kInterruptedEndingFileName,
                 broken);
  CHECK_FALSE(moonclient::TakeInterruptedEnding(root.path()).has_value());
  CHECK_FALSE(std::filesystem::exists(SurvivorsSavePath(root.path(), kInterruptedEndingFileName)));
  CHECK(
      moonclient::ReadJournalFile(SurvivorsSavePath(root.path(), kLastRunFileName)).error().code ==
      ro::core::ErrorCode::kNotFound);
}

TEST_CASE("Fichiers survivors : une partie finie n'écrit que son journal", "[app][survivors]") {
  const TempDirectory root;
  REQUIRE(moonclient::SaveLastRun(root.path(), SampleJournal()).has_value());
  CHECK(std::filesystem::exists(SurvivorsSavePath(root.path(), kLastRunFileName)));
  CHECK_FALSE(std::filesystem::exists(SurvivorsSavePath(root.path(), kInterruptedEndingFileName)));
}

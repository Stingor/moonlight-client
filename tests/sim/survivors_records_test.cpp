// LES RECORDS LOCAUX : dix lignes au plus, rangées par version des règles,
// puis par score, par temps survécu et par date, en insertion stable.

#include "ro/sim/survivors_records.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::kRecordsPerBoard;
using ro::sim::PlaceRecord;
using ro::sim::RanksAbove;
using ro::sim::SurvivorsRecord;

namespace {

[[nodiscard]] SurvivorsRecord Run(std::uint64_t score, std::uint32_t survived_ms = 0,
                                  std::int64_t played_at_utc_s = 0) {
  SurvivorsRecord record;
  record.score = score;
  record.survived_ms = survived_ms;
  record.played_at_utc_s = played_at_utc_s;
  return record;
}

// Un tableau plein : dix parties de score 1000, 900, … 100.
[[nodiscard]] std::vector<SurvivorsRecord> FullBoard() {
  constexpr std::uint64_t kScoreStep = 100;
  std::vector<SurvivorsRecord> board;
  for (std::size_t rank = 0; rank < kRecordsPerBoard; ++rank) {
    REQUIRE(PlaceRecord(board, Run((kRecordsPerBoard - rank) * kScoreStep)).has_value());
  }
  return board;
}

}  // namespace

TEST_CASE("Records survivors : la onzième partie reste hors du tableau", "[sim][survivors]") {
  std::vector<SurvivorsRecord> board = FullBoard();
  REQUIRE(board.size() == kRecordsPerBoard);
  CHECK_FALSE(PlaceRecord(board, Run(50)).has_value());
  // À égalité avec la dernière, la partie déjà inscrite garde sa place.
  CHECK_FALSE(PlaceRecord(board, Run(100)).has_value());
  CHECK(board.size() == kRecordsPerBoard);
  CHECK(board.back().score == 100);
}

TEST_CASE("Records survivors : insertion en tête et en queue", "[sim][survivors]") {
  std::vector<SurvivorsRecord> board = FullBoard();
  CHECK(PlaceRecord(board, Run(5000)) == std::optional<std::size_t>(0));
  CHECK(board.front().score == 5000);
  CHECK(board.size() == kRecordsPerBoard);
  // L'ancienne dixième (100) est sortie, l'ancienne neuvième (200) ferme le tableau.
  CHECK(board.back().score == 200);
  CHECK(PlaceRecord(board, Run(250)) == std::optional<std::size_t>(kRecordsPerBoard - 1));
  CHECK(board.back().score == 250);

  std::vector<SurvivorsRecord> short_board{Run(300)};
  CHECK(PlaceRecord(short_board, Run(10)) == std::optional<std::size_t>(1));
  CHECK(short_board.size() == 2);
}

TEST_CASE("Records survivors : une égalité de score se tranche par le temps puis la date",
          "[sim][survivors]") {
  constexpr std::uint32_t kLong = 300'000;
  constexpr std::uint32_t kShort = 120'000;
  constexpr std::int64_t kEarly = 1'000;
  constexpr std::int64_t kLate = 2'000;
  CHECK(RanksAbove(Run(500, kLong), Run(500, kShort)));
  CHECK(RanksAbove(Run(500, kShort, kEarly), Run(500, kShort, kLate)));
  CHECK_FALSE(RanksAbove(Run(500, kShort, kEarly), Run(500, kShort, kEarly)));

  std::vector<SurvivorsRecord> board{Run(500, kShort, kEarly)};
  // Plus longue : devant. Plus tardive à temps égal : derrière.
  CHECK(PlaceRecord(board, Run(500, kLong, kLate)) == std::optional<std::size_t>(0));
  CHECK(PlaceRecord(board, Run(500, kShort, kLate)) == std::optional<std::size_t>(2));
  // Parfaitement égale : après celle déjà inscrite.
  SurvivorsRecord twin = Run(500, kShort, kEarly);
  twin.seed = 7;
  CHECK(PlaceRecord(board, twin) == std::optional<std::size_t>(2));
  REQUIRE(board.size() == 4);
  CHECK(board[1].seed == 0);
  CHECK(board[2].seed == 7);
}

TEST_CASE("Records survivors : seules les règles en vigueur évincent une partie ancienne",
          "[sim][survivors]") {
  constexpr std::uint16_t kOlderRules = 3;
  constexpr std::uint16_t kCurrentRules = 4;
  // Dix parties anciennes, toutes mieux notées que la partie neuve.
  std::vector<SurvivorsRecord> board = FullBoard();
  for (SurvivorsRecord& kept : board) kept.ruleset = kOlderRules;
  SurvivorsRecord fresh = Run(50);
  fresh.ruleset = kCurrentRules;
  CHECK(PlaceRecord(board, fresh) == std::optional<std::size_t>(0));
  REQUIRE(board.size() == kRecordsPerBoard);
  CHECK(board.back().score == 200);
  // Une partie ancienne ne passe plus devant la neuve, ni n'en évince.
  SurvivorsRecord older_best = Run(5000);
  older_best.ruleset = kOlderRules;
  CHECK(PlaceRecord(board, older_best) == std::optional<std::size_t>(1));
  CHECK(board.front().ruleset == kCurrentRules);
}

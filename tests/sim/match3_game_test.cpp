// ROJEWELED, côté règles : échange, refus, cascade, grille neuve, fin de
// partie, indice.

#include "ro/sim/match3_game.h"

#include <algorithm>
#include <array>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using ro::sim::kMatch3Cells;
using ro::sim::kMatch3Side;
using ro::sim::Match3Cell;
using ro::sim::Match3Game;
using ro::sim::Match3Phase;

namespace {

constexpr std::uint32_t kSeed = 7;
// De quoi finir n'importe quelle partie : trente coups, chacun suivi de ses
// cascades.
constexpr int kMostTicks = 100000;
// Assez de parties neuves pour qu'une grille mal tirée se montre.
constexpr std::uint32_t kNewGameSeeds = 200;

// UNE GRILLE SANS ALIGNEMENT : deux voisines d'une rangée diffèrent d'une
// espèce, deux voisines d'une colonne de deux. On y pose ensuite à la main
// l'alignement à provoquer.
std::array<int, kMatch3Cells> PlainGrid() {
  std::array<int, kMatch3Cells> kinds = {};
  for (int row = 0; row < kMatch3Side; ++row) {
    for (int column = 0; column < kMatch3Side; ++column) {
      kinds[static_cast<std::size_t>(row * kMatch3Side + column)] =
          (2 * row + column) % ro::sim::kMatch3Kinds;
    }
  }
  return kinds;
}

// La grille simple, où échanger (0,2) et (1,2) aligne trois « 4 » en haut.
std::array<int, kMatch3Cells> GridWithMove() {
  std::array<int, kMatch3Cells> kinds = PlainGrid();
  kinds[0] = 4;
  kinds[1] = 4;
  return kinds;
}

// Une rangée ou une colonne porte un alignement.
[[nodiscard]] bool HasRun(const Match3Game& game) {
  for (const bool horizontal : {true, false}) {
    for (int across = 0; across < kMatch3Side; ++across) {
      const auto cell = [&](int at) {
        return horizontal ? Match3Cell{across, at} : Match3Cell{at, across};
      };
      int run = 1;
      for (int along = 1; along < kMatch3Side; ++along) {
        run = game.KindAt(cell(along)) == game.KindAt(cell(along - 1)) ? run + 1 : 1;
        if (run >= ro::sim::kMatch3ShortestRun) {
          return true;
        }
      }
    }
  }
  return false;
}

// Joue jusqu'au repos, ou jusqu'à la fin de la partie.
std::uint32_t RunUntilQuiet(Match3Game& game, std::uint32_t now_ms) {
  for (int tick = 0; tick < kMostTicks; ++tick) {
    if (game.phase() == Match3Phase::kIdle || game.phase() == Match3Phase::kOver) {
      break;
    }
    now_ms += ro::sim::kMatch3SwapMs;
    game.Advance(now_ms);
  }
  return now_ms;
}

}  // namespace

TEST_CASE("un échange qui aligne efface, coûte un coup et rapporte") {
  Match3Game game(kSeed);
  game.SetCells(GridWithMove(), 0);
  game.Click({0, 2}, 0);
  game.Click({1, 2}, 0);
  CHECK(game.phase() == Match3Phase::kSwapping);
  game.Advance(ro::sim::kMatch3SwapMs);
  CHECK(game.phase() == Match3Phase::kFalling);
  CHECK(game.moves_left() == ro::sim::kMatch3Moves - 1);
  CHECK(game.score() >= 3 * ro::sim::kMatch3GemPoints);
  CHECK(game.bursts().size() >= 3);
  CHECK(game.TakeNews().bursts == 1);
  RunUntilQuiet(game, ro::sim::kMatch3SwapMs);
  CHECK(game.phase() == Match3Phase::kIdle);
  CHECK(game.combo() == 0);
}

TEST_CASE("un échange qui n'aligne rien se défait sans coûter de coup") {
  Match3Game game(kSeed);
  game.SetCells(PlainGrid(), 0);
  game.Click({7, 0}, 0);
  game.Click({7, 1}, 0);
  game.Advance(ro::sim::kMatch3SwapMs);
  CHECK(game.phase() == Match3Phase::kReverting);
  CHECK(game.KindAt({7, 0}) == PlainGrid()[7 * kMatch3Side]);
  CHECK(game.moves_left() == ro::sim::kMatch3Moves);
  CHECK(game.TakeNews().refused_swaps == 1);
  game.Advance(2 * ro::sim::kMatch3SwapMs);
  CHECK(game.phase() == Match3Phase::kIdle);
}

TEST_CASE("une case lointaine change le choix au lieu d'échanger") {
  Match3Game game(kSeed);
  game.SetCells(PlainGrid(), 0);
  game.Click({0, 0}, 0);
  game.Click({3, 3}, 0);
  CHECK(game.phase() == Match3Phase::kIdle);
  REQUIRE(game.selected().has_value());
  CHECK(*game.selected() == Match3Cell{3, 3});
  game.Click({3, 3}, 0);
  CHECK_FALSE(game.selected().has_value());
}

TEST_CASE("les deux gemmes glissent l'une vers l'autre") {
  Match3Game game(kSeed);
  game.SetCells(GridWithMove(), 0);
  game.Click({0, 2}, 0);
  game.Click({1, 2}, 0);
  const auto offset = game.OffsetAt({0, 2}, ro::sim::kMatch3SwapMs / 2);
  CHECK(offset.rows == Catch::Approx(0.5F));
  CHECK(offset.columns == Catch::Approx(0.0F));
  CHECK(game.OffsetAt({5, 5}, ro::sim::kMatch3SwapMs / 2).rows == Catch::Approx(0.0F));
}

TEST_CASE("l'indice paraît après un silence") {
  Match3Game game(kSeed);
  game.SetCells(GridWithMove(), 0);
  CHECK_FALSE(game.Hint(ro::sim::kMatch3HintAfterMs - 1).has_value());
  CHECK(game.Hint(ro::sim::kMatch3HintAfterMs).has_value());
}

TEST_CASE("une grille neuve n'a aucun alignement et offre un échange") {
  for (std::uint32_t seed = 0; seed < kNewGameSeeds; ++seed) {
    INFO("graine " << seed);
    Match3Game game(seed);
    game.NewGame(0);
    CHECK_FALSE(HasRun(game));
    CHECK(game.FindMove().has_value());
  }
}

// ⚠ UNE SEULE ESPÈCE : tout remélange aligne, et seul le repli rend cette
// grille jouable, quel que soit le générateur.
TEST_CASE("le remélange de repli rend une grille jouable") {
  Match3Game game(kSeed);
  game.SetCells(std::array<int, kMatch3Cells>{}, 0);
  game.Reshuffle();
  CHECK_FALSE(HasRun(game));
  CHECK(game.FindMove().has_value());
}

TEST_CASE("une partie neuve se joue jusqu'au dernier coup") {
  Match3Game game(kSeed);
  game.NewGame(0);
  std::uint32_t now_ms = 0;
  bool finished = false;
  for (int tick = 0; tick < kMostTicks && game.phase() != Match3Phase::kOver; ++tick) {
    if (game.phase() == Match3Phase::kIdle) {
      // ⚠ JAMAIS BLOQUÉE : il y a toujours un coup à jouer.
      const auto move = game.FindMove();
      REQUIRE(move.has_value());
      game.Click((*move)[0], now_ms);
      game.Click((*move)[1], now_ms);
    }
    now_ms += ro::sim::kMatch3SwapMs;
    game.Advance(now_ms);
    finished = finished || game.TakeNews().finished;
  }
  CHECK(game.phase() == Match3Phase::kOver);
  CHECK(game.moves_left() == 0);
  CHECK(finished);
  CHECK(game.score() >= ro::sim::kMatch3Moves * 3 * ro::sim::kMatch3GemPoints);
  CHECK(game.best_combo() >= 1);
}

TEST_CASE("un pas de cascade dit ses points au milieu de ce qu'il efface") {
  Match3Game game(kSeed);
  game.SetCells(GridWithMove(), 0);
  game.Click({0, 2}, 0);
  game.Click({1, 2}, 0);
  game.Advance(ro::sim::kMatch3SwapMs);
  REQUIRE(game.gains().size() == 1);
  const ro::sim::Match3Gain& gain = game.gains().front();
  CHECK(gain.points == game.score());
  CHECK(gain.combo == 1);
  CHECK(gain.row == Catch::Approx(0.0F));
  CHECK(gain.column == Catch::Approx(1.0F));
  game.Advance(ro::sim::kMatch3SwapMs + ro::sim::kMatch3GainMs + 1);
  CHECK(game.gains().empty());
}

TEST_CASE("une gemme tombe en accélérant, puis rebondit") {
  Match3Game game(kSeed);
  game.SetCells(GridWithMove(), 0);
  game.Click({0, 2}, 0);
  game.Click({1, 2}, 0);
  const std::uint32_t fall_started_ms = ro::sim::kMatch3SwapMs;
  game.Advance(fall_started_ms);
  REQUIRE(game.phase() == Match3Phase::kFalling);
  // La gemme neuve de (0,2) vient d'une rangée au-dessus : à mi-temps, elle
  // n'a parcouru que le quart du chemin.
  const std::uint32_t one_row_ms = ro::sim::kMatch3FallRowMs;
  CHECK(game.OffsetAt({0, 2}, fall_started_ms).rows == Catch::Approx(-1.0F));
  CHECK(game.OffsetAt({0, 2}, fall_started_ms + one_row_ms / 2).rows == Catch::Approx(-0.75F));
  CHECK(game.OffsetAt({0, 2}, fall_started_ms + one_row_ms + ro::sim::kMatch3LandingMs / 2).rows ==
        Catch::Approx(-ro::sim::kMatch3LandingRows));
  // Une gemme qui n'a pas bougé ne rebondit pas.
  CHECK(game.OffsetAt({5, 5}, fall_started_ms + one_row_ms).rows == Catch::Approx(0.0F));
}

TEST_CASE("la cascade reste dite un instant après le repos") {
  Match3Game game(kSeed);
  game.SetCells(GridWithMove(), 0);
  game.Click({0, 2}, 0);
  game.Click({1, 2}, 0);
  // ⚠ LE REPOS SE PREND À LA FIN DES CHUTES : une grille bloquée passe ensuite
  // par un remélange, plus long que le temps où la cascade reste dite.
  std::uint32_t settled_ms = ro::sim::kMatch3SwapMs;
  for (int tick = 0; tick < kMostTicks; ++tick) {
    if (game.phase() != Match3Phase::kSwapping && game.phase() != Match3Phase::kFalling) {
      break;
    }
    settled_ms += ro::sim::kMatch3SwapMs;
    game.Advance(settled_ms);
  }
  REQUIRE(game.combo() == 0);
  REQUIRE(game.best_combo() >= 1);
  CHECK(game.ShownCombo(settled_ms) == game.best_combo());
  CHECK(game.ShownCombo(settled_ms + ro::sim::kMatch3ComboLingerMs) == 0);
}

TEST_CASE("l'indice est un échange possible, le même jusqu'au prochain coup") {
  Match3Game game(kSeed);
  game.NewGame(0);
  const auto hint = game.Hint(ro::sim::kMatch3HintAfterMs);
  REQUIRE(hint.has_value());
  CHECK(game.Hint(2 * ro::sim::kMatch3HintAfterMs) == hint);
  const auto moves = game.AllMoves();
  CHECK(std::find(moves.begin(), moves.end(), *hint) != moves.end());
}

TEST_CASE("l'indice ne désigne pas toujours le premier échange de la grille") {
  bool varied = false;
  for (std::uint32_t seed = 0; seed < kNewGameSeeds && !varied; ++seed) {
    Match3Game game(seed);
    game.NewGame(0);
    varied = game.Hint(ro::sim::kMatch3HintAfterMs) != game.FindMove();
  }
  CHECK(varied);
}

// ── LES GEMMES SPÉCIALES ─────────────────────────────────────────────────────

namespace {

using ro::sim::Match3Special;

constexpr int kCellsPerLine = kMatch3Side;

void Put(std::array<int, kMatch3Cells>& kinds, Match3Cell cell, int kind) {
  kinds[static_cast<std::size_t>(cell.row * kMatch3Side + cell.column)] = kind;
}

// Joue l'échange de `first` et `second`, jusqu'au premier effacement.
void PlaySwap(Match3Game& game, Match3Cell first, Match3Cell second) {
  game.Click(first, 0);
  game.Click(second, 0);
  game.Advance(ro::sim::kMatch3SwapMs);
}

// La grille simple, où échanger (0,2) et (1,2) aligne des « 4 » en haut, de
// la colonne 0 à la colonne `last_column`.
std::array<int, kMatch3Cells> GridWithRowTo(int last_column) {
  std::array<int, kMatch3Cells> kinds = PlainGrid();
  for (int column = 0; column <= last_column; ++column) {
    if (column != 2) {
      Put(kinds, {0, column}, 4);
    }
  }
  // La case qui suit l'alignement ne le prolonge pas.
  if (last_column + 1 < kMatch3Side && kinds[static_cast<std::size_t>(last_column + 1)] == 4) {
    Put(kinds, {0, last_column + 1}, 5);
  }
  return kinds;
}

}  // namespace

TEST_CASE("quatre alignés laissent une gemme de ligne sur la case déplacée") {
  Match3Game game(kSeed);
  game.SetCells(GridWithRowTo(3), 0);
  PlaySwap(game, {0, 2}, {1, 2});
  REQUIRE(game.phase() == Match3Phase::kFalling);
  CHECK(game.SpecialAt({0, 2}) == Match3Special::kRowBlast);
  CHECK(game.KindAt({0, 2}) == 4);
  CHECK(game.TakeNews().specials_made == 1);
}

TEST_CASE("cinq alignés laissent une étoile, qui n'a plus d'espèce") {
  Match3Game game(kSeed);
  game.SetCells(GridWithRowTo(4), 0);
  PlaySwap(game, {0, 2}, {1, 2});
  REQUIRE(game.phase() == Match3Phase::kFalling);
  CHECK(game.SpecialAt({0, 2}) == Match3Special::kStar);
  CHECK(game.KindAt({0, 2}) == ro::sim::kMatch3StarKind);
}

TEST_CASE("un L laisse une bombe au croisement") {
  std::array<int, kMatch3Cells> kinds = PlainGrid();
  // La rangée 2 porte « 4 4 · 4 », la colonne 2 porte « 4 4 · » : glisser le
  // « 4 » de (2,3) en (2,2) ferme les deux à la fois.
  Put(kinds, {0, 2}, 4);
  Put(kinds, {2, 1}, 4);
  Put(kinds, {2, 3}, 4);
  Match3Game game(kSeed);
  game.SetCells(kinds, 0);
  PlaySwap(game, {2, 3}, {2, 2});
  REQUIRE(game.phase() == Match3Phase::kFalling);
  CHECK(game.SpecialAt({2, 2}) == Match3Special::kBomb);
  CHECK(game.KindAt({2, 2}) == 4);
}

TEST_CASE("une gemme de ligne effacée souffle toute sa colonne") {
  Match3Game game(kSeed);
  game.SetCells(GridWithMove(), 0);
  game.SetSpecial({0, 0}, Match3Special::kColumnBlast);
  PlaySwap(game, {0, 2}, {1, 2});
  // Les trois alignées, et les sept autres de la colonne 0.
  CHECK(game.bursts().size() == 3 + (kCellsPerLine - 1));
  CHECK(game.blasts().size() == 1);
  CHECK(game.TakeNews().line_blasts == 1);
  CHECK(game.score() == (static_cast<int>(game.bursts().size()) * ro::sim::kMatch3GemPoints +
                         ro::sim::kMatch3BlastPoints));
}

TEST_CASE("un souffle qui touche une gemme spéciale la fait souffler à son tour") {
  Match3Game game(kSeed);
  game.SetCells(GridWithMove(), 0);
  game.SetSpecial({0, 0}, Match3Special::kColumnBlast);
  game.SetSpecial({5, 0}, Match3Special::kBomb);
  PlaySwap(game, {0, 2}, {1, 2});
  // La colonne 0 entière, puis les trois cases de la colonne 1 autour de la
  // bombe (rangées 4 à 6).
  CHECK(game.bursts().size() == 3 + (kCellsPerLine - 1) + 3);
  const ro::sim::Match3News news = game.TakeNews();
  CHECK(news.line_blasts == 1);
  CHECK(news.bomb_blasts == 1);
}

TEST_CASE("une étoile échangée efface l'espèce de sa voisine") {
  std::array<int, kMatch3Cells> kinds = PlainGrid();
  const int target = kinds[7 * kMatch3Side + 1];
  const auto same_kind = std::ranges::count(kinds, target);
  Match3Game game(kSeed);
  game.SetCells(kinds, 0);
  game.SetSpecial({7, 0}, Match3Special::kStar);
  // ⚠ LA GRILLE SIMPLE N'A AUCUN ÉCHANGE : seule l'étoile en offre.
  REQUIRE(game.FindMove().has_value());
  PlaySwap(game, {7, 0}, {7, 1});
  REQUIRE(game.phase() == Match3Phase::kFalling);
  CHECK(game.moves_left() == ro::sim::kMatch3Moves - 1);
  CHECK(static_cast<long long>(game.bursts().size()) == same_kind + 1);
  CHECK(game.TakeNews().star_blasts == 1);
}

TEST_CASE("deux étoiles échangées vident la grille") {
  Match3Game game(kSeed);
  game.SetCells(PlainGrid(), 0);
  game.SetSpecial({7, 0}, Match3Special::kStar);
  game.SetSpecial({7, 1}, Match3Special::kStar);
  PlaySwap(game, {7, 0}, {7, 1});
  CHECK(game.bursts().size() == static_cast<std::size_t>(kMatch3Cells));
  CHECK(game.TakeNews().star_blasts == 2);
}

TEST_CASE("une gemme spéciale tombe avec sa gemme") {
  // La rangée 2 porte « 4 4 · » : glisser le « 4 » de (1,2) en (2,2) l'efface,
  // et la colonne 0 descend d'une rangée.
  std::array<int, kMatch3Cells> kinds = PlainGrid();
  Put(kinds, {2, 1}, 4);
  Match3Game game(kSeed);
  game.SetCells(kinds, 0);
  game.SetSpecial({0, 0}, Match3Special::kBomb);
  PlaySwap(game, {2, 2}, {1, 2});
  REQUIRE(game.phase() == Match3Phase::kFalling);
  CHECK(game.SpecialAt({1, 0}) == Match3Special::kBomb);
  CHECK(game.KindAt({1, 0}) == PlainGrid()[0]);
  CHECK(game.SpecialAt({0, 0}) == Match3Special::kNone);
}

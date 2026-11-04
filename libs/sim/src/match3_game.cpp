#include "ro/sim/match3_game.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>
#include <utility>

namespace ro::sim {
namespace {

using Cells = std::array<int, kMatch3Cells>;
using Marks = std::array<bool, kMatch3Cells>;
using Move = std::array<Match3Cell, 2>;

// Les remélanges tentés avant de repartir d'une grille neuve.
constexpr int kMostReshuffles = 400;

[[nodiscard]] int IndexOf(Match3Cell cell) { return cell.row * kMatch3Side + cell.column; }

[[nodiscard]] bool Inside(Match3Cell cell) {
  return cell.row >= 0 && cell.row < kMatch3Side && cell.column >= 0 && cell.column < kMatch3Side;
}

[[nodiscard]] bool Neighbours(Match3Cell first, Match3Cell second) {
  return std::abs(first.row - second.row) + std::abs(first.column - second.column) == 1;
}

[[nodiscard]] int KindIn(const Cells& cells, Match3Cell cell) {
  return Inside(cell) ? cells[static_cast<std::size_t>(IndexOf(cell))] : kMatch3Empty;
}

// Marque les alignements de `cells` ; rend le nombre de gemmes marquées et,
// dans `long_bonus`, les gemmes au-delà de trois.
[[nodiscard]] int MarkMatches(const Cells& cells, Marks& marks, int& long_bonus) {
  marks = {};
  long_bonus = 0;
  int marked = 0;
  // Une passe par sens : `along` avance dans la ligne, `across` choisit la ligne.
  for (const bool horizontal : {true, false}) {
    for (int across = 0; across < kMatch3Side; ++across) {
      const auto cell = [&](int at) {
        return horizontal ? Match3Cell{across, at} : Match3Cell{at, across};
      };
      int run = 1;
      for (int along = 1; along <= kMatch3Side; ++along) {
        const int kind = along < kMatch3Side ? KindIn(cells, cell(along)) : kMatch3Empty;
        if (kind != kMatch3Empty && kind == KindIn(cells, cell(along - 1))) {
          ++run;
          continue;
        }
        if (run >= kMatch3ShortestRun) {
          long_bonus += run - kMatch3ShortestRun;
          for (int at = along - run; at < along; ++at) {
            bool& mark = marks[static_cast<std::size_t>(IndexOf(cell(at)))];
            if (!mark) {
              mark = true;
              ++marked;
            }
          }
        }
        run = 1;
      }
    }
  }
  return marked;
}

// Les échanges possibles dans `cells` ; le premier seul quand `first_only`.
[[nodiscard]] std::vector<Move> MovesIn(Cells cells, bool first_only) {
  std::vector<Move> moves;
  Marks marks;
  int long_bonus = 0;
  const auto swap = [&cells](Match3Cell first, Match3Cell second) {
    std::swap(cells[static_cast<std::size_t>(IndexOf(first))],
              cells[static_cast<std::size_t>(IndexOf(second))]);
  };
  for (int row = 0; row < kMatch3Side; ++row) {
    for (int column = 0; column < kMatch3Side; ++column) {
      const Match3Cell here{row, column};
      for (const Match3Cell there : {Match3Cell{row, column + 1}, Match3Cell{row + 1, column}}) {
        if (!Inside(there)) {
          continue;
        }
        swap(here, there);
        const bool aligns = MarkMatches(cells, marks, long_bonus) > 0;
        swap(here, there);
        if (aligns) {
          moves.push_back(Move{here, there});
          if (first_only) {
            return moves;
          }
        }
      }
    }
  }
  return moves;
}

// L'avancée d'une animation de `duration_ms`, de 0 à 1.
[[nodiscard]] float Progress(std::uint32_t started_ms, std::uint32_t now_ms,
                             std::uint32_t duration_ms) {
  const std::uint32_t elapsed_ms = now_ms - started_ms;
  return elapsed_ms >= duration_ms
             ? 1.0F
             : static_cast<float>(elapsed_ms) / static_cast<float>(duration_ms);
}

// La durée d'une chute de `rows` rangées.
[[nodiscard]] std::uint32_t FallMs(int rows) {
  return rows <= 0 ? 0
                   : static_cast<std::uint32_t>(std::lround(static_cast<float>(kMatch3FallRowMs) *
                                                            std::sqrt(static_cast<float>(rows))));
}

}  // namespace

int& Match3Game::At(Match3Cell cell) { return cells_[static_cast<std::size_t>(IndexOf(cell))]; }

int Match3Game::KindAt(Match3Cell cell) const { return KindIn(cells_, cell); }

int Match3Game::RandomKind() {
  return std::uniform_int_distribution<int>(0, kMatch3Kinds - 1)(random_);
}

void Match3Game::NewGame(std::uint32_t now_ms) {
  FillWithoutMatches();
  if (!FindMove().has_value()) {
    Reshuffle();
  }
  SetCells(cells_, now_ms);
}

void Match3Game::FillWithoutMatches() {
  // Chaque gemme évite de compléter les deux précédentes de sa rangée et de sa
  // colonne.
  for (int row = 0; row < kMatch3Side; ++row) {
    for (int column = 0; column < kMatch3Side; ++column) {
      int kind = 0;
      do {
        kind = RandomKind();
      } while (
          (column >= 2 && KindAt({row, column - 1}) == kind && KindAt({row, column - 2}) == kind) ||
          (row >= 2 && KindAt({row - 1, column}) == kind && KindAt({row - 2, column}) == kind));
      At({row, column}) = kind;
    }
  }
}

void Match3Game::SetCells(const std::array<int, kMatch3Cells>& kinds, std::uint32_t now_ms) {
  cells_ = kinds;
  fallen_rows_ = {};
  longest_fall_ms_ = 0;
  started_ = true;
  phase_ = Match3Phase::kIdle;
  score_ = 0;
  moves_left_ = kMatch3Moves;
  combo_ = 0;
  best_combo_ = 0;
  last_combo_ = 0;
  settled_ms_ = now_ms;
  selected_.reset();
  bursts_.clear();
  gains_.clear();
  news_ = {};
  phase_started_ms_ = now_ms;
  last_action_ms_ = now_ms;
  ChooseHint();
}

int Match3Game::FindMatches(Marks& marks, int& long_bonus) const {
  return MarkMatches(cells_, marks, long_bonus);
}

std::optional<std::array<Match3Cell, 2>> Match3Game::FindMove() const {
  const std::vector<Move> moves = MovesIn(cells_, true);
  if (moves.empty()) {
    return std::nullopt;
  }
  return moves.front();
}

std::vector<std::array<Match3Cell, 2>> Match3Game::AllMoves() const {
  return MovesIn(cells_, false);
}

void Match3Game::ChooseHint() {
  const std::vector<Move> moves = AllMoves();
  if (moves.empty()) {
    hint_.reset();
    return;
  }
  hint_ = moves[std::uniform_int_distribution<std::size_t>(0, moves.size() - 1)(hint_random_)];
}

void Match3Game::Swap(Match3Cell first, Match3Cell second) { std::swap(At(first), At(second)); }

void Match3Game::Reshuffle() {
  Marks marks;
  int long_bonus = 0;
  for (int attempt = 0; attempt < kMostReshuffles; ++attempt) {
    std::shuffle(cells_.begin(), cells_.end(), random_);
    if (FindMatches(marks, long_bonus) == 0 && FindMove().has_value()) {
      return;
    }
  }
  // ⚠ MALCHANCE PERSISTANTE : une grille neuve, tirée sans alignement comme au
  // début d'une partie, jusqu'à ce qu'elle offre un échange. Chaque tirage
  // réussit presque toujours.
  do {
    FillWithoutMatches();
  } while (!FindMove().has_value());
}

void Match3Game::Click(Match3Cell cell, std::uint32_t now_ms) {
  if (phase_ != Match3Phase::kIdle || !Inside(cell)) {
    return;
  }
  last_action_ms_ = now_ms;
  if (!selected_.has_value()) {
    selected_ = cell;
    return;
  }
  if (*selected_ == cell) {
    selected_.reset();
    return;
  }
  if (!Neighbours(*selected_, cell)) {
    selected_ = cell;
    return;
  }
  swap_first_ = *selected_;
  swap_second_ = cell;
  selected_.reset();
  Swap(swap_first_, swap_second_);
  phase_ = Match3Phase::kSwapping;
  phase_started_ms_ = now_ms;
}

bool Match3Game::ClearStep(std::uint32_t now_ms) {
  Marks marks;
  int long_bonus = 0;
  const int marked = FindMatches(marks, long_bonus);
  if (marked == 0) {
    return false;
  }
  ++combo_;
  best_combo_ = std::max(best_combo_, combo_);
  const int points = (marked * kMatch3GemPoints + long_bonus * kMatch3LongRunPoints) * combo_;
  score_ += points;
  ++news_.bursts;
  float row_sum = 0.0F;
  float column_sum = 0.0F;
  for (int index = 0; index < kMatch3Cells; ++index) {
    if (marks[static_cast<std::size_t>(index)]) {
      const Match3Cell cell{index / kMatch3Side, index % kMatch3Side};
      bursts_.push_back(Match3Burst{.cell = cell, .kind = At(cell), .started_ms = now_ms});
      row_sum += static_cast<float>(cell.row);
      column_sum += static_cast<float>(cell.column);
      At(cell) = kMatch3Empty;
    }
  }
  gains_.push_back(Match3Gain{.row = row_sum / static_cast<float>(marked),
                              .column = column_sum / static_cast<float>(marked),
                              .points = points,
                              .combo = combo_,
                              .started_ms = now_ms});
  // Chaque colonne tombe, du bas vers le haut ; ce qui manque vient d'au-dessus
  // de la grille, d'aussi haut qu'il y avait de trous.
  fallen_rows_ = {};
  int longest_fall = 0;
  for (int column = 0; column < kMatch3Side; ++column) {
    int landing = kMatch3Side - 1;
    for (int row = kMatch3Side - 1; row >= 0; --row) {
      const int kind = At({row, column});
      if (kind == kMatch3Empty) {
        continue;
      }
      At({landing, column}) = kind;
      fallen_rows_[static_cast<std::size_t>(IndexOf({landing, column}))] = landing - row;
      longest_fall = std::max(longest_fall, landing - row);
      --landing;
    }
    const int holes = landing + 1;
    longest_fall = std::max(longest_fall, holes);
    for (int row = landing; row >= 0; --row) {
      At({row, column}) = RandomKind();
      fallen_rows_[static_cast<std::size_t>(IndexOf({row, column}))] = holes;
    }
  }
  longest_fall_ms_ = FallMs(longest_fall);
  return true;
}

void Match3Game::Settle(std::uint32_t now_ms) {
  last_combo_ = combo_;
  settled_ms_ = now_ms;
  combo_ = 0;
  fallen_rows_ = {};
  phase_started_ms_ = now_ms;
  last_action_ms_ = now_ms;
  if (moves_left_ <= 0) {
    phase_ = Match3Phase::kOver;
    news_.finished = true;
    return;
  }
  if (FindMove().has_value()) {
    phase_ = Match3Phase::kIdle;
  } else {
    Reshuffle();
    news_.reshuffled = true;
    phase_ = Match3Phase::kReshuffling;
  }
  ChooseHint();
}

void Match3Game::Advance(std::uint32_t now_ms) {
  std::erase_if(bursts_, [now_ms](const Match3Burst& burst) {
    return now_ms - burst.started_ms > kMatch3BurstMs;
  });
  std::erase_if(gains_, [now_ms](const Match3Gain& gain) {
    return now_ms - gain.started_ms > kMatch3GainMs;
  });
  switch (phase_) {
    case Match3Phase::kIdle:
    case Match3Phase::kOver:
      return;
    case Match3Phase::kSwapping:
      if (now_ms - phase_started_ms_ < kMatch3SwapMs) {
        return;
      }
      if (ClearStep(now_ms)) {
        --moves_left_;
        phase_ = Match3Phase::kFalling;
      } else {
        Swap(swap_first_, swap_second_);
        ++news_.refused_swaps;
        phase_ = Match3Phase::kReverting;
      }
      phase_started_ms_ = now_ms;
      return;
    case Match3Phase::kReverting:
      if (now_ms - phase_started_ms_ >= kMatch3SwapMs) {
        phase_ = Match3Phase::kIdle;
        last_action_ms_ = now_ms;
      }
      return;
    case Match3Phase::kFalling:
      if (now_ms - phase_started_ms_ < longest_fall_ms_ + kMatch3LandingMs) {
        return;
      }
      if (ClearStep(now_ms)) {
        phase_started_ms_ = now_ms;
      } else {
        Settle(now_ms);
      }
      return;
    case Match3Phase::kReshuffling:
      if (now_ms - phase_started_ms_ >= kMatch3ReshuffleMs) {
        phase_ = Match3Phase::kIdle;
        last_action_ms_ = now_ms;
      }
      return;
  }
}

Match3News Match3Game::TakeNews() noexcept { return std::exchange(news_, Match3News{}); }

int Match3Game::ShownCombo(std::uint32_t now_ms) const {
  if (combo_ > 0) {
    return combo_;
  }
  return now_ms - settled_ms_ < kMatch3ComboLingerMs ? last_combo_ : 0;
}

Match3Game::Offset Match3Game::OffsetAt(Match3Cell cell, std::uint32_t now_ms) const {
  if (!Inside(cell)) {
    return {};
  }
  if (phase_ == Match3Phase::kFalling) {
    const int fallen = fallen_rows_[static_cast<std::size_t>(IndexOf(cell))];
    if (fallen == 0) {
      return {};
    }
    const std::uint32_t fall_ms = FallMs(fallen);
    const std::uint32_t elapsed_ms = now_ms - phase_started_ms_;
    if (elapsed_ms < fall_ms) {
      // Le chemin parcouru croît comme le carré du temps.
      const float done = Progress(phase_started_ms_, now_ms, fall_ms);
      return Offset{.rows = -static_cast<float>(fallen) * (1.0F - done * done), .columns = 0.0F};
    }
    const float landed = Progress(phase_started_ms_ + fall_ms, now_ms, kMatch3LandingMs);
    return Offset{.rows = -kMatch3LandingRows * std::sin(std::numbers::pi_v<float> * landed),
                  .columns = 0.0F};
  }
  if (phase_ != Match3Phase::kSwapping && phase_ != Match3Phase::kReverting) {
    return {};
  }
  // La gemme vient de la place de l'autre.
  Match3Cell from;
  if (cell == swap_first_) {
    from = swap_second_;
  } else if (cell == swap_second_) {
    from = swap_first_;
  } else {
    return {};
  }
  const float left = 1.0F - Progress(phase_started_ms_, now_ms, kMatch3SwapMs);
  return Offset{.rows = static_cast<float>(from.row - cell.row) * left,
                .columns = static_cast<float>(from.column - cell.column) * left};
}

float Match3Game::GemScale(std::uint32_t now_ms) const {
  if (phase_ != Match3Phase::kReshuffling) {
    return 1.0F;
  }
  const float done = Progress(phase_started_ms_, now_ms, kMatch3ReshuffleMs);
  // Elle s'élance vite et ralentit en arrivant à sa taille.
  return 1.0F - (1.0F - done) * (1.0F - done);
}

std::optional<std::array<Match3Cell, 2>> Match3Game::Hint(std::uint32_t now_ms) const {
  if (phase_ != Match3Phase::kIdle || now_ms - last_action_ms_ < kMatch3HintAfterMs) {
    return std::nullopt;
  }
  return hint_;
}

}  // namespace ro::sim

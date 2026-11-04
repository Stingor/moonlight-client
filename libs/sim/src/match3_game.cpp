#include "ro/sim/match3_game.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <numbers>
#include <numeric>
#include <utility>

namespace ro::sim {
namespace {

using Cells = std::array<int, kMatch3Cells>;
using Marks = std::array<bool, kMatch3Cells>;
using Move = std::array<Match3Cell, 2>;

// Les remélanges tentés avant de repartir d'une grille neuve.
constexpr int kMostReshuffles = 400;
// Le rayon du carré que souffle une bombe, en cases.
constexpr int kBombReach = 1;

[[nodiscard]] int IndexOf(Match3Cell cell) { return cell.row * kMatch3Side + cell.column; }

[[nodiscard]] std::size_t SlotOf(Match3Cell cell) {
  return static_cast<std::size_t>(IndexOf(cell));
}

[[nodiscard]] Match3Cell CellAt(int index) {
  return Match3Cell{index / kMatch3Side, index % kMatch3Side};
}

[[nodiscard]] bool Inside(Match3Cell cell) {
  return cell.row >= 0 && cell.row < kMatch3Side && cell.column >= 0 && cell.column < kMatch3Side;
}

[[nodiscard]] bool Neighbours(Match3Cell first, Match3Cell second) {
  return std::abs(first.row - second.row) + std::abs(first.column - second.column) == 1;
}

[[nodiscard]] int KindIn(const Cells& cells, Match3Cell cell) {
  return Inside(cell) ? cells[SlotOf(cell)] : kMatch3Empty;
}

// Une espèce qui s'aligne : ni case vide, ni étoile.
[[nodiscard]] bool Alignable(int kind) { return kind >= 0 && kind < kMatch3Kinds; }

// UN ALIGNEMENT : `length` gemmes de `kind` depuis `start`, dans la rangée
// `across` s'il est horizontal, dans la colonne `across` sinon.
struct Run {
  bool horizontal = true;
  int across = 0;
  int start = 0;
  int length = 0;
  int kind = 0;

  [[nodiscard]] Match3Cell CellOf(int at) const {
    return horizontal ? Match3Cell{across, at} : Match3Cell{at, across};
  }
  [[nodiscard]] bool Holds(Match3Cell cell) const {
    const int along = horizontal ? cell.column : cell.row;
    const int line = horizontal ? cell.row : cell.column;
    return line == across && along >= start && along < start + length;
  }
};

[[nodiscard]] std::vector<Run> FindRuns(const Cells& cells) {
  std::vector<Run> runs;
  // Une passe par sens : `along` avance dans la ligne, `across` choisit la ligne.
  for (const bool horizontal : {true, false}) {
    for (int across = 0; across < kMatch3Side; ++across) {
      Run line{.horizontal = horizontal, .across = across};
      int run = 1;
      for (int along = 1; along <= kMatch3Side; ++along) {
        const int previous = KindIn(cells, line.CellOf(along - 1));
        const int kind = along < kMatch3Side ? KindIn(cells, line.CellOf(along)) : kMatch3Empty;
        if (Alignable(kind) && kind == previous) {
          ++run;
          continue;
        }
        if (run >= kMatch3ShortestRun && Alignable(previous)) {
          runs.push_back(Run{.horizontal = horizontal,
                             .across = across,
                             .start = along - run,
                             .length = run,
                             .kind = previous});
        }
        run = 1;
      }
    }
  }
  return runs;
}

// Marque les gemmes de `runs` ; rend leur nombre et, dans `long_bonus`, les
// gemmes au-delà de trois.
int MarkRuns(const std::vector<Run>& runs, Marks& marks, int& long_bonus) {
  marks = {};
  long_bonus = 0;
  int marked = 0;
  for (const Run& run : runs) {
    long_bonus += run.length - kMatch3ShortestRun;
    for (int at = run.start; at < run.start + run.length; ++at) {
      bool& mark = marks[SlotOf(run.CellOf(at))];
      if (!mark) {
        mark = true;
        ++marked;
      }
    }
  }
  return marked;
}

// Les échanges possibles dans `cells` ; le premier seul quand `first_only`.
// ⚠ Un échange qui touche une étoile tient toujours.
[[nodiscard]] std::vector<Move> MovesIn(Cells cells, bool first_only) {
  std::vector<Move> moves;
  const auto swap = [&cells](Match3Cell first, Match3Cell second) {
    std::swap(cells[SlotOf(first)], cells[SlotOf(second)]);
  };
  for (int row = 0; row < kMatch3Side; ++row) {
    for (int column = 0; column < kMatch3Side; ++column) {
      const Match3Cell here{row, column};
      for (const Match3Cell there : {Match3Cell{row, column + 1}, Match3Cell{row + 1, column}}) {
        if (!Inside(there)) {
          continue;
        }
        bool works =
            KindIn(cells, here) == kMatch3StarKind || KindIn(cells, there) == kMatch3StarKind;
        if (!works) {
          swap(here, there);
          works = !FindRuns(cells).empty();
          swap(here, there);
        }
        if (works) {
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

int& Match3Game::At(Match3Cell cell) { return cells_[SlotOf(cell)]; }

Match3Special& Match3Game::SpecialRef(Match3Cell cell) { return specials_[SlotOf(cell)]; }

int Match3Game::KindAt(Match3Cell cell) const { return KindIn(cells_, cell); }

Match3Special Match3Game::SpecialAt(Match3Cell cell) const {
  return Inside(cell) ? specials_[SlotOf(cell)] : Match3Special::kNone;
}

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
  specials_ = {};
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
  specials_ = {};
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
  blasts_.clear();
  news_ = {};
  phase_started_ms_ = now_ms;
  last_action_ms_ = now_ms;
  ChooseHint();
}

void Match3Game::SetSpecial(Match3Cell cell, Match3Special special) {
  if (!Inside(cell)) {
    return;
  }
  SpecialRef(cell) = special;
  if (special == Match3Special::kStar) {
    At(cell) = kMatch3StarKind;
  }
  ChooseHint();
}

int Match3Game::FindMatches(Marks& marks, int& long_bonus) const {
  return MarkRuns(FindRuns(cells_), marks, long_bonus);
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

void Match3Game::Swap(Match3Cell first, Match3Cell second) {
  std::swap(At(first), At(second));
  std::swap(SpecialRef(first), SpecialRef(second));
}

void Match3Game::Reshuffle() {
  Marks marks;
  int long_bonus = 0;
  // ⚠ UNE GEMME SPÉCIALE SE REMÉLANGE AVEC SON ESPÈCE : on mélange les places,
  // pas les deux tableaux chacun de son côté.
  std::array<int, kMatch3Cells> order = {};
  for (int attempt = 0; attempt < kMostReshuffles; ++attempt) {
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), random_);
    const Cells kinds = cells_;
    const std::array<Match3Special, kMatch3Cells> specials = specials_;
    for (std::size_t slot = 0; slot < order.size(); ++slot) {
      cells_[slot] = kinds[static_cast<std::size_t>(order[slot])];
      specials_[slot] = specials[static_cast<std::size_t>(order[slot])];
    }
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

int Match3Game::MostCommonKind() const {
  std::array<int, kMatch3Kinds> counts = {};
  for (const int kind : cells_) {
    if (Alignable(kind)) {
      ++counts[static_cast<std::size_t>(kind)];
    }
  }
  return static_cast<int>(std::distance(counts.begin(), std::ranges::max_element(counts)));
}

int Match3Game::Detonate(Marks& marks, Marks& fired, std::uint32_t now_ms) {
  int blasts = 0;
  bool spread = true;
  while (spread) {
    spread = false;
    for (int index = 0; index < kMatch3Cells; ++index) {
      const auto slot = static_cast<std::size_t>(index);
      if (!marks[slot] || fired[slot] || specials_[slot] == Match3Special::kNone) {
        continue;
      }
      fired[slot] = true;
      spread = true;
      ++blasts;
      const Match3Cell here = CellAt(index);
      const Match3Special special = specials_[slot];
      blasts_.push_back(Match3Blast{
          .cell = here, .special = special, .kind = cells_[slot], .started_ms = now_ms});
      const auto mark = [&marks](Match3Cell cell) {
        if (Inside(cell)) {
          marks[SlotOf(cell)] = true;
        }
      };
      switch (special) {
        case Match3Special::kNone:
          break;
        case Match3Special::kRowBlast:
          ++news_.line_blasts;
          for (int column = 0; column < kMatch3Side; ++column) {
            mark({here.row, column});
          }
          break;
        case Match3Special::kColumnBlast:
          ++news_.line_blasts;
          for (int row = 0; row < kMatch3Side; ++row) {
            mark({row, here.column});
          }
          break;
        case Match3Special::kBomb:
          ++news_.bomb_blasts;
          for (int row = here.row - kBombReach; row <= here.row + kBombReach; ++row) {
            for (int column = here.column - kBombReach; column <= here.column + kBombReach;
                 ++column) {
              mark({row, column});
            }
          }
          break;
        case Match3Special::kStar: {
          ++news_.star_blasts;
          const int kind = MostCommonKind();
          for (int other = 0; other < kMatch3Cells; ++other) {
            if (cells_[static_cast<std::size_t>(other)] == kind) {
              marks[static_cast<std::size_t>(other)] = true;
            }
          }
          break;
        }
      }
    }
  }
  return blasts;
}

bool Match3Game::ClearStep(std::uint32_t now_ms) {
  const std::vector<Run> runs = FindRuns(cells_);
  if (runs.empty()) {
    return false;
  }
  Marks marks;
  int long_bonus = 0;
  MarkRuns(runs, marks, long_bonus);

  // LES GEMMES SPÉCIALES QUI NAISSENT : l'étoile d'abord, puis la bombe d'un
  // croisement, puis la ligne ; un alignement n'en donne qu'une.
  std::vector<Creation> creations;
  std::vector<bool> spent(runs.size(), false);
  Marks reserved = {};
  // La case qui reçoit la gemme spéciale : une case échangée si elle est dans
  // l'alignement, sinon celle du milieu, jamais une case déjà promise.
  const auto place = [&](const Run& run) -> std::optional<Match3Cell> {
    if (swap_step_) {
      for (const Match3Cell swapped : {swap_second_, swap_first_}) {
        if (run.Holds(swapped) && !reserved[SlotOf(swapped)]) {
          return swapped;
        }
      }
    }
    for (int step = 0; step < 2 * run.length; ++step) {
      // Du milieu vers les bords, en alternant.
      const int middle = run.start + (run.length - 1) / 2;
      const int at = middle + ((step % 2 == 0) ? step / 2 : -(step + 1) / 2);
      if (at < run.start || at >= run.start + run.length) {
        continue;
      }
      const Match3Cell cell = run.CellOf(at);
      if (!reserved[SlotOf(cell)]) {
        return cell;
      }
    }
    return std::nullopt;
  };
  const auto create = [&](Match3Cell cell, Match3Special special, int kind) {
    reserved[SlotOf(cell)] = true;
    creations.push_back(Creation{.cell = cell, .special = special, .kind = kind});
  };
  for (std::size_t index = 0; index < runs.size(); ++index) {
    if (runs[index].length >= kMatch3StarRun) {
      if (const auto cell = place(runs[index])) {
        create(*cell, Match3Special::kStar, kMatch3StarKind);
        spent[index] = true;
      }
    }
  }
  for (std::size_t first = 0; first < runs.size(); ++first) {
    for (std::size_t second = 0; second < runs.size(); ++second) {
      const Run& across = runs[first];
      const Run& down = runs[second];
      if (spent[first] || spent[second] || !across.horizontal || down.horizontal ||
          across.kind != down.kind) {
        continue;
      }
      const Match3Cell crossing{across.across, down.across};
      if (across.Holds(crossing) && down.Holds(crossing) && !reserved[SlotOf(crossing)]) {
        create(crossing, Match3Special::kBomb, across.kind);
        spent[first] = true;
        spent[second] = true;
      }
    }
  }
  for (std::size_t index = 0; index < runs.size(); ++index) {
    if (!spent[index] && runs[index].length == kMatch3LineRun) {
      if (const auto cell = place(runs[index])) {
        create(*cell,
               runs[index].horizontal ? Match3Special::kRowBlast : Match3Special::kColumnBlast,
               runs[index].kind);
      }
    }
  }

  Marks fired = {};
  const int blasts = Detonate(marks, fired, now_ms);
  Resolve(marks, long_bonus, blasts, creations, now_ms);
  return true;
}

void Match3Game::StarStep(std::uint32_t now_ms) {
  Marks marks = {};
  Marks fired = {};
  const int first_kind = KindAt(swap_first_);
  const int second_kind = KindAt(swap_second_);
  // ⚠ DEUX ÉTOILES VIDENT LA GRILLE ; une seule efface l'espèce de l'autre case.
  const bool both = first_kind == kMatch3StarKind && second_kind == kMatch3StarKind;
  const int target = first_kind == kMatch3StarKind ? second_kind : first_kind;
  for (int index = 0; index < kMatch3Cells; ++index) {
    const auto slot = static_cast<std::size_t>(index);
    if (both || cells_[slot] == target) {
      marks[slot] = true;
    }
  }
  for (const Match3Cell swapped : {swap_first_, swap_second_}) {
    if (KindAt(swapped) != kMatch3StarKind) {
      continue;
    }
    marks[SlotOf(swapped)] = true;
    fired[SlotOf(swapped)] = true;
    ++news_.star_blasts;
    blasts_.push_back(Match3Blast{.cell = swapped,
                                  .special = Match3Special::kStar,
                                  .kind = kMatch3StarKind,
                                  .started_ms = now_ms});
  }
  const int stars = both ? 2 : 1;
  const int blasts = stars + Detonate(marks, fired, now_ms);
  Resolve(marks, 0, blasts, {}, now_ms);
}

void Match3Game::Resolve(const Marks& marks, int long_bonus, int blasts,
                         const std::vector<Creation>& creations, std::uint32_t now_ms) {
  const int marked = static_cast<int>(std::ranges::count(marks, true));
  ++combo_;
  best_combo_ = std::max(best_combo_, combo_);
  const int points = (marked * kMatch3GemPoints + long_bonus * kMatch3LongRunPoints +
                      blasts * kMatch3BlastPoints) *
                     combo_;
  score_ += points;
  ++news_.bursts;
  float row_sum = 0.0F;
  float column_sum = 0.0F;
  for (int index = 0; index < kMatch3Cells; ++index) {
    if (marks[static_cast<std::size_t>(index)]) {
      const Match3Cell cell = CellAt(index);
      bursts_.push_back(Match3Burst{.cell = cell, .kind = At(cell), .started_ms = now_ms});
      row_sum += static_cast<float>(cell.row);
      column_sum += static_cast<float>(cell.column);
      At(cell) = kMatch3Empty;
      SpecialRef(cell) = Match3Special::kNone;
    }
  }
  gains_.push_back(Match3Gain{.row = row_sum / static_cast<float>(std::max(marked, 1)),
                              .column = column_sum / static_cast<float>(std::max(marked, 1)),
                              .points = points,
                              .combo = combo_,
                              .started_ms = now_ms});
  for (const Creation& creation : creations) {
    At(creation.cell) = creation.kind;
    SpecialRef(creation.cell) = creation.special;
    ++news_.specials_made;
  }
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
      const Match3Special special = SpecialRef({row, column});
      SpecialRef({row, column}) = Match3Special::kNone;
      At({landing, column}) = kind;
      SpecialRef({landing, column}) = special;
      fallen_rows_[SlotOf({landing, column})] = landing - row;
      longest_fall = std::max(longest_fall, landing - row);
      --landing;
    }
    const int holes = landing + 1;
    longest_fall = std::max(longest_fall, holes);
    for (int row = landing; row >= 0; --row) {
      At({row, column}) = RandomKind();
      SpecialRef({row, column}) = Match3Special::kNone;
      fallen_rows_[SlotOf({row, column})] = holes;
    }
  }
  longest_fall_ms_ = FallMs(longest_fall);
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
  std::erase_if(blasts_, [now_ms](const Match3Blast& blast) {
    return now_ms - blast.started_ms > kMatch3BlastMs;
  });
  switch (phase_) {
    case Match3Phase::kIdle:
    case Match3Phase::kOver:
      return;
    case Match3Phase::kSwapping: {
      if (now_ms - phase_started_ms_ < kMatch3SwapMs) {
        return;
      }
      const bool star =
          KindAt(swap_first_) == kMatch3StarKind || KindAt(swap_second_) == kMatch3StarKind;
      swap_step_ = true;
      if (star) {
        StarStep(now_ms);
        --moves_left_;
        phase_ = Match3Phase::kFalling;
      } else if (ClearStep(now_ms)) {
        --moves_left_;
        phase_ = Match3Phase::kFalling;
      } else {
        Swap(swap_first_, swap_second_);
        ++news_.refused_swaps;
        phase_ = Match3Phase::kReverting;
      }
      swap_step_ = false;
      phase_started_ms_ = now_ms;
      return;
    }
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
    const int fallen = fallen_rows_[SlotOf(cell)];
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

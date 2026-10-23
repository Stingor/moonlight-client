#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace ro::sim {

// ── ROJEWELED : UN JEU DE TROIS À ALIGNER ─────────────────────────────────────
//
// Une grille de huit sur huit, six espèces de gemmes. On échange deux voisines ;
// l'échange tient s'il aligne au moins trois gemmes de la même espèce, sinon il
// se défait. L'alignement s'efface, les colonnes tombent, le haut se remplit, et
// un nouvel alignement né de la chute est une CASCADE qui rapporte davantage.
//
// 🔴 UNE CASCADE SE JOUE PAS À PAS : effacement, puis chute animée, puis
// nouvel examen. Résolue d'un coup, la grille changerait sous les yeux sans
// qu'on voie d'où vient le score.
//
// ⚠ LA PARTIE SE COMPTE EN COUPS. Un échange qui tient en coûte un ; un
// échange défait n'en coûte aucun. À zéro, la partie est finie : c'est ce qui
// donne un sens au meilleur score.
//
// ⚠ AUCUNE GRILLE N'EST JAMAIS BLOQUÉE : une grille sans échange possible est
// remélangée, avant d'être montrée.

inline constexpr int kMatch3Side = 8;
inline constexpr int kMatch3Kinds = 6;
inline constexpr int kMatch3Cells = kMatch3Side * kMatch3Side;
// Une case vide, le temps d'une chute.
inline constexpr int kMatch3Empty = -1;
// Les coups d'une partie.
inline constexpr int kMatch3Moves = 30;
// Ce que rapporte une gemme effacée, avant le multiplicateur de cascade.
inline constexpr int kMatch3GemPoints = 50;
// Ce que rapporte chaque gemme au-delà de trois dans un même alignement.
inline constexpr int kMatch3LongRunPoints = 100;

// Les durées des animations, en millisecondes.
inline constexpr std::uint32_t kMatch3SwapMs = 140;
inline constexpr std::uint32_t kMatch3FallMs = 220;
inline constexpr std::uint32_t kMatch3BurstMs = 380;
// Le silence après lequel un échange possible se montre.
inline constexpr std::uint32_t kMatch3HintAfterMs = 3500;

struct Match3Cell {
  int row = 0;
  int column = 0;

  [[nodiscard]] bool operator==(const Match3Cell&) const = default;
};

// Une gemme qui vient de s'effacer, et qui éclate encore un instant.
struct Match3Burst {
  Match3Cell cell;
  int kind = 0;
  std::uint32_t started_ms = 0;
};

enum class Match3Phase : std::uint8_t {
  kIdle,
  // Les deux gemmes glissent l'une vers l'autre.
  kSwapping,
  // L'échange n'alignait rien : elles reviennent.
  kReverting,
  // Les colonnes tombent après un effacement.
  kFalling,
  // Plus aucun coup.
  kOver,
};

// CE QUE LA PARTIE A FAIT ENTENDRE ou accompli depuis la dernière lecture.
struct Match3News {
  // Un effacement par pas de cascade.
  int bursts = 0;
  // Un échange défait.
  int refused_swaps = 0;
  // La partie vient de finir.
  bool finished = false;
};

class Match3Game {
 public:
  explicit Match3Game(std::uint32_t seed) : random_(seed) {}

  void NewGame(std::uint32_t now_ms);

  // UN CLIC SUR UNE CASE : la choisir, l'échanger avec la case choisie si elle
  // lui est voisine, ou changer de choix. Ignoré pendant une animation.
  void Click(Match3Cell cell, std::uint32_t now_ms);

  // À CHAQUE IMAGE : fait avancer les animations et les cascades.
  void Advance(std::uint32_t now_ms);

  [[nodiscard]] Match3News TakeNews() noexcept;

  [[nodiscard]] int KindAt(Match3Cell cell) const;
  [[nodiscard]] bool started() const noexcept { return started_; }
  [[nodiscard]] Match3Phase phase() const noexcept { return phase_; }
  [[nodiscard]] int score() const noexcept { return score_; }
  [[nodiscard]] int moves_left() const noexcept { return moves_left_; }
  // Le pas de cascade en cours, 1 au premier effacement d'un coup.
  [[nodiscard]] int combo() const noexcept { return combo_; }
  // La plus longue cascade de la partie.
  [[nodiscard]] int best_combo() const noexcept { return best_combo_; }
  [[nodiscard]] std::optional<Match3Cell> selected() const noexcept { return selected_; }
  [[nodiscard]] const std::vector<Match3Burst>& bursts() const noexcept { return bursts_; }

  // OÙ UNE GEMME SE DESSINE, en cases, par rapport à sa place : nulle au repos,
  // vers sa place d'avant pendant un glissement ou une chute.
  struct Offset {
    float rows = 0.0F;
    float columns = 0.0F;
  };
  [[nodiscard]] Offset OffsetAt(Match3Cell cell, std::uint32_t now_ms) const;

  // L'ÉCHANGE À SUGGÉRER, après un silence de `kMatch3HintAfterMs`.
  [[nodiscard]] std::optional<std::array<Match3Cell, 2>> Hint(std::uint32_t now_ms) const;

  // ── POUR LES ESSAIS ────────────────────────────────────────────────────────
  // Pose une grille entière, rangée par rangée. Aucun remélange.
  void SetCells(const std::array<int, kMatch3Cells>& kinds, std::uint32_t now_ms);
  // Un échange possible, ou aucun : la grille est bloquée.
  [[nodiscard]] std::optional<std::array<Match3Cell, 2>> FindMove() const;

 private:
  using Marks = std::array<bool, kMatch3Cells>;

  [[nodiscard]] int& At(Match3Cell cell);
  [[nodiscard]] int RandomKind();
  // Marque les alignements ; rend le nombre de gemmes marquées et, dans
  // `long_bonus`, les gemmes au-delà de trois.
  [[nodiscard]] int FindMatches(Marks& marks, int& long_bonus) const;
  void Swap(Match3Cell first, Match3Cell second);
  // Efface ce qui est aligné, fait tomber, remplit : un pas de cascade. Faux
  // quand rien n'était aligné.
  bool ClearStep(std::uint32_t now_ms);
  void Reshuffle();
  void Settle(std::uint32_t now_ms);

  std::mt19937 random_;
  std::array<int, kMatch3Cells> cells_ = {};
  // Les rangées qu'une gemme vient de tomber, pour l'animation de chute.
  std::array<int, kMatch3Cells> fallen_rows_ = {};
  bool started_ = false;
  Match3Phase phase_ = Match3Phase::kIdle;
  int score_ = 0;
  int moves_left_ = kMatch3Moves;
  int combo_ = 0;
  int best_combo_ = 0;
  std::optional<Match3Cell> selected_;
  Match3Cell swap_first_;
  Match3Cell swap_second_;
  std::uint32_t phase_started_ms_ = 0;
  std::uint32_t last_action_ms_ = 0;
  std::vector<Match3Burst> bursts_;
  Match3News news_;
};

}  // namespace ro::sim

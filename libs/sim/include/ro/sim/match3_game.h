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
// remélangée. Le remélange est une phase à part, que la fenêtre annonce, et
// pendant laquelle les gemmes neuves apparaissent.

inline constexpr int kMatch3Side = 8;
inline constexpr int kMatch3Kinds = 6;
inline constexpr int kMatch3Cells = kMatch3Side * kMatch3Side;
// Le plus petit alignement qui s'efface.
inline constexpr int kMatch3ShortestRun = 3;
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
inline constexpr std::uint32_t kMatch3BurstMs = 380;
// ⚠ UNE GEMME TOMBE COMME SOUS LA PESANTEUR : elle accélère, et une chute de
// `n` rangées dure `kMatch3FallRowMs × √n`. Une chute longue va donc plus vite
// qu'une courte, sans durer `n` fois plus.
inline constexpr std::uint32_t kMatch3FallRowMs = 110;
// Le petit bond d'une gemme qui vient de se poser, et sa hauteur en rangées.
inline constexpr std::uint32_t kMatch3LandingMs = 80;
inline constexpr float kMatch3LandingRows = 0.08F;
// Le remélange d'une grille bloquée.
inline constexpr std::uint32_t kMatch3ReshuffleMs = 900;
// Le temps qu'une cascade reste dite une fois la grille au repos.
inline constexpr std::uint32_t kMatch3ComboLingerMs = 900;
// Le temps que des points gagnés restent à l'écran.
inline constexpr std::uint32_t kMatch3GainMs = 900;
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

// LES POINTS D'UN PAS DE CASCADE, montrés au milieu de ce qui s'est effacé.
struct Match3Gain {
  // Le centre des gemmes effacées, en cases.
  float row = 0.0F;
  float column = 0.0F;
  // Multiplicateur de cascade compris.
  int points = 0;
  int combo = 0;
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
  // La grille était bloquée : une grille remélangée apparaît.
  kReshuffling,
  // Plus aucun coup.
  kOver,
};

// CE QUE LA PARTIE A FAIT ENTENDRE ou accompli depuis la dernière lecture.
struct Match3News {
  // Un effacement par pas de cascade.
  int bursts = 0;
  // Un échange défait.
  int refused_swaps = 0;
  // La grille vient d'être remélangée.
  bool reshuffled = false;
  // La partie vient de finir.
  bool finished = false;
};

class Match3Game {
 public:
  explicit Match3Game(std::uint32_t seed) : random_(seed), hint_random_(~seed) {}

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
  [[nodiscard]] const std::vector<Match3Gain>& gains() const noexcept { return gains_; }

  // LA CASCADE À DIRE : celle en cours, ou la dernière pendant
  // `kMatch3ComboLingerMs` après le retour au repos.
  [[nodiscard]] int ShownCombo(std::uint32_t now_ms) const;

  // OÙ UNE GEMME SE DESSINE, en cases, par rapport à sa place : nulle au repos,
  // vers sa place d'avant pendant un glissement ou une chute, un peu au-dessus
  // pendant le bond qui suit la chute.
  struct Offset {
    float rows = 0.0F;
    float columns = 0.0F;
  };
  [[nodiscard]] Offset OffsetAt(Match3Cell cell, std::uint32_t now_ms) const;

  // LA TAILLE D'UNE GEMME, de 0 à 1 : elle grandit pendant un remélange.
  [[nodiscard]] float GemScale(std::uint32_t now_ms) const;

  // L'ÉCHANGE À SUGGÉRER, après un silence de `kMatch3HintAfterMs`. Tiré au
  // hasard parmi les échanges possibles à chaque retour au repos, et le même
  // jusqu'au suivant.
  [[nodiscard]] std::optional<std::array<Match3Cell, 2>> Hint(std::uint32_t now_ms) const;

  // ── POUR LES ESSAIS ────────────────────────────────────────────────────────
  // Pose une grille entière, rangée par rangée. Aucun remélange.
  void SetCells(const std::array<int, kMatch3Cells>& kinds, std::uint32_t now_ms);
  // Un échange possible, ou aucun : la grille est bloquée.
  [[nodiscard]] std::optional<std::array<Match3Cell, 2>> FindMove() const;
  // Tous les échanges possibles.
  [[nodiscard]] std::vector<std::array<Match3Cell, 2>> AllMoves() const;
  // Remélange une grille bloquée, et en tire une neuve quand les remélanges
  // échouent trop longtemps. Rend une grille sans alignement qui offre un échange.
  void Reshuffle();

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
  // Tire une grille entière sans alignement. Elle peut n'offrir aucun échange.
  void FillWithoutMatches();
  void Settle(std::uint32_t now_ms);
  void ChooseHint();

  std::mt19937 random_;
  // ⚠ L'INDICE A SON PROPRE GÉNÉRATEUR : le tirer ne change pas les gemmes qui
  // tombent ensuite. Amorcé du complément de la graine, pour ne pas rejouer la
  // suite des gemmes ; et pas un `minstd_rand`, dont le premier tirage d'une
  // petite graine est si petit qu'il désigne toujours le premier échange.
  std::mt19937 hint_random_;
  std::array<int, kMatch3Cells> cells_ = {};
  // Les rangées qu'une gemme vient de tomber, pour l'animation de chute.
  std::array<int, kMatch3Cells> fallen_rows_ = {};
  // La plus longue chute du pas de cascade en cours.
  std::uint32_t longest_fall_ms_ = 0;
  bool started_ = false;
  Match3Phase phase_ = Match3Phase::kIdle;
  int score_ = 0;
  int moves_left_ = kMatch3Moves;
  int combo_ = 0;
  int best_combo_ = 0;
  // La cascade du dernier coup, dite encore un instant après le repos.
  int last_combo_ = 0;
  std::uint32_t settled_ms_ = 0;
  std::optional<Match3Cell> selected_;
  std::optional<std::array<Match3Cell, 2>> hint_;
  Match3Cell swap_first_;
  Match3Cell swap_second_;
  std::uint32_t phase_started_ms_ = 0;
  std::uint32_t last_action_ms_ = 0;
  std::vector<Match3Burst> bursts_;
  std::vector<Match3Gain> gains_;
  Match3News news_;
};

}  // namespace ro::sim

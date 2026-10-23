#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace ro::sim {

// ── ROGGLE : UNE BILLE, DES PLOTS, UN SEAU ────────────────────────────────────
//
// Un canon en haut du plateau lance une bille qui tombe sous la gravité et
// rebondit sur des plots. Les plots ORANGE sont à effacer tous pour gagner ; les
// BLEUS ne rapportent que des points. Un plot touché s'allume, et s'efface quand
// la bille quitte le jeu. Un seau va et vient en bas : la bille qu'il attrape
// est rendue.
//
// 🔴 LA PHYSIQUE AVANCE À PAS FIXE, quelle que soit la cadence d'affichage : le
// même tir donne le même rebond à 30 comme à 240 images par seconde.
//
// ⚠ PLUS ON EFFACE D'ORANGE, PLUS UN PLOT RAPPORTE : le multiplicateur monte
// par paliers. Et chaque bille restante à la victoire rapporte un bonus.
//
// Les longueurs sont en pixels du plateau, les vitesses en pixels par seconde :
// le plateau se dessine à son échelle naturelle.

inline constexpr float kPegBoardWidth = 520.0F;
inline constexpr float kPegBoardHeight = 640.0F;
inline constexpr float kPegBallRadius = 7.0F;
inline constexpr float kPegRadius = 10.0F;
inline constexpr float kPegCannonX = kPegBoardWidth * 0.5F;
inline constexpr float kPegCannonY = 34.0F;
inline constexpr float kPegBucketWidth = 82.0F;
inline constexpr float kPegBucketHeight = 16.0F;
inline constexpr float kPegBucketY = kPegBoardHeight - 22.0F;
inline constexpr int kPegStartBalls = 10;

// Le pas de la physique, en secondes.
inline constexpr float kPegStepS = 1.0F / 240.0F;

// Ce que rapporte un plot, avant le multiplicateur.
inline constexpr int kPegOrangePoints = 200;
inline constexpr int kPegBluePoints = 100;
// Ce que rapporte une bille restante à la victoire.
inline constexpr int kPegBallLeftPoints = 1000;

struct Peg {
  float x = 0.0F;
  float y = 0.0F;
  bool orange = false;
  // Touché pendant le tir en cours : il s'effacera quand la bille sortira.
  bool lit = false;
  bool removed = false;
};

enum class PegPhase : std::uint8_t {
  kAiming,
  kFlying,
  kWon,
  kLost,
};

// Un point du plateau.
struct PegPoint {
  float x = 0.0F;
  float y = 0.0F;
};

// CE QUE LA PARTIE A FAIT ENTENDRE ou accompli depuis la dernière lecture.
struct PegNews {
  int orange_hits = 0;
  int blue_hits = 0;
  // Le seau vient d'attraper la bille.
  bool caught = false;
  // La partie vient de finir, gagnée ou perdue.
  bool finished = false;
};

class PegGame {
 public:
  explicit PegGame(std::uint32_t seed) : random_(seed) {}

  void NewGame();

  // Vise vers ce point du plateau.
  void Aim(PegPoint target) noexcept;
  // Tire, si l'on vise.
  void Launch();
  // Fait avancer la physique de `elapsed_s` secondes, par pas fixes.
  void Advance(float elapsed_s);

  [[nodiscard]] PegNews TakeNews() noexcept;

  // LA TRAJECTOIRE DU TIR VISÉ, gravité et murs compris, plots ignorés : un
  // point par pas d'affichage du guide.
  [[nodiscard]] std::vector<PegPoint> AimGuide() const;

  [[nodiscard]] bool started() const noexcept { return started_; }
  [[nodiscard]] PegPhase phase() const noexcept { return phase_; }
  [[nodiscard]] const std::vector<Peg>& pegs() const noexcept { return pegs_; }
  [[nodiscard]] PegPoint ball() const noexcept { return ball_; }
  [[nodiscard]] PegPoint aim() const noexcept { return aim_; }
  [[nodiscard]] float bucket_x() const noexcept { return bucket_x_; }
  [[nodiscard]] int balls_left() const noexcept { return balls_left_; }
  [[nodiscard]] int score() const noexcept { return score_; }
  [[nodiscard]] int orange_left() const noexcept { return orange_left_; }
  // Les billes que le seau a rendues dans la partie.
  [[nodiscard]] int catches() const noexcept { return catches_; }
  // Le multiplicateur des plots, selon la part d'orange déjà effacée.
  [[nodiscard]] int multiplier() const noexcept;

  // ── POUR LES ESSAIS ────────────────────────────────────────────────────────
  // Pose ses propres plots et repart d'une partie neuve.
  void SetPegs(std::vector<Peg> pegs);
  void PlaceBall(PegPoint at, PegPoint velocity);

 private:
  void Restart();
  void Step(float step_s);
  void EndBall();
  [[nodiscard]] PegPoint LaunchVelocity() const noexcept;

  std::mt19937 random_;
  std::vector<Peg> pegs_;
  bool started_ = false;
  PegPhase phase_ = PegPhase::kAiming;
  int balls_left_ = kPegStartBalls;
  int score_ = 0;
  int orange_total_ = 0;
  int orange_left_ = 0;
  int catches_ = 0;
  PegPoint ball_;
  PegPoint velocity_;
  float ball_time_s_ = 0.0F;
  bool caught_ = false;
  float bucket_x_ = kPegBoardWidth * 0.5F;
  float bucket_direction_ = 1.0F;
  PegPoint aim_{.x = kPegBoardWidth * 0.5F, .y = kPegBoardHeight};
  // Le temps pas encore consommé par un pas de physique.
  float pending_s_ = 0.0F;
  PegNews news_;
};

}  // namespace ro::sim

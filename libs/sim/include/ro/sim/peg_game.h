#pragma once

#include <array>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

namespace ro::sim {

// ── ROGGLE : UNE BILLE, DES PLOTS, UN SEAU ────────────────────────────────────
//
// Un canon en haut du plateau lance une bille qui tombe sous la gravité et
// rebondit sur des plots. Les plots ORANGE sont à effacer tous pour gagner ; les
// BLEUS ne rapportent que des points, le VIOLET davantage, et il change de place
// après chaque tir. Un plot VERT déclenche un pouvoir. Un plot touché s'allume,
// et s'efface quand le tir finit. Un seau va et vient en bas : un tir qu'il
// attrape rend sa bille.
//
// 🔴 LA PHYSIQUE AVANCE À PAS FIXE, quelle que soit la cadence d'affichage : le
// même tir donne le même rebond à 30 comme à 240 images par seconde. Le ralenti
// près du dernier orange espace les pas dans le temps réel, sans les changer.
//
// ⚠ PLUS ON EFFACE D'ORANGE, PLUS UN PLOT RAPPORTE : le multiplicateur monte
// par paliers. Les bonus de style, la bille offerte et les cases de la finale
// ne passent pas par lui. Chaque bille restante à la victoire rapporte un bonus.
//
// Les longueurs sont en pixels du plateau, les vitesses en pixels par seconde :
// le plateau se dessine à son échelle naturelle. Voir l'ADR 0139.

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
inline constexpr int kPegPurplePoints = 500;
inline constexpr int kPegGreenPoints = 100;
// Ce que rapporte une bille restante à la victoire.
inline constexpr int kPegBallLeftPoints = 1000;

// LES BONUS DE STYLE, hors multiplicateur.
inline constexpr int kPegLongShotPoints = 2500;
// La course sans plot qui fait d'un orange touché un tir lointain.
inline constexpr float kPegLongShotPx = 260.0F;
inline constexpr int kPegBankShotPoints = 1000;
inline constexpr int kPegVolleyPoints = 2000;
// Les plots allumés en un tir qui font une rafale.
inline constexpr int kPegVolleyPegs = 10;
// Ce qu'un tir doit rapporter pour offrir une bille, une fois par tir.
inline constexpr int kPegFreeBallShotPoints = 5000;

// LES CASES DE LA FINALE, de gauche à droite, d'égale largeur.
inline constexpr std::array<int, 5> kPegFinaleBins = {2000, 5000, 10000, 5000, 2000};
inline constexpr float kPegFinaleBinHeight = 26.0F;

// Le seau sous Safety Wall, et le nombre de tirs qu'il dure, celui-ci compris.
inline constexpr float kPegSafetyWallWidth = kPegBucketWidth * 2.0F;
inline constexpr int kPegSafetyWallShots = 2;
// Le rayon de Magnum Break, du centre du plot vert.
inline constexpr float kPegMagnumRadius = 72.0F;

// LE RALENTI : une bille à cette distance du dernier orange encore éteint fait
// couler le temps à cette fraction.
inline constexpr float kPegSlowRadius = 70.0F;
inline constexpr float kPegSlowScale = 0.3F;

enum class PegKind : std::uint8_t {
  kBlue,
  kOrange,
  kPurple,
  kGreen,
};

// Le pouvoir d'un plot vert ; `kNone` pour tous les autres.
enum class PegPower : std::uint8_t {
  kNone,
  kMagnumBreak,
  kSafetyWall,
  kDoubleStrafe,
};
inline constexpr int kPegPowerCount = 3;

enum class PegLayout : std::uint8_t {
  kScattered,
  kPyramid,
  kRings,
  kPoring,
  kWaves,
};
inline constexpr int kPegLayoutCount = 5;

// Un point du plateau.
struct PegPoint {
  float x = 0.0F;
  float y = 0.0F;
};

// UN PLOT QUI TOURNE autour d'un centre ; immobile sous un rayon nul.
struct PegOrbit {
  PegPoint center;
  float radius = 0.0F;
  float speed_rad_s = 0.0F;
  float angle_rad = 0.0F;
};

struct Peg {
  float x = 0.0F;
  float y = 0.0F;
  PegKind kind = PegKind::kBlue;
  PegPower power = PegPower::kNone;
  // Touché pendant le tir en cours : il s'effacera quand le tir finira.
  bool lit = false;
  bool removed = false;
  PegOrbit orbit;
};

enum class PegPhase : std::uint8_t {
  kAiming,
  kFlying,
  kWon,
  kLost,
};

// UNE BILLE EN JEU. Double Strafe en met une seconde sur le plateau.
struct PegBall {
  PegPoint at;
  PegPoint velocity;
  // LA GARDE CONTRE UNE BILLE COINCÉE : le plus bas qu'elle soit descendue, et
  // depuis combien de temps elle n'a ni descendu plus bas ni allumé de plot.
  float lowest_y = 0.0F;
  float still_s = 0.0F;
  // La course depuis le dernier plot, pour le tir lointain.
  float since_peg_px = 0.0F;
  bool touched_peg = false;
  // Le dernier contact était un mur, pour la bande.
  bool after_wall = false;
};

// CE QUI MÉRITE UN MOT À L'ÉCRAN, à l'endroit où c'est arrivé.
enum class PegCalloutKind : std::uint8_t {
  // Les points d'un plot allumé.
  kPoints,
  kLongShot,
  kBankShot,
  kVolley,
  kFreeBall,
  kMagnumBreak,
  kSafetyWall,
  kDoubleStrafe,
  // Le dernier orange est touché : place aux cases.
  kFinale,
  // Une case de la finale.
  kBin,
};

struct PegCallout {
  PegCalloutKind kind = PegCalloutKind::kPoints;
  PegPoint at;
  // Nul quand l'avis ne rapporte rien.
  int points = 0;
};

// Un plot qui vient de s'effacer.
struct PegCleared {
  PegPoint at;
  PegKind kind = PegKind::kBlue;
};

// CE QUE LA PARTIE A FAIT ENTENDRE ou accompli depuis la dernière lecture.
struct PegNews {
  int orange_hits = 0;
  int blue_hits = 0;
  // Violets et verts.
  int special_hits = 0;
  int wall_hits = 0;
  // Un plot vert vient de jouer son pouvoir.
  bool power = false;
  // Le seau vient d'attraper une bille.
  bool caught = false;
  bool free_ball = false;
  // La partie vient de finir, gagnée ou perdue.
  bool finished = false;
  std::vector<PegCallout> callouts;
  std::vector<PegCleared> cleared;
};

class PegGame {
 public:
  explicit PegGame(std::uint32_t seed) : seeds_(seed) {}

  // Une partie neuve, sur un plateau tiré au hasard ou nommé.
  void NewGame();
  void NewGame(PegLayout layout);
  // LE DÉFI DU JOUR : le plateau et tout le hasard de la partie se tirent du
  // numéro du jour, compté depuis l'époque Unix ; la même partie pour tous.
  void NewDailyGame(std::uint32_t day);

  // Vise vers ce point du plateau.
  void Aim(PegPoint target) noexcept;
  // TOURNE LA VISÉE autour du canon, d'un quart de degré par cran ; un cran
  // positif tourne dans le sens des aiguilles d'une montre à l'écran.
  void NudgeAim(int steps) noexcept;
  // Tire, si l'on vise.
  void Launch();
  // Fait avancer la partie de `elapsed_s` secondes de temps réel, par pas fixes.
  void Advance(float elapsed_s);

  [[nodiscard]] PegNews TakeNews();

  // LA TRAJECTOIRE DU TIR VISÉ, gravité et murs compris, plots ignorés : un
  // point par pas d'affichage du guide.
  [[nodiscard]] std::vector<PegPoint> AimGuide() const;

  [[nodiscard]] bool started() const noexcept { return started_; }
  [[nodiscard]] PegPhase phase() const noexcept { return phase_; }
  [[nodiscard]] const std::vector<Peg>& pegs() const noexcept { return pegs_; }
  [[nodiscard]] std::span<const PegBall> balls() const noexcept { return balls_; }
  // La première bille en jeu, ou la dernière sortie.
  [[nodiscard]] PegPoint ball() const noexcept;
  [[nodiscard]] PegPoint aim() const noexcept { return aim_; }
  [[nodiscard]] float bucket_x() const noexcept { return bucket_x_; }
  [[nodiscard]] float bucket_width() const noexcept;
  [[nodiscard]] int balls_left() const noexcept { return balls_left_; }
  [[nodiscard]] int score() const noexcept { return score_; }
  [[nodiscard]] int orange_left() const noexcept { return orange_left_; }
  // Les billes que le seau a rendues dans la partie.
  [[nodiscard]] int catches() const noexcept { return catches_; }
  // Le multiplicateur des plots, selon la part d'orange déjà effacée.
  [[nodiscard]] int multiplier() const noexcept;
  [[nodiscard]] PegLayout layout() const noexcept { return layout_; }
  [[nodiscard]] bool daily() const noexcept { return daily_; }
  [[nodiscard]] std::uint32_t day() const noexcept { return day_; }
  // Le dernier orange est touché : le seau a laissé place aux cases.
  [[nodiscard]] bool finale() const noexcept { return finale_; }
  // Une bille frôle le dernier orange : le temps ralentit.
  [[nodiscard]] bool slowed() const noexcept;
  [[nodiscard]] int powers_used() const noexcept { return powers_used_; }
  [[nodiscard]] int free_balls() const noexcept { return free_balls_; }
  // Le tir le plus rentable de la partie, style compris.
  [[nodiscard]] int best_shot() const noexcept { return best_shot_; }

  // ── POUR LES ESSAIS ────────────────────────────────────────────────────────
  // Pose ses propres plots et repart d'une partie neuve, sans violet tournant.
  void SetPegs(std::vector<Peg> pegs);
  void PlaceBall(PegPoint at, PegPoint velocity);

 private:
  void StartBoard(PegLayout layout, bool daily, std::uint32_t day);
  void Restart();
  void Step(float step_s);
  void MoveBucket(float step_s);
  void MoveOrbits(float step_s);
  void StepBall(PegBall& ball, float step_s);
  void ReleaseStuck(PegBall& ball);
  void Light(Peg& peg, PegBall* ball);
  void Award(PegCalloutKind kind, PegPoint at, int points);
  void TriggerPower(const Peg& peg, PegBall* ball);
  void LeaveBoard(const PegBall& ball);
  void EndShot();
  void ChoosePurple();
  [[nodiscard]] const Peg* LastUnlitOrange() const noexcept;
  [[nodiscard]] PegPoint LaunchVelocity() const noexcept;

  // LE GÉNÉRATEUR DES PARTIES LIBRES : il amorce `random_` à chaque partie, ce
  // qu'un défi du jour ne détourne pas.
  std::mt19937 seeds_;
  std::mt19937 random_;
  std::vector<Peg> pegs_;
  std::vector<PegBall> balls_;
  // Les billes que Double Strafe ajoute, posées à la fin du pas.
  std::vector<PegBall> spawned_;
  PegPoint last_ball_;
  bool started_ = false;
  PegPhase phase_ = PegPhase::kAiming;
  PegLayout layout_ = PegLayout::kScattered;
  bool daily_ = false;
  std::uint32_t day_ = 0;
  bool purple_rotates_ = false;
  int balls_left_ = kPegStartBalls;
  int score_ = 0;
  int orange_total_ = 0;
  int orange_left_ = 0;
  int catches_ = 0;
  int powers_used_ = 0;
  int free_balls_ = 0;
  int best_shot_ = 0;
  // LE TIR EN COURS.
  float shot_time_s_ = 0.0F;
  int shot_score_ = 0;
  int shot_lit_ = 0;
  bool shot_free_ball_ = false;
  bool caught_ = false;
  bool finale_ = false;
  int safety_shots_ = 0;
  float bucket_x_ = kPegBoardWidth * 0.5F;
  float bucket_direction_ = 1.0F;
  PegPoint aim_{.x = kPegBoardWidth * 0.5F, .y = kPegBoardHeight};
  // Le temps réel pas encore consommé par un pas de physique.
  float pending_s_ = 0.0F;
  PegNews news_;
};

// LE RENVOI SUR LES MURS de gauche, de droite et du haut ; le bas est ouvert.
// Partagé par la bille et le guide de visée. Rend vrai sur un contact.
bool BouncePegOnWalls(PegPoint& at, PegPoint& velocity) noexcept;

// LES PLOTS D'UN PLATEAU, sortes et pouvoirs tirés : un quart d'orange, deux
// verts, le reste en bleu. Voir `peg_layouts.cpp`.
[[nodiscard]] std::vector<Peg> BuildPegLayout(PegLayout layout, std::mt19937& random);

}  // namespace ro::sim

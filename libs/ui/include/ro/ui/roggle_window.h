#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ROGGLE : un canon, une bille, des plots à effacer. Les règles et la physique
// vivent dans `ro/sim/peg_game.h` ; la fenêtre montre le plateau, et rend la
// visée, la visée fine, le tir et le choix de la partie.
//
// ⚠ JOUER NE DÉPLACE PAS LE PERSONNAGE : la souris au-dessus d'une fenêtre
// appartient à l'interface, et le clic de tir n'atteint pas le monde. Le
// clavier, lui, n'est pas pris : les flèches et l'espace restent au jeu.

inline constexpr const char* kRoggleTitle = core::TrText("Roggle");
inline constexpr const char* kRoggleId = "roggle";

// Un point du plateau, en pixels du plateau.
struct RogglePoint {
  float x = 0.0F;
  float y = 0.0F;
};

enum class RogglePegKind : std::uint8_t { kBlue, kOrange, kPurple, kGreen };
enum class RogglePower : std::uint8_t { kNone, kMagnumBreak, kSafetyWall, kDoubleStrafe };

struct RogglePeg {
  RogglePoint at;
  RogglePegKind kind = RogglePegKind::kBlue;
  RogglePower power = RogglePower::kNone;
  bool lit = false;
};

// UN MOT QUI MONTE AU-DESSUS DU PLATEAU : des points, un bonus de style, un
// pouvoir. `progress` va de 0 à son apparition à 1 à sa disparition.
enum class RoggleCalloutKind : std::uint8_t {
  kPoints,
  kLongShot,
  kBankShot,
  kVolley,
  kFreeBall,
  kMagnumBreak,
  kSafetyWall,
  kDoubleStrafe,
  kFinale,
  kBin,
};

struct RoggleCallout {
  RoggleCalloutKind kind = RoggleCalloutKind::kPoints;
  RogglePoint at;
  int points = 0;
  float progress = 0.0F;
};

// UN PLOT QUI ÉCLATE en s'effaçant.
struct RoggleBurst {
  RogglePoint at;
  RogglePegKind kind = RogglePegKind::kBlue;
  float progress = 0.0F;
};

enum class RogglePhase : std::uint8_t { kAiming, kFlying, kWon, kLost };

// LA GÉOMÉTRIE DU PLATEAU, recopiée des règles par `app` : la fenêtre ne
// connaît pas `sim`.
struct RoggleBoard {
  float width = 0.0F;
  float height = 0.0F;
  float ball_radius = 0.0F;
  float peg_radius = 0.0F;
  RogglePoint cannon;
  float bucket_height = 0.0F;
  float bucket_y = 0.0F;
  float finale_bin_height = 0.0F;
};

struct RoggleView {
  RoggleBoard board;
  std::span<const RogglePeg> pegs;
  RogglePhase phase = RogglePhase::kAiming;
  // Les billes en jeu : deux sous Double Strafe.
  std::span<const RogglePoint> balls;
  float bucket_x = 0.0F;
  // Plus large sous Safety Wall.
  float bucket_width = 0.0F;
  RogglePoint aim;
  std::span<const RogglePoint> guide;
  std::span<const RoggleCallout> callouts;
  std::span<const RoggleBurst> bursts;
  // LA FINALE : le seau laisse place à ces cases, de gauche à droite.
  bool finale = false;
  std::span<const int> finale_bins;
  // Le temps ralentit près du dernier orange.
  bool slowed = false;
  // La fenêtre ne regarde pas : la partie attend.
  bool paused = false;
  int balls_left = 0;
  int score = 0;
  int orange_left = 0;
  int multiplier = 1;
  int best_score = 0;
  // LE NOM DU PLATEAU, déjà traduit, et le défi du jour.
  const char* layout_name = "";
  bool daily = false;
  // Le meilleur score du défi d'aujourd'hui ; nul sans défi fini ce jour.
  int daily_best = 0;
  // Les gemmes des plots et le Poring de la bille ; inutilisables, des
  // formes simples les remplacent.
  SkinImage orange_art;
  SkinImage blue_art;
  SkinImage purple_art;
  SkinImage green_art;
  SkinImage ball_art;
  // Le trophée qui vient d'être gagné, en français ; nul sans avis.
  const char* toast = nullptr;
  float toast_alpha = 0.0F;
};

struct RoggleState {
  bool open = false;
};

struct RoggleAsk {
  // Le point du plateau visé, quand la souris a bougé au-dessus du plateau.
  std::optional<RogglePoint> aim;
  // LA VISÉE FINE, en crans de molette ; positif dans le sens des aiguilles
  // d'une montre.
  int aim_nudge = 0;
  bool launch = false;
  bool new_game = false;
  bool daily_game = false;
  // La fenêtre est dépliée et a le focus : la partie peut courir.
  bool watching = false;
};

[[nodiscard]] RoggleAsk DrawRoggleWindow(RoggleState& state, const RoggleView& view,
                                         const SkinArt& art);

}  // namespace ro::ui

#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ROGGLE : un canon, une bille, des plots à effacer. Les règles et la physique
// vivent dans `ro/sim/peg_game.h` ; la fenêtre montre le plateau, et rend la
// visée et le tir.
//
// ⚠ JOUER NE DÉPLACE PAS LE PERSONNAGE : la souris au-dessus d'une fenêtre
// appartient à l'interface, et le clic de tir n'atteint pas le monde.

inline constexpr const char* kRoggleTitle = core::TrText("Roggle");
inline constexpr const char* kRoggleId = "roggle";

// Un point du plateau, en pixels du plateau.
struct RogglePoint {
  float x = 0.0F;
  float y = 0.0F;
};

struct RogglePeg {
  RogglePoint at;
  bool orange = false;
  bool lit = false;
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
  float bucket_width = 0.0F;
  float bucket_height = 0.0F;
  float bucket_y = 0.0F;
};

struct RoggleView {
  RoggleBoard board;
  std::span<const RogglePeg> pegs;
  RogglePhase phase = RogglePhase::kAiming;
  RogglePoint ball;
  float bucket_x = 0.0F;
  RogglePoint aim;
  std::span<const RogglePoint> guide;
  int balls_left = 0;
  int score = 0;
  int orange_left = 0;
  int multiplier = 1;
  int best_score = 0;
  // Les gemmes des plots et le Poring de la bille ; inutilisables, des
  // formes simples les remplacent.
  SkinImage orange_art;
  SkinImage blue_art;
  SkinImage ball_art;
  // Le trophée qui vient d'être gagné, en français ; nul sans avis.
  const char* toast = nullptr;
  float toast_alpha = 0.0F;
};

struct RoggleState {
  bool open = false;
};

struct RoggleAsk {
  // Le point du plateau visé, souris au-dessus du plateau.
  std::optional<RogglePoint> aim;
  bool launch = false;
  bool new_game = false;
};

[[nodiscard]] RoggleAsk DrawRoggleWindow(RoggleState& state, const RoggleView& view,
                                         const SkinArt& art);

}  // namespace ro::ui

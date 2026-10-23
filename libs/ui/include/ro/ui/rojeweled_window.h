#pragma once

#include <array>
#include <optional>
#include <span>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ROJEWELED : trois gemmes à aligner, et les gemmes sont des monstres de la
// famille Poring. Les règles vivent dans `ro/sim/match3_game.h` ; la fenêtre
// montre la grille et rend la case cliquée.
//
// ⚠ CHAQUE ESPÈCE A SA COULEUR, en pastille sous le monstre : plusieurs
// monstres de la famille se ressemblent à s'y tromper, et c'est la couleur
// qu'on aligne. Sans sprite, la pastille reste seule.

inline constexpr const char* kRojeweledTitle = core::TrText("Rojeweled");
inline constexpr const char* kRojeweledId = "rojeweled";

inline constexpr int kRojeweledSide = 8;
inline constexpr int kRojeweledKinds = 6;
inline constexpr int kRojeweledCells = kRojeweledSide * kRojeweledSide;

struct RojeweledCell {
  int row = 0;
  int column = 0;

  [[nodiscard]] bool operator==(const RojeweledCell&) const = default;
};

// Une gemme, telle qu'elle se dessine.
struct RojeweledGem {
  // L'espèce, ou un nombre négatif pour une case vide.
  int kind = 0;
  // Où la dessiner, en cases, par rapport à sa place.
  float offset_rows = 0.0F;
  float offset_columns = 0.0F;
};

// Une gemme effacée qui éclate encore.
struct RojeweledBurst {
  RojeweledCell cell;
  int kind = 0;
  // De 0, l'effacement, à 1, la fin de l'éclat.
  float progress = 0.0F;
};

struct RojeweledView {
  // Rangée par rangée.
  std::array<RojeweledGem, kRojeweledCells> gems = {};
  std::span<const RojeweledBurst> bursts;
  std::optional<RojeweledCell> selected;
  // L'échange à suggérer.
  std::optional<std::array<RojeweledCell, 2>> hint;
  int score = 0;
  int moves_left = 0;
  // Le pas de cascade en cours ; montré à partir de deux.
  int combo = 0;
  int best_score = 0;
  bool over = false;
  // Le monstre de chaque espèce ; inutilisable, la pastille reste seule.
  std::array<SkinImage, kRojeweledKinds> art = {};
  // Le trophée qui vient d'être gagné, en français ; nul sans avis.
  const char* toast = nullptr;
  float toast_alpha = 0.0F;
};

struct RojeweledState {
  bool open = false;
};

struct RojeweledAsk {
  std::optional<RojeweledCell> clicked;
  bool new_game = false;
};

[[nodiscard]] RojeweledAsk DrawRojeweledWindow(RojeweledState& state, const RojeweledView& view,
                                               const SkinArt& art);

}  // namespace ro::ui

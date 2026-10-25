#pragma once

#include <cstdint>
#include <span>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// L'OUVERTURE D'UN OLD BLUE BOX (U17) : une roulette d'icônes pendant 1,5 s,
// qu'un clic passe, puis la liste des gains, qu'un clic ferme. Le monde reste
// figé jusque-là (Q1) ; c'est l'étage qui compte le temps et dit quand la
// liste paraît.

enum class SurvivorsChestGainLook : std::uint8_t { kEvolution, kLevel, kZeny };

// Un gain, déjà résolu par `app`.
struct SurvivorsChestGainRow {
  ImTextureID icon = 0;
  const char* name = "";  // nom affiché, tiré des tables du jeu ; vide pour les Zeny
  int level = 0;          // le niveau atteint
  SurvivorsChestGainLook look = SurvivorsChestGainLook::kLevel;
};

struct SurvivorsChestView {
  std::span<const SurvivorsChestGainRow> gains;
  // Les icônes qui défilent : celles des skills et des cartes possédés.
  std::span<const ImTextureID> roulette_icons;
  std::uint64_t elapsed_ms = 0;  // temps d'écran depuis l'ouverture
  bool list_shown = false;
  bool accepts_input = false;
};

// Le pas de la roulette : une icône chasse l'autre toutes les kRouletteStepMs.
inline constexpr std::uint64_t kRouletteStepMs = 80;

// Vrai quand un clic passe la roulette ou ferme la liste : `proceed` de
// l'intention de l'étage.
[[nodiscard]] bool DrawSurvivorsChestWindow(const SurvivorsChestView& view, const SkinArt& art);

}  // namespace ro::ui

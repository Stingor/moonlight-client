#pragma once

#include <span>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA SALLE DES MINI-JEUX : de quoi lancer Rojeweled et Roggle, leurs meilleurs
// scores, et les trophées.
//
// Un ajout de ce client. Elle s'ouvre au menu d'Échap.
//
// 🔴 ELLE NE POSSÈDE RIEN : scores et trophées viennent d'`app`, qui les range
// dans `savedata`. Elle demande l'ouverture d'un jeu, c'est tout.

inline constexpr const char* kMinigamesTitle = core::TrText("Mini-jeux");
inline constexpr const char* kMinigamesId = "minigames";

// Un trophée, tel que la liste le montre.
struct MinigameTrophyRow {
  // ⚠ EN FRANÇAIS, marqués par `TrText` : la fenêtre les traduit à la lecture.
  const char* name = "";
  const char* description = "";
  bool unlocked = false;
};

struct MinigamesView {
  int rojeweled_best = 0;
  int roggle_best = 0;
  std::span<const MinigameTrophyRow> trophies;
};

struct MinigamesState {
  bool open = false;
};

struct MinigamesAsk {
  bool open_rojeweled = false;
  bool open_roggle = false;
};

[[nodiscard]] MinigamesAsk DrawMinigamesWindow(MinigamesState& state, const MinigamesView& view,
                                               const SkinArt& art);

// L'AVIS D'UN TROPHÉE GAGNÉ, centré en haut du plateau d'un jeu, par-dessus :
// son nom, qui s'efface. Il ne prend aucune place, pour que le plateau ne saute
// pas à son apparition. ⚠ `name` EN FRANÇAIS, marqué par `TrText`. Sans effet
// quand `alpha` est nul.
void DrawTrophyToast(const char* name, float alpha, ImVec2 board_corner, float board_width);

}  // namespace ro::ui

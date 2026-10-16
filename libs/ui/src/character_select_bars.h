#pragma once

#include <imgui.h>

#include "ro/ui/character_select_window.h"
#include "ro/ui/skin_art.h"

// LES BARRES DE LA TABLÉE : l'action, la sortie, les pages.
//
// En-tête PRIVÉ de la scène, qui pose les barres à leurs ancres et arbitre
// entre elles ; ce qui suit les dessine.
namespace ro::ui {

// L'écart entre deux boutons, et entre deux rangées.
inline constexpr float kBarGapPixels = 12.0F;

// LA FORME D'UNE BARRE : la largeur commune de ses boutons, et sur combien de
// rangées elle tient.
//
// ⚠ ON MESURE, et l'on coupe en deux quand il le faut, plutôt que de laisser
// sortir du cadre ce qu'aucune langue ne garantit court.
struct BarShape {
  float width = 0.0F;
  int rows = 1;
  int per_row = 0;
};

[[nodiscard]] float BarRowHeight(const SkinArt& art);

// LA BARRE D'ACTION, sur une ou DEUX rangées. Son ancre est le milieu-haut de
// sa DERNIÈRE rangée : une seconde monte au-dessus.
//
// ⚠ PENDANT LE MODE DÉPLACEMENT ELLE CHANGE DE BOUTONS : valider l'arrivée, ou
// renoncer. Et Échap renonce.
[[nodiscard]] CharacterSelectAction DrawActionBar(CharacterSelectState& state,
                                                  const CharacterSelectView& view,
                                                  const CharacterSlotView* designated,
                                                  const SkinArt& art, ImVec2 middle_top);

// LA FORME QUE PREND LA BARRE D'ACTION AU PLUS LARGE.
//
// 🔴 C'EST ELLE, ET NON LA BARRE DU MOMENT, qui décide où va la barre de
// sortie : sinon celle-ci sauterait de rangée au gré des sélections.
[[nodiscard]] BarShape WidestActionBar(const SkinArt& art, int* count);

// LA BARRE DE SORTIE : revenir au login, ou quitter le jeu. Son ancre est son
// coin gauche-haut.
[[nodiscard]] CharacterSelectVerb DrawExitBar(const SkinArt& art, ImVec2 top_left, float width);

// LES PAGES, autour de leur ancre — le milieu-haut de la rangée.
void DrawPageBar(CharacterSelectState& state, int pages, const SkinArt& art, ImDrawList* list,
                 ImVec2 middle_top);

}  // namespace ro::ui

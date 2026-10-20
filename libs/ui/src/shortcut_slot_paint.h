#pragma once

// LE DESSIN D'UNE CASE DE RACCOURCI, partagé par la barre et par la roue.
//
// En-tête PRIVÉ de `libs/ui/src`. 🔴 UN SEUL DESSIN POUR LES DEUX : une case de
// la roue qui se peindrait autrement que sa sœur de la barre — un voile de
// recharge d'une autre teinte, un rang écrit ailleurs — se lirait comme une
// autre case, alors que c'est la même.

#include <imgui.h>

#include "ro/ui/shortcut_bar.h"

namespace ro::ui {

// L'arrondi des coins d'une case, en pixels de l'art d'origine.
inline constexpr float kShortcutSlotRounding = 3.0F;

// Ce qui marque une case sous la souris, ou sous un glisser.
struct SlotPaintMarks {
  // Survolée : un voile clair.
  bool hot = false;
  // Cible d'un glisser : un cadre doré, même sur une barre sans cadre.
  bool dropping = false;
  // L'étiquette de touche, en haut à gauche.
  bool show_key = true;
};

// Peint `slot` dans le carré de côté `side` dont `top_left` est le coin :
// fond, icône, voile de recharge, cadre, quantité, touche, rang et décompte,
// selon l'apparence `look` de sa barre.
void PaintShortcutSlot(ImDrawList* into, const ShortcutBarLayout& look,
                       const ShortcutSlotView& slot, ImVec2 top_left, float side,
                       SlotPaintMarks marks);

}  // namespace ro::ui

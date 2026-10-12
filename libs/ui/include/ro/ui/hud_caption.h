#pragma once

#include <imgui.h>

namespace ro::ui {

// LE TEXTE D'UN CADRE DE HUD : centré dans les deux sens, avec une ombre
// portée, et tronqué par « ... » quand il est plus large que le cadre moins
// sa marge.
//
// ⚠ MESURÉ À LA TAILLE DE DESSIN : mesurer à une taille et peindre à une autre
// décale le centrage. L'ombre suit la taille, et ne descend jamais sous un
// pixel — sous un gros texte, une ombre fixe disparaîtrait.

// Ce que la largeur du cadre perd avant qu'un texte ne se tronque.
inline constexpr float kHudCaptionMargin = 4.0F;

void PaintHudCaption(ImDrawList& into, ImVec2 top_left, ImVec2 bottom_right, const char* text,
                     ImU32 color, float pixels);

}  // namespace ro::ui

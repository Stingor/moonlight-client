#pragma once

// Les cellules de table qui ne mentent pas sur ce qu'elles coupent : une
// largeur qui tient le plus long libellé TRADUIT, et le texte entier en bulle
// quand la colonne, réglée plus étroite, le coupe. Interne à `ui`.

#include <initializer_list>
#include <string_view>

#include <imgui.h>

namespace ro::ui {

// La largeur du plus large de ces textes, en pixels d'écran : la largeur
// initiale d'une colonne qui doit les tenir tous. ⚠ TRADUITS PAR L'APPELANT :
// « Récompense MVP » n'a pas la même longueur dans chaque langue.
[[nodiscard]] float WidestText(std::initializer_list<std::string_view> texts);

// La largeur d'un en-tête de colonne TRIABLE : son libellé et la flèche du tri
// qui s'y ajoute.
[[nodiscard]] float SortedHeaderWidth(std::string_view label);

// UN TEXTE QUE SA COLONNE COUPE : plus large que la place qui reste dans la
// cellule. À juger AVANT de l'écrire.
[[nodiscard]] bool OverflowsCell(std::string_view text);

// L'entier d'un texte coupé, en bulle. ⚠ Ne teste pas le survol.
void TooltipWhole(std::string_view text);

// Le texte d'une cellule, dans cette couleur — nulle : celle du texte ; coupé,
// il se lit entier au survol.
void CellText(std::string_view text, ImU32 color = 0);

}  // namespace ro::ui

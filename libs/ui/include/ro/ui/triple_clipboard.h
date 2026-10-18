#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "ro/ui/map_cells.h"

namespace ro::ui {

// TROIS VALEURS AU PRESSE-PAPIERS, en texte : « x y z », deux décimales — de
// quoi recopier une position, une rotation ou une échelle d'un objet à un
// autre, ou ailleurs que dans le jeu.
[[nodiscard]] std::string TripleText(const std::array<float, 3>& values);

// Trois nombres lus dans `text`, séparés par des espaces, des virgules ou des
// points-virgules ; rien quand il n'y en a pas exactement trois.
[[nodiscard]] std::optional<std::array<float, 3>> ParseTriple(std::string_view text);

// Les trois nombres que porte le presse-papiers, s'il en porte trois.
[[nodiscard]] std::optional<std::array<float, 3>> ClipboardTriple();

// LE MENU DU CLIC DROIT sur le champ à trois valeurs qu'on vient de poser :
// « Copier » met `values` au presse-papiers ; « Coller », grisé quand le
// presse-papiers ne porte pas trois nombres, les remplace. `id` distingue les
// menus d'une même fenêtre. Vrai quand Coller a changé `values`.
[[nodiscard]] bool TripleClipboardMenu(const char* id, std::array<float, 3>& values);

// Le même menu sur un champ « Position » en cases (voir `PositionInCells`) : le
// texte porte des cases, `native` reste dans le repère du RSW.
[[nodiscard]] bool PositionClipboardMenu(const char* id, std::array<float, 3>& native,
                                         MapCubes cubes);

}  // namespace ro::ui

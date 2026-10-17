#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// TROIS VALEURS AU PRESSE-PAPIERS, en texte : « x y z », deux décimales — de
// quoi recopier une position, une rotation ou une échelle d'un objet à un
// autre, ou ailleurs que dans le jeu.
[[nodiscard]] std::string TripleText(const std::array<float, 3>& values);

// Trois nombres lus dans `text`, séparés par des espaces, des virgules ou des
// points-virgules ; rien quand il n'y en a pas exactement trois.
[[nodiscard]] std::optional<std::array<float, 3>> ParseTriple(std::string_view text);

// « Copier » et « Coller », deux petits boutons à la suite de la ligne. Copier
// met `values` au presse-papiers ; Coller les remplace par ce qu'il porte,
// s'il porte trois nombres. Vrai quand Coller a changé `values`.
[[nodiscard]] bool CopyPasteTriple(const char* id, std::array<float, 3>& values,
                                   const SkinArt& art);

}  // namespace ro::ui

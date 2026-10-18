#pragma once

// LES PETITS CALCULS DE VALEURS DE L'INTERFACE, en un seul endroit : l'ordre à
// trois issues d'un tri qu'on renverse, et la part d'une jauge en pour cent.
//
// En-tête PRIVÉ de `libs/ui/src`. 🔴 AUCUNE COPIE LOCALE : une copie gardée
// dans un espace anonyme ou dans un sous-espace de famille masque l'aide
// commune SANS ERREUR dans tout l'espace qui l'entoure. Elle ne devient
// ambiguë — donc visible — que déclarée dans `ro::ui` même, avec la même
// signature, et pour les appels faits hors de son espace anonyme.

#include <algorithm>
#include <cstdint>

namespace ro::ui {

// -1, 0 ou 1 selon que `left` vient avant `right`, l'égale ou le suit : un tri
// d'en-tête de colonne se renverse en changeant le signe.
template <typename T>
[[nodiscard]] int CompareValues(const T& left, const T& right) {
  return left < right ? -1 : (right < left ? 1 : 0);
}

// LA PART D'UNE JAUGE EN POUR CENT, bornée à [0, 100], tronquée ; zéro quand la
// jauge n'est pas connue ou n'a rien à diviser.
[[nodiscard]] inline std::int64_t GaugePercent(bool known, std::int64_t current,
                                               std::int64_t most) {
  constexpr std::int64_t kPercent = 100;
  if (!known || most <= 0) {
    return 0;
  }
  return std::clamp<std::int64_t>(current * kPercent / most, 0, kPercent);
}

}  // namespace ro::ui

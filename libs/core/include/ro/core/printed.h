#pragma once

#include <string>

namespace ro::core {

// UN TEXTE À LA MANIÈRE DE `printf`, entier.
//
// Le texte est MESURÉ avant d'être écrit : aucun tampon à dimensionner, donc
// aucune troncature silencieuse quand un nom ou un nombre est plus long que
// prévu.
//
// ⚠ UNE VRAIE FONCTION VARIADIQUE, MARQUÉE COMME `printf` : c'est ce qui fait
// vérifier ses arguments contre le littéral français que `Tr` a reçu
// (`Printed(Tr("Pas assez de %s."), name)`). Un gabarit de fonction perdrait ce
// contrôle.
#if defined(__GNUC__) || defined(__clang__)
[[nodiscard]] __attribute__((format(printf, 1, 2))) std::string Printed(const char* format, ...);
#else
[[nodiscard]] std::string Printed(const char* format, ...);
#endif

}  // namespace ro::core

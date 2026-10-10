#pragma once

#include <cstddef>
#include <limits>

namespace ro::core {

// LA PLACE QU'UN ENTIER PREND ÉCRIT EN DÉCIMAL, signe et zéro final compris.
//
// `digits10` compte les chiffres que le type tient TOUS ; sa plus grande valeur
// en a un de plus. S'y ajoutent le signe et le zéro final : un `int` demande
// donc douze octets, un `std::size_t` vingt-deux.
//
// ⚠ C'EST LA TAILLE DU PIRE CAS, et c'est elle qu'un tampon de `snprintf` doit
// avoir : une valeur « qui ne dépasse jamais trois chiffres » finit un jour
// par en avoir onze, et le texte est alors tronqué sans que rien ne le dise.
template <typename Integer>
inline constexpr std::size_t kDecimalTextBytes =
    static_cast<std::size_t>(std::numeric_limits<Integer>::digits10) + 3;

}  // namespace ro::core

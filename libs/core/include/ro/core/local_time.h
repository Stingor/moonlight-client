#pragma once

// L'HEURE LOCALE D'UN INSTANT, sous Windows comme ailleurs.
//
// `std::localtime` rend un tampon partagé entre les fils ; chaque système offre
// sa variante réentrante, sous un autre nom et avec ses arguments dans l'autre
// ordre. 🔴 LE CHOIX SE FAIT ICI, ET ICI SEULEMENT : un `#ifdef` recopié dans
// chaque fichier qui affiche une date finit par diverger.

#include <ctime>

namespace ro::core {

// Faux quand le système ne sait pas convertir l'instant — hors de ses bornes.
[[nodiscard]] inline bool ToLocalTime(std::time_t instant, std::tm& local) {
#ifdef _WIN32
  return localtime_s(&local, &instant) == 0;
#else
  return localtime_r(&instant, &local) != nullptr;
#endif
}

}  // namespace ro::core

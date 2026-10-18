#pragma once

// LES NOMS DES SIX CARACTÉRISTIQUES, dans l'ordre du client : STR, AGI, VIT,
// INT, DEX, LUK. Ce sont aussi les noms des commandes du serveur, en
// minuscules : `@str`, `@agi`… Ils passent par `core::Tr` sans se traduire :
// ce sont les noms que les joueurs lisent depuis toujours.
//
// En-tête PRIVÉ de `libs/ui/src`.

#include <cstddef>

#include "ro/core/i18n.h"

namespace ro::ui {

// Le nom de la caractéristique de rang `rank` ; LUK au-delà du cinquième.
[[nodiscard]] inline const char* AttributeName(std::size_t rank) {
  // ⚠ DES LITTÉRAUX, un par branche : `core::Tr` veut voir sa chaîne à l'appel.
  switch (rank) {
    case 0:
      return core::Tr("STR");
    case 1:
      return core::Tr("AGI");
    case 2:
      return core::Tr("VIT");
    case 3:
      return core::Tr("INT");
    case 4:
      return core::Tr("DEX");
    default:
      return core::Tr("LUK");
  }
}

}  // namespace ro::ui

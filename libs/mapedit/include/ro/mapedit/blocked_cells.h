#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/formats/gat.h"
#include "ro/mapedit/map_edit.h"

namespace ro::mapedit {

// LES CASES BLOQUÉES DU DOCUMENT, posées sur la grille de marche : voir
// docs/adr/0131. Le serveur bloque les mêmes, et c'est lui qui fait foi.
//
// Une case `kWall` prend le type mur de la GAT, une case `kFence` le type
// crevasse. On ne fait que bloquer : un mur de la GAT reste un mur même sous
// `kFence`, et une crevasse devient un mur sous `kWall`. Une case hors de la
// grille ne se pose pas, et le rapport le dit.
[[nodiscard]] MapEditReport BlockGatCells(std::span<const BlockedCell> cells, formats::GatMap& gat);

// LES CASES SOUS UNE EMPRISE : celles dont le centre tombe dans l'enveloppe
// convexe de `corners`, des points de la grille de marche en cases
// fractionnaires. Le centre d'une case est son numéro entier, comme partout
// où la grille se compte. Rangées par rangée puis par colonne ; rien sous moins
// de trois points qui ne soient alignés.
[[nodiscard]] std::vector<std::array<std::int32_t, 2>> CellsUnderFootprint(
    std::span<const std::array<float, 2>> corners);

}  // namespace ro::mapedit

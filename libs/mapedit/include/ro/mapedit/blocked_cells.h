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

// LE TYPE QU'UNE CASE PREND UNE FOIS BLOQUÉE, depuis son type brut dans la GAT :
// le mur sous `kWall`, la crevasse sous `kFence` — sauf un mur, qui le reste.
// C'est la règle de `BlockGatCells`, pour qui lit une case sans réécrire la
// grille.
[[nodiscard]] std::int32_t BlockedGatType(std::int32_t raw_type, CellBlock block);

// LA CASE BLOQUÉE EN (x, y) parmi `cells`, ou rien. `cells` est rangé comme le
// document range les siennes : par rangée, puis par colonne.
[[nodiscard]] const BlockedCell* FindBlockedCell(std::span<const BlockedCell> cells,
                                                 std::int32_t cell_x, std::int32_t cell_y);

// CELLES DES RANGÉES `first_y` à `last_y`, bornes comprises, d'une liste rangée
// de même : ce qui reste à consulter quand on ne regarde qu'un morceau de la
// carte. Rien quand les bornes se croisent.
[[nodiscard]] std::span<const BlockedCell> BlockedCellsInRows(std::span<const BlockedCell> cells,
                                                              std::int32_t first_y,
                                                              std::int32_t last_y);

// LES CASES SOUS UNE EMPRISE : celles dont le centre tombe dans l'enveloppe
// convexe de `corners`, des points de la grille de marche en cases
// fractionnaires. Le centre d'une case est son numéro entier, comme partout
// où la grille se compte. Rangées par rangée puis par colonne ; rien sous moins
// de trois points qui ne soient alignés.
[[nodiscard]] std::vector<std::array<std::int32_t, 2>> CellsUnderFootprint(
    std::span<const std::array<float, 2>> corners);

}  // namespace ro::mapedit

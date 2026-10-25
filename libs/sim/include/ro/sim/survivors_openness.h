#pragma once

#include <cstdint>
#include <vector>

#include "ro/sim/packed_position.h"
#include "ro/sim/walk_map.h"

namespace ro::sim {

// L'OUVERTURE D'UNE ARÈNE : la part praticable de la carte vue par fenêtres
// carrées, celle où le héros se bat. Une carte de champ trop boisée enferme le
// héros entre des arbres et fait naître la horde hors de sa vue.
//
// La connexité est celle des quatre voisins : une diagonale entre deux murs ne
// relie pas deux régions. La mesure sous-estime donc la marche, jamais elle ne
// la surestime.

// Le côté des fenêtres, en cases : le cadre d'apparition de la horde (30 cases
// d'est en ouest de part et d'autre du héros) tient dans deux fenêtres.
inline constexpr std::int32_t kOpennessWindowCells = 40;

// Une fenêtre de `window_cells` de côté autour d'une case. Pour un côté pair,
// elle couvre de `centre − côté/2` à `centre + côté/2 − 1` sur chaque axe ; ce
// qui sort de la carte compte comme bloqué.
struct OpennessWindow {
  CellPosition centre;
  std::int32_t passable_cells = 0;
  // Les cases praticables atteignables depuis le centre sans sortir de la
  // fenêtre ; zéro quand le centre est bloqué.
  std::int32_t reachable_cells = 0;
};

struct ArenaOpenness {
  std::int32_t window_cells = 0;
  std::int32_t map_cells = 0;
  std::int32_t passable_cells = 0;
  // La plus grande région praticable d'un seul tenant, en cases.
  std::int32_t main_region_cells = 0;
  // Les fenêtres PAVÉES, sans recouvrement et entièrement dans la carte : leur
  // nombre de cases praticables, croissant. Vide si la carte est plus petite
  // qu'une fenêtre.
  std::vector<std::int32_t> tiled_passable;
  // LA MEILLEURE FENÊTRE GLISSANTE : la plus praticable parmi celles dont le
  // centre appartient à la région principale. Ex aequo, la plus proche du
  // centre de la carte (distance de Tchebychev), puis la plus au sud, puis la
  // plus à l'ouest. Centre nul et compte nul pour une carte sans case praticable.
  OpennessWindow best;
};

// Mesure l'ouverture de `map`. `window_cells` est ramené à au moins une case.
[[nodiscard]] ArenaOpenness MeasureArenaOpenness(const WalkMap& map, std::int32_t window_cells);

// La fenêtre de `window_cells` de côté autour de `centre`, cases atteignables
// comprises.
[[nodiscard]] OpennessWindow WindowAround(const WalkMap& map, CellPosition centre,
                                          std::int32_t window_cells);

}  // namespace ro::sim

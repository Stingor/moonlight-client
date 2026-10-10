#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/navi_guide.h"
#include "ro/ui/world_map_window.h"
#include "ro/vfs/world_view.h"

namespace moonclient {

// ── L'ITINÉRAIRE SUR LA CARTE DU MONDE ─────────────────────────────────────

// Les cartes que le guidage traverse, dans l'ordre, la destination en dernier ;
// chacune une fois. Vide sans guidage.
[[nodiscard]] std::vector<std::string> WorldRouteMaps(const ro::sim::NaviGuide* guide);

// La carte de l'étape en cours : celle de la prochaine liaison à franchir, puis
// la destination. Vide sans guidage.
[[nodiscard]] std::string WorldRouteStepMap(const ro::sim::NaviGuide* guide);

// À partir de ce groupe, une carte est un champ ou une ville ; en deçà, l'étage
// d'un donjon, qui n'est pas une étape.
inline constexpr std::int32_t kWorldFieldGroupFloor = 100;

// UNE ÉTAPE, posée sur le premier monde qui la montre comme un champ ou une
// ville.
struct WorldRouteStep {
  std::string_view map_name;
  std::size_t world = 0;
  ro::vfs::WorldRect rect;
};

// Les étapes d'un itinéraire : une carte qu'aucun monde ne montre ainsi n'en
// est pas une.
[[nodiscard]] std::vector<WorldRouteStep> WorldRouteSteps(const ro::vfs::WorldViewTables& tables,
                                                          std::span<const std::string> maps);

// ── LES DIRIGEABLES ─────────────────────────────────────────────────────────
//
// Deux lignes, que le client d'origine écrit en dur : l'internationale
// (Einbroch, Lighthalzen, Juno, Hugel) puis l'intérieure (Izlude, Juno, et le
// champ ra_fild12). Pour chaque escale, le circuit qui en part et y revient.
struct WorldAirTourSpec {
  std::string_view start;
  std::span<const std::string_view> stops;
};

[[nodiscard]] std::span<const WorldAirTourSpec> WorldAirLine(std::size_t line);

}  // namespace moonclient

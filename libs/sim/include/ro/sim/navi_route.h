#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "ro/formats/navi_tables.h"

namespace ro::sim {

// L'ITINÉRAIRE D'UNE CARTE À UNE AUTRE, sur le graphe que le serveur a généré.
//
// Les nœuds sont les LIAISONS — un portail, un PNJ qui transporte — et non les
// cartes : on entre dans une liaison en un point précis d'une carte, on en sort
// en un point précis d'une autre, et la table de distances du serveur dit ce que
// coûte la marche de cette sortie à chacune des liaisons suivantes. Une carte n'a
// pas de « coût » à elle ; ce qu'on paie, c'est la traversée entre deux points.
//
// Ce que le planificateur ajoute à ces tables :
//
//   · le DÉPART est mesuré, pas estimé — l'appelant fournit la vraie marche du
//     joueur jusqu'à chaque sortie de sa carte, calculée sur sa grille ;
//   · l'ARRIVÉE aussi quand c'est possible : vers un PNJ, la table des distances
//     aux PNJ ; vers une case, une estimation sans obstacle faute de grille ;
//   · chaque passage coûte en plus un forfait — l'écran de chargement d'un
//     portail, le dialogue d'un PNJ — pour que, à marche égale, l'itinéraire qui
//     change moins souvent de carte l'emporte.

// Ce que l'itinéraire s'autorise.
struct NaviRouteOptions {
  // Les services de transport : sur Moonlight, le Warp Agent, gratuit et présent
  // partout — le seul moyen d'atteindre les lieux qu'aucun portail ne relie.
  bool allow_transport_services = true;
  bool allow_airport = true;
};

// Le forfait d'un passage, en unités de pas (dix par case droite).
//
// Un portail vaut l'écran de chargement qu'il déclenche, soit à peu près deux
// secondes de marche ; un PNJ ajoute son dialogue.
inline constexpr std::uint32_t kPortalHopCost = 150;
inline constexpr std::uint32_t kDialogHopCost = 300;

enum class NaviGoalKind : std::uint8_t {
  // La carte elle-même : y entrer suffit.
  kMap,
  // Une case précise de la carte.
  kCell,
  // Un PNJ de la table : sa carte et sa case sont les siennes.
  kNpc,
};

struct NaviGoal {
  NaviGoalKind kind = NaviGoalKind::kMap;
  formats::NaviMapIndex map = formats::kNoNaviMap;
  std::int32_t cell_x = 0;
  std::int32_t cell_y = 0;
  // Pour `kNpc` seulement.
  std::uint32_t npc_id = 0;
};

// Le joueur : sa carte et sa case.
struct NaviStart {
  formats::NaviMapIndex map = formats::kNoNaviMap;
  std::int32_t cell_x = 0;
  std::int32_t cell_y = 0;
};

// Ce que coûte, en unités de pas, la marche du joueur jusqu'à une case de SA
// carte. Rien si la case est injoignable. Sans elle, le départ est estimé à vol
// d'oiseau.
using NaviStartCost = std::function<std::optional<std::uint32_t>(std::int32_t, std::int32_t)>;

struct NaviRoute {
  // Les liaisons à franchir, dans l'ordre. Vide quand la destination est sur la
  // carte du joueur : il n'y a alors qu'à marcher.
  std::vector<std::uint32_t> link_ids;
  // Le coût total estimé, en unités de pas, forfaits compris.
  std::uint32_t total_cost = 0;
};

// Rend rien quand aucun itinéraire autorisé ne relie le départ à la destination.
[[nodiscard]] std::optional<NaviRoute> PlanNaviRoute(const formats::NaviData& data,
                                                     const NaviStart& start, const NaviGoal& goal,
                                                     const NaviRouteOptions& options,
                                                     const NaviStartCost& start_cost = {});

// La liaison est-elle permise par ces options ?
[[nodiscard]] bool NaviLinkAllowed(const formats::NaviLink& link,
                                   const NaviRouteOptions& options) noexcept;

// Le forfait d'une liaison selon sa nature.
[[nodiscard]] std::uint32_t NaviHopCost(const formats::NaviLink& link) noexcept;

}  // namespace ro::sim

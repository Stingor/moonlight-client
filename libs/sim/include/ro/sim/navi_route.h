#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "ro/formats/navi_npc_distances.h"
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
//   · l'ARRIVÉE aussi : l'appelant fournit la marche de la sortie de chaque
//     liaison jusqu'à la destination, mesurée sur la grille de la carte visée ou
//     lue dans la table des distances aux PNJ. Sans elle, une case ou un PNJ
//     d'une autre carte n'a pas d'itinéraire ;
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

// Ce que coûte, en unités de pas, la marche depuis la SORTIE d'une liaison, sur
// la carte visée, jusqu'à la destination. Rien : cette liaison ne mène pas à la
// destination, elle n'est pas un dernier passage. Sans elle, aucune liaison ne
// l'est : seule la marche directe, sur la carte du joueur, atteint une case ou
// un PNJ. N'est consultée que pour une case ou un PNJ.
using NaviGoalCost = std::function<std::optional<std::uint32_t>(const formats::NaviLink&)>;

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
                                                     const NaviStartCost& start_cost = {},
                                                     const NaviGoalCost& goal_cost = {});

// L'arrivée lue dans la table des distances aux PNJ, pour le PNJ visé. Une
// liaison absente de sa liste ne mène pas à lui. Vide quand la table ne dit
// rien de ce PNJ : aucune liaison ne mène alors à lui.
[[nodiscard]] NaviGoalCost NaviTableGoalCost(const formats::NaviData& data,
                                             const formats::NaviNpcDistances& table,
                                             std::uint32_t npc_id);

// La liaison est-elle permise par ces options ?
[[nodiscard]] bool NaviLinkAllowed(const formats::NaviLink& link,
                                   const NaviRouteOptions& options) noexcept;

// Le forfait d'une liaison selon sa nature.
[[nodiscard]] std::uint32_t NaviHopCost(const formats::NaviLink& link) noexcept;

}  // namespace ro::sim

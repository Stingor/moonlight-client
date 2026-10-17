#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "ro/sim/actor.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/pending_action.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"

namespace ro::sim {

// LA MARCHE AU-DELÀ DE CE QUE LE SERVEUR ACCEPTE.
//
// Le serveur refuse, sans rien répondre, une marche dont SON chemin dépasse
// `max_walk_path` pas (`SessionSettings::max_walk_path_steps`). moonclient voit
// et clique plus loin que cela : une destination lointaine se rejoint par
// ÉTAPES, chacune une marche que le serveur accepte, et la suivante se demande
// quelques pas avant la fin de la précédente, pour que le personnage ne marque
// pas d'arrêt.

// Les pas qui restent à la marche en cours quand l'étape suivante se demande.
inline constexpr std::size_t kNextLegLeadSteps = 3;

// Le délai au-delà duquel une étape que le serveur n'a pas fait partir est
// tenue pour refusée, en millisecondes.
inline constexpr std::uint32_t kLegConfirmMs = 1000;

// Le chemin COMPLET de `from` à `to`, sans la borne de path_search : A* sur
// toute la carte, avec ses coûts et son refus de couper un coin. Les cases
// traversées, départ exclu, arrivée comprise ; nullopt si rien ne les relie.
//
// ⚠ CE N'EST PAS LE CHEMIN DU SERVEUR, seulement un fil à suivre : chaque étape
// prise dessus est vérifiée contre la copie fidèle, `FindWalkPath`.
[[nodiscard]] std::optional<std::vector<CellPosition>> FindRoute(const WalkMap& map,
                                                                 CellPosition from,
                                                                 CellPosition to);

// LA CASE À DEMANDER MAINTENANT pour aller de `from` vers `to` : `to` elle-même
// si le serveur y mène en au plus `max_steps` pas ; sinon la case la plus
// avancée du chemin complet qu'il atteint ainsi ; sinon `to`, et le serveur
// tranchera comme il l'aurait fait.
[[nodiscard]] CellPosition WalkStopToward(const WalkMap& map, CellPosition from, CellPosition to,
                                          std::size_t max_steps);

// Les pas qu'une étape peut compter : ceux du serveur, moins un pour la case
// d'où il repartira, qu'il prend au pas suivant de la marche en cours.
[[nodiscard]] std::size_t LegSteps(const World& world) noexcept;

// L'écart de Tchebychev entre deux cases : le moins de pas qui les sépare.
[[nodiscard]] std::size_t StepsApart(CellPosition from, CellPosition to) noexcept;

// Le serveur a-t-il fait marcher l'acteur vers `stop` depuis la demande
// partie à `sent_ms` ?
[[nodiscard]] bool LegTaken(const Actor& actor, CellPosition stop, std::uint32_t sent_ms) noexcept;

// Vrai quand la marche en cours de l'acteur a encore plus de
// `kNextLegLeadSteps` pas devant elle : l'étape suivante peut attendre.
[[nodiscard]] bool LegStillRunning(const Actor& actor, const WalkPath& path, std::uint32_t now_ms);

// UN CLIC AU SOL LOINTAIN, en cours de route.
struct LongWalk {
  CellPosition destination;
  // L'étape demandée en dernier, et l'instant de la demande.
  CellPosition stop;
  std::uint32_t sent_ms = 0;
  // L'entrée sur carte où elle a commencé : changer de carte l'oublie.
  std::uint32_t map_entry = 0;
};

// Un clic au sol vers `destination` part maintenant : rend la case à demander
// au serveur. Au-delà d'une marche, c'est une étape, et `walk` retient la
// destination ; sinon `walk` s'efface.
[[nodiscard]] CellPosition BeginLongWalk(const World& world, std::optional<LongWalk>& walk,
                                         CellPosition destination, std::uint32_t now_ms);

// L'étape suivante d'un clic lointain, à demander maintenant, ou rien.
//
// La marche par étapes s'efface d'elle-même quand le serveur ne fait pas
// partir une étape (`kLegConfirmMs`), quand une AUTRE marche a pris le
// personnage — une attaque, un ramassage, un nouveau clic —, quand la dernière
// étape est partie, et au changement de carte. Elle MUTE `clock`, comme toute
// marche : voir `AdmitWalkRequest`.
[[nodiscard]] std::optional<CellPosition> AdvanceLongWalk(const World& world, WalkPlanner& planner,
                                                          std::optional<LongWalk>& walk,
                                                          ApproachWalkClock& clock,
                                                          std::uint32_t now_ms);

}  // namespace ro::sim

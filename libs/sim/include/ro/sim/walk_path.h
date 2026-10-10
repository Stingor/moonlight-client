#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "ro/sim/actor.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/world_fwd.h"

namespace ro::sim {

// Le chemin d'une marche, tel que le SERVEUR le calcule.
//
// Le serveur n'annonce que les deux bouts d'un déplacement, mais il fait
// marcher l'acteur case par case le long d'un chemin qu'il a calculé lui-même
// : path_search, dans src/map/path.cpp. Un client qui en calcule un autre
// montre le personnage ailleurs que là où le serveur le tient — à travers un
// mur, ou en coupant le coin que le serveur contourne. Ce module est donc la
// COPIE de path_search, tas binaire et ordre des voisins compris : deux
// chemins de même coût se départagent par l'ordre d'exploration, et c'est cet
// ordre qu'il faut avoir, pas seulement le coût.

// MAX_WALKPATH : au-delà, le serveur refuse la marche.
inline constexpr std::size_t kMaxWalkSteps = 32;

// MOVE_COST et MOVE_DIAGONAL_COST : le coût d'un pas dans la recherche, et le
// rapport de durée d'un pas en diagonale.
inline constexpr std::uint32_t kMoveCost = 10;
inline constexpr std::uint32_t kMoveDiagonalCost = 14;

// MIN_WALK_SPEED : le pas le plus COURT que le serveur accepte, en
// millisecondes par case (src/common/mmo.hpp). C'est-à-dire la marche la plus
// RAPIDE — cinquante cases par seconde.
//
// ⚠ CE N'EST PAS UN CAS TORDU MAIS UNE BORNE DU PROTOCOLE. `@speed 20` s'y
// pose exactement, et tout ce que le client calibre sur une vitesse — le
// rattrapage de la caméra en premier — doit tenir jusque-là. Un réglage
// silencieusement calibré sur DEFAULT_WALK_SPEED, cent cinquante, marche pour
// la vitesse ordinaire et se défait à la borne, là où personne ne l'essaie.
inline constexpr std::uint16_t kFastestStepMs = 20;

// Une suite de directions à partir d'une case ; voir DirectionTowards pour la
// numérotation. Vide quand il n'y a pas de pas à faire.
struct WalkPath {
  std::vector<std::uint8_t> directions;
};

[[nodiscard]] bool IsDiagonal(std::uint8_t direction) noexcept;

// La case voisine dans cette direction.
[[nodiscard]] CellPosition Stepped(CellPosition from, std::uint8_t direction) noexcept;

// La durée d'un pas : la vitesse du serveur, et quatorze dixièmes de plus en
// diagonale — en entier, comme lui (unit_walktoxy_sub, src/map/unit.cpp).
[[nodiscard]] std::uint32_t StepDurationMs(std::uint16_t speed_ms_per_cell,
                                           std::uint8_t direction) noexcept;

// Le chemin que le serveur calcule pour un joueur : A* avec l'heuristique du
// client, qui SURESTIME, et le refus de couper les coins. nullopt si la
// destination est bloquée ou hors carte, si rien ne la relie au départ, ou si
// le chemin dépasse kMaxWalkSteps — le serveur refuse alors la marche.
//
// ⚠ Copie littérale, jusque dans une limite de path_search : sa table de
// nœuds indexe les cases modulo 32 par axe, et deux cases qui s'y heurtent
// font échouer la recherche. Le reproduire est plus fidèle que le corriger.
[[nodiscard]] std::optional<WalkPath> FindWalkPath(const WalkMap& map, CellPosition from,
                                                   CellPosition to);

// Le « chemin facile » du serveur : en diagonale d'abord, puis tout droit, et
// échec au premier obstacle. C'est celui de certains monstres ; les joueurs
// n'y passent jamais.
[[nodiscard]] std::optional<WalkPath> FindEasyWalkPath(const WalkMap& map, CellPosition from,
                                                       CellPosition to);

// La même marche, sans carte : diagonale d'abord puis tout droit, obstacles
// ignorés. C'est le repli quand la carte est inconnue ou refuse le chemin que
// le serveur a pourtant annoncé.
[[nodiscard]] WalkPath StraightWalkPath(CellPosition from, CellPosition to);

// Les chemins de tous les acteurs, calculés une fois par déplacement annoncé.
//
// Le monde ne porte que les deux bouts ; le chemin s'en déduit, et se recalcule
// quand ils changent — pas à chaque image. Sans carte, tout le monde marche en
// ligne droite.
class WalkPlanner {
 public:
  explicit WalkPlanner(const WalkMap* map) : map_(map) {}

  // Le chemin de l'acteur ; vide s'il ne se déplace pas.
  [[nodiscard]] const WalkPath& PathFor(const Actor& actor);

  void Clear() { planned_.clear(); }

 private:
  struct Planned {
    CellMovement movement;
    std::uint32_t started_ms = 0;
    WalkPath path;
  };

  const WalkMap* map_;
  std::unordered_map<std::uint32_t, Planned> planned_;
  WalkPath no_path_;
};

}  // namespace ro::sim

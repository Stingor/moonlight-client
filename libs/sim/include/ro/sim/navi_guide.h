#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/formats/navi_tables.h"
#include "ro/sim/navi_route.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/walk_field.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/walk_path.h"

namespace ro::sim {

// LE GUIDAGE EN COURS : un itinéraire, et où en est le joueur.
//
// Ce que le client d'origine fait mal, et que ce module corrige :
//
//   · il calcule le chemin DANS la carte une fois, puis ne le refait que quand
//     le joueur s'en est tant écarté qu'aucune trace ne se voit plus. Ici le
//     champ part de la CIBLE : le chemin se relit depuis la case du joueur à
//     chaque pas, détours compris, sans rien recalculer ;
//   · il suit une suite de cartes figée. Ici, entrer sur une carte qui n'est pas
//     la suivante relance le calcul depuis là — le joueur a pris un autre
//     portail, l'itinéraire le suit ;
//   · il ne dit pas qu'on est ARRIVÉ. Ici l'arrivée se constate, et le guidage
//     s'arrête de lui-même ;
//   · il ne distingue pas « aucun chemin » de « déjà sur place ». Ici si.

enum class NaviGuideStatus : std::uint8_t {
  kIdle,
  // Une demande attend les données, la grille ou la position du joueur.
  kPending,
  kGuiding,
  kNoRoute,
  // La demande visait la carte où l'on se trouve déjà.
  kAlreadyHere,
  kArrived,
};

// Ce qu'on demande : une carte par son nom interne, éventuellement une case ou un
// PNJ dessus.
struct NaviGuideRequest {
  std::string map_name;
  NaviGoalKind kind = NaviGoalKind::kMap;
  std::int32_t cell_x = 0;
  std::int32_t cell_y = 0;
  std::uint32_t npc_id = 0;
  NaviRouteOptions options;
};

// CE QUE LE GUIDAGE SAIT DE LA CARTE VISÉE quand ce n'est pas celle du joueur.
// L'application le fournit : lire une grille demande l'archive.
//
// La marche jusqu'à une case ou un PNJ se mesure sur la GRILLE de la carte
// visée : une inondation depuis la destination donne d'un coup la marche depuis
// chaque arrivée de liaison. Sans grille lisible, la destination n'a pas
// d'itinéraire.
class NaviGoalTerrain {
 public:
  virtual ~NaviGoalTerrain() = default;
  // La grille de marche d'une carte, nulle si elle ne se lit pas. Le pointeur
  // vaut jusqu'à l'appel suivant.
  [[nodiscard]] virtual const WalkMap* GridOf(std::string_view map_name) = 0;
};

// Une étape : une liaison à franchir.
struct NaviGuideStep {
  std::uint32_t link_id = 0;
  formats::NaviMapIndex from_map = formats::kNoNaviMap;
  formats::NaviMapIndex to_map = formats::kNoNaviMap;
  std::int32_t exit_x = 0;
  std::int32_t exit_y = 0;
  std::uint16_t link_kind = 0;
};

// À moins de cette marche de la destination, on y est.
inline constexpr std::uint32_t kNaviArrivalCost = 3 * kMoveCost;
// Rayon de recherche d'une case praticable autour d'une cible posée sur un
// décor.
inline constexpr std::int32_t kNaviTargetRing = 4;

class NaviGuide {
 public:
  void SetData(std::shared_ptr<const formats::NaviData> data);
  // Nul : une case ou un PNJ d'une autre carte que celle du joueur n'a pas
  // d'itinéraire.
  void SetGoalTerrain(NaviGoalTerrain* terrain) noexcept { goal_terrain_ = terrain; }
  [[nodiscard]] const formats::NaviData* data() const noexcept { return data_.get(); }

  void Request(NaviGuideRequest request);
  void Stop();

  // À appeler à chaque image : la carte courante, sa grille (nulle tant qu'elle
  // n'est pas chargée) et la case du joueur.
  void Update(std::string_view map_name, const WalkMap* walk, CellPosition player);

  [[nodiscard]] NaviGuideStatus status() const noexcept { return status_; }
  [[nodiscard]] bool active() const noexcept {
    return status_ == NaviGuideStatus::kGuiding || status_ == NaviGuideStatus::kPending;
  }
  [[nodiscard]] const std::vector<NaviGuideStep>& steps() const noexcept { return steps_; }
  // L'étape à franchir depuis la carte courante ; `steps().size()` sur la carte
  // d'arrivée.
  [[nodiscard]] std::size_t current_step() const noexcept { return current_step_; }
  [[nodiscard]] const NaviGuideRequest& request() const noexcept { return request_; }
  [[nodiscard]] formats::NaviMapIndex goal_map() const noexcept { return goal_map_; }
  [[nodiscard]] std::uint32_t estimated_cost() const noexcept { return estimated_cost_; }

  // Sur la carte courante : la cible locale (la sortie à prendre, ou la
  // destination), et le chemin du joueur jusqu'à elle réduit à ses coins.
  [[nodiscard]] std::optional<CellPosition> local_target() const noexcept { return local_target_; }
  [[nodiscard]] const std::vector<CellPosition>& path_corners() const noexcept {
    return path_corners_;
  }
  // Le chemin complet, case par case, pour la trace au sol.
  [[nodiscard]] const std::vector<CellPosition>& path_cells() const noexcept { return path_cells_; }

 private:
  void Plan(formats::NaviMapIndex here, const WalkMap* walk, CellPosition player);
  void AimLocally(formats::NaviMapIndex here, const WalkMap* walk);
  void Trace(CellPosition player);

  std::shared_ptr<const formats::NaviData> data_;
  NaviGoalTerrain* goal_terrain_ = nullptr;
  NaviGuideRequest request_;
  NaviGuideStatus status_ = NaviGuideStatus::kIdle;
  formats::NaviMapIndex goal_map_ = formats::kNoNaviMap;
  std::vector<NaviGuideStep> steps_;
  std::size_t current_step_ = 0;
  std::uint32_t estimated_cost_ = 0;

  // La carte sur laquelle `field_` a été calculé, et la grille qui l'a servi.
  formats::NaviMapIndex field_map_ = formats::kNoNaviMap;
  const WalkMap* field_walk_ = nullptr;
  WalkField field_;
  std::optional<CellPosition> local_target_;
  std::optional<CellPosition> traced_from_;
  std::vector<CellPosition> path_cells_;
  std::vector<CellPosition> path_corners_;
};

}  // namespace ro::sim

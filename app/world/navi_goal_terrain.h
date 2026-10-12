#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "ro/sim/navi_guide.h"
#include "ro/sim/walk_map.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// LA CARTE VISÉE PAR LE GUIDAGE, lue dans l'archive de la partie quand il la
// demande : sa GRILLE, depuis son .gat — quelques dizaines de Kio, lus au
// calcul d'un itinéraire. La dernière lue est gardée, puisqu'un nouveau calcul
// vise d'ordinaire la même carte.
//
// ⚠ SUR LE FIL DE RENDU, avec l'archive de la partie : le guidage se calcule
// là. Une grille coûte quelques millisecondes.
class NaviGoalLoader final : public ro::sim::NaviGoalTerrain {
 public:
  // Nulle : rien ne se lit.
  void Attach(ro::vfs::Vfs* vfs) noexcept { vfs_ = vfs; }

  [[nodiscard]] const ro::sim::WalkMap* GridOf(std::string_view map_name) override;

 private:
  struct Grid {
    std::string map_name;
    // Vide quand la carte n'a pas de grille lisible : on ne la relit pas.
    std::optional<ro::sim::WalkMap> walk;
  };

  ro::vfs::Vfs* vfs_ = nullptr;
  std::optional<Grid> grid_;
};

}  // namespace moonclient

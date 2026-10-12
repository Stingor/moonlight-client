#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "ro/formats/navi_npc_distances.h"
#include "ro/formats/navi_tables.h"
#include "ro/sim/navi_guide.h"
#include "ro/sim/walk_map.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// LA CARTE VISÉE PAR LE GUIDAGE, lue dans l'archive de la partie quand il la
// demande.
//
// · Sa GRILLE, depuis son .gat : quelques dizaines de Kio, lus au calcul d'un
//   itinéraire. La dernière lue est gardée, puisqu'un nouveau calcul vise
//   d'ordinaire la même carte.
// · La TABLE DES DISTANCES AUX PNJ, repli d'une carte sans grille : lue et
//   indexée à la première demande seulement, puis gardée ; le bloc de la
//   dernière carte demandée est gardé analysé. Une lecture qui a échoué ne se
//   retente pas de la partie.
//
// ⚠ SUR LE FIL DE RENDU, avec l'archive de la partie : le guidage se calcule
// là. Une grille coûte quelques millisecondes ; la table, cinq mégaoctets, ne se
// paie qu'au repli.
class NaviGoalLoader final : public ro::sim::NaviGoalTerrain {
 public:
  // Nulle : rien ne se lit.
  void Attach(ro::vfs::Vfs* vfs) noexcept { vfs_ = vfs; }

  [[nodiscard]] const ro::sim::WalkMap* GridOf(std::string_view map_name) override;
  [[nodiscard]] const ro::formats::NaviNpcDistances* NpcDistancesOn(
      const ro::formats::NaviData& data, std::string_view map_name) override;

 private:
  struct Grid {
    std::string map_name;
    // Vide quand la carte n'a pas de grille lisible : on ne la relit pas.
    std::optional<ro::sim::WalkMap> walk;
  };
  struct Distances {
    std::string map_name;
    std::optional<ro::formats::NaviNpcDistances> table;
  };

  ro::vfs::Vfs* vfs_ = nullptr;
  std::optional<Grid> grid_;
  bool table_tried_ = false;
  std::optional<ro::formats::NaviNpcDistanceFile> table_file_;
  std::optional<Distances> distances_;
};

}  // namespace moonclient

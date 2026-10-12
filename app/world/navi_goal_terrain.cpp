#include "world/navi_goal_terrain.h"

#include <string>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/formats/gat.h"
#include "ro/vfs/navi_library.h"

namespace moonclient {
namespace {

constexpr std::string_view kGatExtension = ".gat";

}  // namespace

const ro::sim::WalkMap* NaviGoalLoader::GridOf(std::string_view map_name) {
  if (vfs_ == nullptr || map_name.empty()) return nullptr;
  const std::string wanted = ro::core::AsciiLower(map_name);
  if (!grid_.has_value() || grid_->map_name != wanted) {
    grid_ = Grid{.map_name = wanted, .walk = std::nullopt};
    // Le nom de la carte se traduit pour lui-même : une carte clonée prend la
    // grille d'une autre.
    auto bytes = vfs_->Read(vfs_->res_names().MapFilePath(wanted + std::string(kGatExtension)));
    if (bytes) {
      if (auto gat = ro::formats::GatMap::Parse(*bytes)) {
        grid_->walk = ro::sim::WalkMap::FromGat(*gat);
      } else {
        ro::core::LogInfo("navigation : grille de {} illisible ({})", wanted,
                          ro::core::Describe(gat.error()));
      }
    } else {
      ro::core::LogInfo("navigation : grille de {} absente ({})", wanted,
                        ro::core::Describe(bytes.error()));
    }
  }
  return grid_->walk.has_value() ? &*grid_->walk : nullptr;
}

const ro::formats::NaviNpcDistances* NaviGoalLoader::NpcDistancesOn(
    const ro::formats::NaviData& data, std::string_view map_name) {
  if (vfs_ == nullptr) return nullptr;
  if (!table_tried_) {
    table_tried_ = true;
    if (auto file = ro::vfs::LoadNaviNpcDistanceFile(*vfs_)) {
      ro::core::LogInfo("navigation : distances aux PNJ indexées — {} cartes, {} octets",
                        file->map_count(), file->byte_count());
      if (file->steps() == ro::formats::NaviSteps::kModulo256)
        ro::core::LogWarning(
            "navigation : distances aux PNJ d'un ancien générateur (longueurs modulo 256, "
            "redressées) — à régénérer");
      table_file_ = std::move(*file);
    } else {
      ro::core::LogWarning("navigation : distances aux PNJ illisibles ({})",
                           ro::core::Describe(file.error()));
    }
  }
  if (!table_file_.has_value()) return nullptr;
  const std::string wanted = ro::core::AsciiLower(map_name);
  if (!distances_.has_value() || distances_->map_name != wanted) {
    distances_ = Distances{.map_name = wanted, .table = std::nullopt};
    if (auto table = table_file_->ReadMap(wanted, data)) {
      distances_->table = std::move(*table);
    } else {
      ro::core::LogWarning("navigation : {}", ro::core::Describe(table.error()));
    }
  }
  return distances_->table.has_value() ? &*distances_->table : nullptr;
}

}  // namespace moonclient

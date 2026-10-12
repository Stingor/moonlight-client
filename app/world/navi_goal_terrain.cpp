#include "world/navi_goal_terrain.h"

#include <string>

#include "ro/core/ascii.h"
#include "ro/core/error.h"
#include "ro/core/log.h"
#include "ro/formats/gat.h"

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

}  // namespace moonclient

#include "survivors/arena_choice.h"

#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/formats/gat.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors/arena_music.h"

namespace moonclient {
namespace {

constexpr std::string_view kMapFileExtension = ".gat";

[[nodiscard]] std::string GatFileOf(std::string_view map_name) {
  std::string file(map_name);
  file += kMapFileExtension;
  return file;
}

}  // namespace

ro::core::Result<std::optional<ro::sim::WalkMap>> ArenaWalkOf(ro::vfs::Vfs* vfs,
                                                              const ro::sim::ArenaRule& arena) {
  if (arena.terrain_id != 0) return std::optional<ro::sim::WalkMap>();
  if (vfs == nullptr) {
    return ro::core::Fail(ro::core::ErrorCode::kNotFound,
                          "arène " + std::string(arena.key) + " : aucune installation à lire");
  }
  auto bytes = vfs->Read(vfs->res_names().MapFilePath(GatFileOf(arena.map_name)));
  if (!bytes) return std::unexpected(bytes.error());
  auto gat = ro::formats::GatMap::Parse(*bytes);
  if (!gat) return std::unexpected(gat.error());
  return std::optional<ro::sim::WalkMap>(ro::sim::WalkMap::FromGat(*gat));
}

const BiomeLook* ArenaLookOf(const ro::sim::SurvivorsRules& rules,
                             const ro::sim::ArenaRule& arena) {
  if (arena.terrain_id == 0) return nullptr;
  const ro::sim::TerrainRule* terrain = ro::sim::FindTerrain(rules, arena.terrain_id);
  return terrain != nullptr ? FindBiomeLook(terrain->key) : nullptr;
}

std::string ArenaNameOf(const ro::sim::SurvivorsRules& rules, const ro::sim::ArenaRule& arena,
                        const ro::vfs::MapDisplayNames* names) {
  if (const BiomeLook* look = ArenaLookOf(rules, arena); look != nullptr) {
    return ro::core::Tr(look->display_name);
  }
  if (names != nullptr) {
    if (const std::optional<std::string_view> shown =
            names->DisplayNameOf(GatFileOf(arena.map_name));
        shown.has_value() && !shown->empty()) {
      return std::string(*shown);
    }
  }
  return arena.key;
}

std::optional<ro::vfs::MapSign> ArenaSignOf(const ro::sim::SurvivorsRules& rules,
                                            const ro::sim::ArenaRule& arena,
                                            const ro::vfs::MapDisplayNames* names) {
  if (const BiomeLook* look = ArenaLookOf(rules, arena); look != nullptr) {
    return ro::vfs::MapSign{.background = std::string(look->sign_background),
                            .title = ro::core::Tr(look->display_name),
                            .announced = true};
  }
  if (arena.terrain_id != 0 || names == nullptr) return std::nullopt;
  const ro::vfs::MapSign* sign = names->SignOf(GatFileOf(arena.map_name));
  if (sign == nullptr) return std::nullopt;
  return *sign;
}

std::string_view ArenaFieldTrackOf(const ro::sim::SurvivorsRules& rules,
                                   const ro::sim::ArenaRule& arena) {
  const BiomeLook* look = ArenaLookOf(rules, arena);
  return look != nullptr && !look->field_track.empty() ? look->field_track : kArenaFieldTrack;
}

}  // namespace moonclient

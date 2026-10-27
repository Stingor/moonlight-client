#include "survivors/arena_ground_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

#include "ro/formats/gat.h"
#include "ro/formats/gnd.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/scene_light.h"
#include "ro/render/terrain_mesh.h"
#include "ro/vfs/encoding.h"

namespace moonclient {
namespace {

using ro::core::ErrorCode;
using ro::core::Fail;
using ro::core::Result;

// LE SOL SYNTHÉTIQUE s'écrit en GND 1.7, comme les champs de Prontera : des
// lightmaps et des tuiles, pas d'eau ; et le décor en RSW 2.1.
constexpr std::uint8_t kGndMajor = 1;
constexpr std::uint8_t kGndMinor = 7;
constexpr float kGndProportionRatio = 10.0F;
constexpr std::uint8_t kRswMajor = 2;
constexpr std::uint8_t kRswMinor = 1;
// Un texel de lightmap : son ombre, puis trois octets de couleur, nuls ici.
constexpr std::size_t kLightmapColorBytes = 3;
constexpr std::int32_t kGatWalkable = 0;
constexpr std::int32_t kGatBlocked = 1;
// Une case vaut la moitié d'un cube.
constexpr float kCellUnits = ro::render::kWorldUnitsPerCube / kGroundCellsPerCube;
constexpr float kTextureQuarter = 1.0F / kGroundTextureCubes;
constexpr std::uint8_t kQuarterTurns = 4;
// UN MODÈLE SE POSE SUR LE PLUS BAS du sol à une demi-case autour de son
// pied : sur une ondulation, il ne flotte d'aucun côté.
constexpr float kFootReachCells = 0.5F;

// Le nom `utf8` tel que l'installation l'écrit.
Result<std::string> RawName(std::string_view utf8) {
  auto raw = ro::vfs::Utf8ToCp949(utf8);
  if (!raw) {
    return Fail(ErrorCode::kInvalidArgument,
                "sol généré : nom intraduisible en CP949 — " + std::string(utf8));
  }
  return std::move(*raw);
}

ro::formats::RswSceneHeader SyntheticHeader() {
  ro::formats::RswSceneHeader header;
  header.major_version = kRswMajor;
  header.minor_version = kRswMinor;
  return header;
}

// Le point (x, y) d'un motif de 4 × 4 cubes, compté en cubes depuis son coin
// sud-ouest, après `turn` quarts de tour : ses coordonnées de texture.
std::pair<float, float> MotifPoint(std::int32_t x, std::int32_t y, std::uint8_t turn) {
  const std::int32_t side = kGroundTextureCubes;
  std::int32_t u = x;
  std::int32_t v = y;
  switch (turn % kQuarterTurns) {
    case 1:
      u = y;
      v = side - x;
      break;
    case 2:
      u = side - x;
      v = side - y;
      break;
    case 3:
      u = side - y;
      v = x;
      break;
    default:
      break;
  }
  return {static_cast<float>(u) * kTextureQuarter, static_cast<float>(v) * kTextureQuarter};
}

// LE SOL DU PLAN, en pièces de GND : une tuile par cube, sa lightmap, le quart
// de sa texture.
ro::formats::GndParts GroundOf(const GroundSources& sources, const GroundPlan& plan) {
  ro::formats::GndParts parts{.major_version = kGndMajor,
                              .minor_version = kGndMinor,
                              .width = kGroundWindowCubes,
                              .height = kGroundWindowCubes,
                              .proportion_ratio = kGndProportionRatio,
                              .texture_paths = sources.texture_paths,
                              .lightmap_width = kGroundLightmapSide,
                              .lightmap_height = kGroundLightmapSide};
  parts.lightmap_data.reserve(plan.lightmaps.size() *
                              static_cast<std::size_t>(kGroundLightmapTexels) *
                              (1 + kLightmapColorBytes));
  for (const PlannedShade& shade : plan.lightmaps) {
    parts.lightmap_data.insert(parts.lightmap_data.end(), shade.begin(), shade.end());
    parts.lightmap_data.insert(
        parts.lightmap_data.end(),
        static_cast<std::size_t>(kGroundLightmapTexels) * kLightmapColorBytes, 0);
  }
  parts.tiles.reserve(plan.cubes.size());
  parts.cubes.reserve(plan.cubes.size());
  for (const PlannedCube& cube : plan.cubes) {
    ro::formats::GndTile tile;
    // Les coins dans l'ordre du fichier : sud-ouest, sud-est, nord-ouest,
    // nord-est, chacun à sa place dans le motif tourné.
    for (std::size_t corner = 0; corner < tile.u.size(); ++corner) {
      const auto [u, v] =
          MotifPoint(cube.quarter_east + static_cast<std::int32_t>(corner % 2),
                     cube.quarter_north + static_cast<std::int32_t>(corner / 2), cube.turn);
      tile.u[corner] = u;
      tile.v[corner] = v;
    }
    parts.tiles.push_back(
        ro::formats::GndTile{.u = tile.u,
                             .v = tile.v,
                             .texture_index = static_cast<std::int16_t>(cube.texture),
                             .lightmap_index = static_cast<std::uint16_t>(cube.lightmap),
                             .color = cube.color});
    parts.cubes.push_back(
        ro::formats::GndCube{.corner_height = cube.corner_height,
                             .tile_up = static_cast<std::int32_t>(parts.tiles.size() - 1)});
  }
  return parts;
}

std::vector<ro::formats::GatCell> CellsOf(const GroundPlan& plan) {
  std::vector<ro::formats::GatCell> cells;
  cells.reserve(plan.passable.size());
  for (std::size_t rank = 0; rank < plan.passable.size(); ++rank) {
    cells.push_back(
        ro::formats::GatCell{.corner_height = plan.cell_heights[rank],
                             .type = plan.passable[rank] != 0 ? kGatWalkable : kGatBlocked});
  }
  return cells;
}

// L'altitude du monde au point (`east`, `north`) en cases de la fenêtre.
float WorldHeightAt(const ro::formats::GndMap& gnd, float east, float north) {
  const float depth = static_cast<float>(gnd.height()) * ro::render::kWorldUnitsPerCube;
  return ro::render::GroundHeightAt(gnd, east * kCellUnits,
                                    depth + ro::render::kWorldUnitsPerCube - north * kCellUnits);
}

// LES POSES DU PLAN, dans le repère du RSW : relatives au centre de la carte,
// la hauteur comptée vers le bas comme dans le fichier.
std::vector<ro::formats::RswModel> ModelsOf(const GroundSources& sources, const GroundPlan& plan,
                                            const ro::formats::GndMap& gnd) {
  const float half_width = static_cast<float>(gnd.width()) * ro::render::kWorldUnitsPerCube / 2.0F;
  const float half_depth = static_cast<float>(gnd.height()) * ro::render::kWorldUnitsPerCube / 2.0F;
  std::vector<ro::formats::RswModel> models;
  models.reserve(plan.poses.size());
  for (const PlannedPose& pose : plan.poses) {
    const auto file = sources.model_files.find(pose.model);
    if (file == sources.model_files.end()) continue;
    float lowest = WorldHeightAt(gnd, pose.east, pose.north);
    for (const float dx : {-kFootReachCells, kFootReachCells}) {
      for (const float dy : {-kFootReachCells, kFootReachCells}) {
        lowest = std::min(lowest, WorldHeightAt(gnd, pose.east + dx, pose.north + dy));
      }
    }
    ro::formats::RswModel model;
    model.model_name = file->second;
    model.position = {pose.east * kCellUnits - half_width, -lowest,
                      pose.north * kCellUnits - half_depth};
    model.rotation = {0.0F, pose.yaw_degrees, 0.0F};
    model.scale = {pose.scale, pose.scale, pose.scale};
    models.push_back(std::move(model));
  }
  return models;
}

// Chaque fichier de modèle que le biome cite, une fois.
std::vector<std::string_view> ModelsCited(const BiomeLook& look) {
  std::vector<std::string_view> cited;
  for (const PieceLook& piece : look.pieces) {
    for (const PieceModelSlot& slot : piece.slots) {
      for (const GroundModelChoice& choice : slot.models) cited.push_back(choice.model);
    }
  }
  for (const SeedLook& seed : look.seeds) cited.push_back(seed.model);
  std::ranges::sort(cited);
  const auto [first, last] = std::ranges::unique(cited);
  cited.erase(first, last);
  return cited;
}

}  // namespace

Result<GroundSources> OpenGroundSources(ro::vfs::Vfs& vfs, const ro::sim::TerrainRule& rule,
                                        std::uint64_t run_seed, const BiomeLook& look,
                                        ro::core::LoadProgress* progress) {
  const std::string reference(look.reference_map);
  auto rsw_bytes = vfs.Read(vfs.res_names().MapFilePath(reference + ".rsw"));
  if (!rsw_bytes) return std::unexpected(rsw_bytes.error());
  auto reference_rsw = ro::formats::RswScene::Parse(*rsw_bytes);
  if (!reference_rsw) return std::unexpected(reference_rsw.error());

  const ro::formats::RswLight light = reference_rsw->light();
  std::vector<std::string> texture_paths;
  for (const GroundTextureLook& texture : look.textures) {
    auto raw = RawName(texture.texture);
    if (!raw) return std::unexpected(raw.error());
    texture_paths.push_back(std::move(*raw));
  }
  std::map<std::string_view, std::string> model_files;
  std::vector<ro::formats::RswModel> every_model;
  for (const std::string_view model : ModelsCited(look)) {
    auto raw = RawName(model);
    if (!raw) return std::unexpected(raw.error());
    ro::formats::RswModel cited;
    cited.model_name = *raw;
    every_model.push_back(std::move(cited));
    model_files.emplace(model, std::move(*raw));
  }

  // LA BIBLIOTHÈQUE : un sol d'un cube qui cite toutes les textures, un décor
  // qui cite tous les modèles. Ce qu'elle lit, les fenêtres le reprennent.
  auto ground =
      ro::formats::GndMap::Assemble(ro::formats::GndParts{.major_version = kGndMajor,
                                                          .minor_version = kGndMinor,
                                                          .width = 1,
                                                          .height = 1,
                                                          .proportion_ratio = kGndProportionRatio,
                                                          .texture_paths = texture_paths,
                                                          .cubes = {ro::formats::GndCube{}}});
  if (!ground) return std::unexpected(ground.error());
  ro::render::MapScene library = ro::render::AssembleMapScene(
      vfs, look.reference_map, std::move(*ground),
      ro::formats::RswScene::Assemble(SyntheticHeader(), light, std::move(every_model)),
      ro::render::MapSceneOptions{.edit = ro::mapedit::MapEdit(), .progress = progress});
  return GroundSources{
      .rule = &rule,
      .run_seed = run_seed,
      .look = &look,
      .light = light,
      .plan_light = GroundPlanLight{.toward_sun = ro::render::MakeSceneLight(light).direction},
      .texture_paths = std::move(texture_paths),
      .model_files = std::move(model_files),
      .library = std::move(library)};
}

Result<MapStage> AssembleGroundWindow(ro::vfs::Vfs& vfs, const GroundSources& sources,
                                      const GroundPlan& plan, std::string name,
                                      std::int32_t ground_subdivisions) {
  auto gnd = ro::formats::GndMap::Assemble(GroundOf(sources, plan));
  if (!gnd) return std::unexpected(gnd.error());
  auto gat = ro::formats::GatMap::Assemble(kGroundWindowCells, kGroundWindowCells, CellsOf(plan));
  if (!gat) return std::unexpected(gat.error());
  std::vector<ro::formats::RswModel> models = ModelsOf(sources, plan, *gnd);
  ro::render::MapScene scene = ro::render::AssembleMapScene(
      vfs, sources.look->reference_map, std::move(*gnd),
      ro::formats::RswScene::Assemble(SyntheticHeader(), sources.light, std::move(models)),
      ro::render::MapSceneOptions{.ground_subdivisions = ground_subdivisions,
                                  .edit = ro::mapedit::MapEdit(),
                                  .resources = &sources.library});
  return AssembleMapStage(std::move(name), std::move(scene), std::move(*gat));
}

Result<MapStage> BuildGroundWindow(ro::vfs::Vfs& vfs, const GroundSources& sources,
                                   GroundWindowOrigin origin, std::string name,
                                   std::int32_t ground_subdivisions) {
  const GroundPlan plan =
      PlanGroundWindow(*sources.rule, sources.run_seed, *sources.look, origin, sources.plan_light);
  return AssembleGroundWindow(vfs, sources, plan, std::move(name), ground_subdivisions);
}

}  // namespace moonclient

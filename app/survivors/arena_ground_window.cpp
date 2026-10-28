#include "survivors/arena_ground_window.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/log.h"
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
constexpr std::string_view kComposedPagePrefix = "composed_ground_";
constexpr std::string_view kRecomposedPatchPrefix = "composed_patch_";
constexpr std::uint8_t kPlaceholderGrey = 128;
constexpr std::uint8_t kPlaceholderOpaque = 255;

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

// Rapproche chaque coin de la tuile du centre de son quart de texture de
// kGroundTileInset.
void InsetTile(ro::formats::GndTile& tile) {
  const auto inward = [](std::array<float, 4>& coordinates) {
    const float low = *std::ranges::min_element(coordinates);
    const float high = *std::ranges::max_element(coordinates);
    for (float& coordinate : coordinates) {
      coordinate = coordinate == low ? low + kGroundTileInset : high - kGroundTileInset;
    }
  };
  inward(tile.u);
  inward(tile.v);
}

// LE SOL DU PLAN, en pièces de GND : une tuile par cube, sa lightmap, le quart
// de sa texture — ou sa tuile composée, quand il en a une.
ro::formats::GndParts GroundOf(const GroundSources& sources, const GroundPlan& plan,
                               const ComposedGround& composed) {
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
  for (std::size_t rank = 0; rank < plan.cubes.size(); ++rank) {
    const PlannedCube& cube = plan.cubes[rank];
    ro::formats::GndTile tile;
    auto texture = static_cast<std::int16_t>(cube.texture);
    const std::optional<ComposedTileRef>& own =
        rank < composed.tiles.size() ? composed.tiles[rank] : std::nullopt;
    if (own.has_value()) {
      texture = static_cast<std::int16_t>(sources.first_composed_page + own->page);
      // Les coins dans l'ordre du fichier, la tuile debout : l'est vers les u
      // croissants, le nord vers les v croissants.
      tile.u = {own->south_west[0], own->north_east[0], own->south_west[0], own->north_east[0]};
      tile.v = {own->south_west[1], own->south_west[1], own->north_east[1], own->north_east[1]};
    } else {
      // Une tache recomposée lit la version de son quart de tour, au même
      // point de texture.
      if (cube.texture < sources.recomposed_patches.size() &&
          sources.recomposed_patches[cube.texture].has_value()) {
        texture = static_cast<std::int16_t>(*sources.recomposed_patches[cube.texture] +
                                            cube.turn % kQuarterTurns);
      }
      // Les coins dans l'ordre du fichier : sud-ouest, sud-est, nord-ouest,
      // nord-est, chacun à sa place dans le motif tourné — dans son bord, s'il
      // est bordé.
      const auto rank_of_texture = static_cast<std::size_t>(texture);
      const float border = rank_of_texture < sources.motif_borders.size()
                               ? sources.motif_borders[rank_of_texture]
                               : 0.0F;
      for (std::size_t corner = 0; corner < tile.u.size(); ++corner) {
        const auto [u, v] =
            MotifPoint(cube.quarter_east + static_cast<std::int32_t>(corner % 2),
                       cube.quarter_north + static_cast<std::int32_t>(corner / 2), cube.turn);
        tile.u[corner] = border + u * (1.0F - 2.0F * border);
        tile.v[corner] = border + v * (1.0F - 2.0F * border);
      }
      if (cube.inset) InsetTile(tile);
    }
    parts.tiles.push_back(
        ro::formats::GndTile{.u = tile.u,
                             .v = tile.v,
                             .texture_index = texture,
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

// LE NOM D'UNE PAGE DE LA TERRE COMPOSÉE : aucun fichier ne le porte.
std::string ComposedPageName(std::size_t page) {
  return std::string(kComposedPagePrefix) + std::to_string(page);
}

// LE NOM DE LA TACHE de rang `rank` recomposée pour le quart de tour `turn`.
std::string RecomposedPatchName(std::size_t rank, std::uint8_t turn) {
  return std::string(kRecomposedPatchPrefix) + std::to_string(rank) + "_" + std::to_string(turn);
}

// LA PLACE D'UNE PAGE OU D'UNE TACHE, tant que rien n'y est posé : un texel
// opaque, qu'aucun carreau ne lit.
ro::formats::Image PagePlaceholder() {
  return ro::formats::Image{
      .width = 1,
      .height = 1,
      .pixels = {kPlaceholderGrey, kPlaceholderGrey, kPlaceholderGrey, kPlaceholderOpaque},
      .file = ro::formats::ImageFile::kBmp};
}

// Vrai si aucun autre rang de `look` ne cite le fichier du rang `rank`.
bool OnlyRoleOfFile(const BiomeLook& look, std::size_t rank) {
  return std::ranges::count_if(look.textures, [&look, rank](const GroundTextureLook& other) {
           return other.texture == look.textures[rank].texture;
         }) == 1;
}

// Le rang de la première texture du rôle `role`, ou rien.
std::optional<std::size_t> RankOfRole(const BiomeLook& look, GroundTextureRole role) {
  for (std::size_t rank = 0; rank < look.textures.size(); ++rank) {
    if (look.textures[rank].role == role) return rank;
  }
  return std::nullopt;
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
  for (const SeedLook& seed : look.verge_seeds) cited.push_back(seed.model);
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
  for (std::size_t page = 0; page < kComposedAtlasPages; ++page) {
    texture_paths.push_back(ComposedPageName(page));
  }
  std::vector<std::optional<std::size_t>> recomposed_patches(look.textures.size());
  for (std::size_t rank = 0; rank < look.textures.size(); ++rank) {
    const GroundTextureRole role = look.textures[rank].role;
    if (role != GroundTextureRole::kPatch && role != GroundTextureRole::kScatter) continue;
    recomposed_patches[rank] = texture_paths.size();
    for (std::uint8_t turn = 0; turn < kQuarterTurns; ++turn) {
      texture_paths.push_back(RecomposedPatchName(rank, turn));
    }
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
  // LES PAGES DE LA TERRE COMPOSÉE ET LES TACHES RECOMPOSÉES n'ont pas de
  // fichier : leur place d'un texel ne compte pas pour une texture absente.
  const std::size_t first_composed_page = look.textures.size();
  for (std::size_t rank = first_composed_page; rank < library.terrain_textures.size(); ++rank) {
    ro::formats::Image& kept = library.terrain_textures[rank];
    if (kept.empty() && library.missing_terrain_textures > 0) --library.missing_terrain_textures;
    kept = PagePlaceholder();
  }
  std::optional<GroundComposeSources> compose;
  const std::optional<std::size_t> grass = RankOfRole(look, GroundTextureRole::kGrass);
  const std::optional<std::size_t> dirt = RankOfRole(look, GroundTextureRole::kPathFull);
  if (grass.has_value() && dirt.has_value()) {
    compose =
        MakeGroundComposeSources(library.terrain_textures[*grass], library.terrain_textures[*dirt]);
  }
  // Les taches se recomposent une fois pour toutes : les fenêtres les
  // reprennent de la bibliothèque.
  for (std::size_t rank = 0; rank < recomposed_patches.size(); ++rank) {
    if (!recomposed_patches[rank].has_value()) continue;
    for (std::uint8_t turn = 0; turn < kQuarterTurns; ++turn) {
      std::optional<ro::formats::Image> recomposed =
          compose.has_value() ? ComposePatchTexture(*compose, library.terrain_textures[rank], turn)
                              : std::nullopt;
      if (!recomposed.has_value()) {
        recomposed_patches[rank].reset();
        break;
      }
      library.terrain_textures[*recomposed_patches[rank] + turn] = std::move(*recomposed);
    }
  }
  // LES MOTIFS QUI SE RÉPÈTENT D'UN EMPLACEMENT À L'AUTRE se lisent bordés de
  // la période suivante : l'herbe de fond, la terre pleine, chaque tache
  // recomposée. Une texture que deux rôles partagent garde sa lecture.
  std::vector<float> motif_borders(texture_paths.size(), 0.0F);
  const auto border = [&](std::size_t rank, const ro::formats::Image& beyond, std::uint8_t turn) {
    ro::formats::Image& image = library.terrain_textures[rank];
    ro::formats::Image bordered = BorderMotif(image, beyond, turn);
    if (bordered.width == image.width) return;
    motif_borders[rank] =
        static_cast<float>(kMotifBorderTexels) / static_cast<float>(bordered.width);
    image = std::move(bordered);
  };
  if (compose.has_value()) {
    if (OnlyRoleOfFile(look, *grass)) border(*grass, compose->grass, 0);
    if (OnlyRoleOfFile(look, *dirt)) border(*dirt, compose->dirt, 0);
    for (const std::optional<std::size_t>& first : recomposed_patches) {
      if (!first.has_value()) continue;
      for (std::uint8_t turn = 0; turn < kQuarterTurns; ++turn) {
        border(*first + turn, compose->grass, turn);
      }
    }
  }
  // UN MODÈLE ABSENT DE L'INSTALLATION ne se pose pas : une fenêtre ne cite que
  // ce que la bibliothèque tient, et ne lit donc aucun fichier.
  std::erase_if(model_files, [&library](const auto& cited) {
    return std::ranges::none_of(library.models, [&cited](const ro::render::ModelInstances& kept) {
      return kept.rsm_file == cited.second;
    });
  });
  return GroundSources{
      .rule = &rule,
      .run_seed = run_seed,
      .look = &look,
      .light = light,
      .plan_light = GroundPlanLight{.toward_sun = ro::render::MakeSceneLight(light).direction},
      .texture_paths = std::move(texture_paths),
      .model_files = std::move(model_files),
      .library = std::move(library),
      .compose = std::move(compose),
      .first_composed_page = first_composed_page,
      .recomposed_patches = std::move(recomposed_patches),
      .motif_borders = std::move(motif_borders)};
}

Result<MapStage> AssembleGroundWindow(ro::vfs::Vfs& vfs, const GroundSources& sources,
                                      const GroundPlan& plan, std::string name,
                                      std::int32_t ground_subdivisions) {
  ComposedGround composed;
  if (sources.compose.has_value()) {
    composed = ComposeGround(*sources.compose, sources.run_seed, plan);
    if (composed.overflow > 0) {
      ro::core::LogWarning(
          "sol généré : {} cubes de lisière gardent leurs carreaux, faute de place",
          composed.overflow);
    }
  }
  auto gnd = ro::formats::GndMap::Assemble(GroundOf(sources, plan, composed));
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
  for (std::size_t page = 0; page < composed.pages.size(); ++page) {
    const std::size_t rank = sources.first_composed_page + page;
    if (composed.pages[page].empty() || rank >= scene.terrain_textures.size()) continue;
    scene.terrain_textures[rank] = std::move(composed.pages[page]);
  }
  // LA PART PURE DU TÉLÉVERSEMENT avec le reste : le fil qui monte la fenêtre
  // n'a plus qu'à copier.
  ro::render::PrepareSceneUpload(scene);
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

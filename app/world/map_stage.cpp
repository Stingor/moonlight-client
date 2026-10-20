#include "world/map_stage.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

#include "ro/core/build_stamp.h"
#include "ro/core/log.h"
#include "ro/formats/gat.h"
#include "ro/mapedit/ground_edit.h"
#include "ro/render/map_fog.h"
#include "ro/render/map_sky.h"
#include "ro/render/terrain_mesh.h"
#include "ro/sim/world.h"
#include "ro/vfs/encoding.h"

namespace moonclient {
namespace {

// Une carte est trois fichiers ; celui de la MARCHABILITÉ porte l'extension
// .gat, sous data\\.
//
// ⚠ LA GRILLE SE TRADUIT POUR ELLE-MÊME : une carte clonée n'a pas de fichier à
// son nom, et neuf d'entre elles ne prennent pas leurs trois fichiers au même
// endroit. Voir `ro::vfs::ResNameTable`.
std::string GatFilePath(const ro::vfs::ResNameTable& res_names, std::string_view map_name) {
  return res_names.MapFilePath(std::string(map_name) + ".gat");
}

// La lecture qu'une arrivée demande.
[[nodiscard]] MapStageRead ReadOf(const MapArrival& arrival) {
  return MapStageRead{.flatten_relief = arrival.flatten_relief,
                      .edits = arrival.map_edits,
                      .real_time_shadows = arrival.real_time_shadows,
                      .rounded_ground = arrival.rounded_ground,
                      .progress = arrival.progress};
}

}  // namespace

std::string WindowTitle(bool test_server) {
  return std::string(test_server ? kTestServerTitleMark : std::string_view()) +
         std::string(kWindowTitle) + " " + std::string(ro::core::kBuildVersion);
}

std::string WindowTitle(bool names_shown, const ro::sim::World& world, bool test_server) {
  if (!names_shown) {
    return WindowTitle(test_server);
  }
  constexpr std::string_view kSeparator = " - ";
  std::string title(test_server ? kTestServerTitleMark : std::string_view());
  const std::array<std::string_view, 2> parts = {world.player_name(), world.map_name()};
  for (const std::string_view part : parts) {
    if (!part.empty()) {
      title.append(part).append(kSeparator);
    }
  }
  // La marque est déjà en tête : elle ne se répète pas avant le nom.
  return title + WindowTitle(/*test_server=*/false);
}

ro::core::Result<MapStage> LoadMapStage(ro::vfs::Vfs& vfs, std::string_view map_name,
                                        const MapStageRead& read) {
  const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
  ro::render::MapSceneOptions options{
      .flatten_relief = read.flatten_relief,
      .ground_subdivisions = read.rounded_ground ? ro::render::kRoundedGroundSubdivisions
                                                 : ro::render::kFlatGroundSubdivisions,
      .progress = read.progress};
  // Les documents d'édition se lisent avec les fichiers de la carte.
  ro::core::BeginLoadStep(read.progress, ro::core::LoadStep::kMap);
  if (read.edits != nullptr) {
    read.edits->Load(map_name, vfs);
    // Les lumières de modèle valent pour toutes les cartes : lues une fois.
    read.edits->Load(ro::mapedit::kModelLightsDocument, vfs);
    options.edit = read.edits->Document(map_name);
  } else {
    options.edit = ro::render::LoadMapEdit(vfs, map_name);
  }
  // LE RELIEF RETOUCHÉ, seulement sous les ombres en temps réel : la scène
  // reçoit un document sans lui, le magasin garde le sien entier.
  if (!read.real_time_shadows) {
    options.edit->ClearGround();
  }
  auto scene = ro::render::LoadMapScene(vfs, map_name, options);
  if (!scene) {
    return std::unexpected(scene.error());
  }
  if (scene->terrain.empty()) {
    return ro::core::Fail(ro::core::ErrorCode::kCorruptData,
                          std::string(map_name) + " : la carte ne produit aucune géométrie");
  }

  // La scène ne se construit pas vide — sa carte de hauteurs n'a pas de valeur
  // par défaut —, d'où l'agrégation d'un seul tenant.
  MapStage stage{std::string(map_name), std::move(*scene), ro::sim::WalkMap()};
  stage.flatten_asked = read.flatten_relief;
  const ro::core::LoadStepScope walk_grid(read.progress, ro::core::LoadStep::kWalkGrid);
  if (auto gat_bytes = vfs.Read(GatFilePath(vfs.res_names(), map_name)); gat_bytes) {
    if (auto gat = ro::formats::GatMap::Parse(*gat_bytes); gat) {
      stage.walk_map = ro::sim::WalkMap::FromGat(*gat);
      // Au niveau du sol, et seulement s'il a été aplati : un terrain sans sol
      // ne l'est pas, et la grille garde alors le sien. ⚠ AVANT LA LUMIÈRE, qui
      // compare les deux reliefs pour reconnaître un pont.
      if (stage.scene.relief_level.has_value()) {
        gat->LevelCornerHeights(*stage.scene.relief_level);
      }
      // Les hauteurs du fichier se gardent pour l'éditeur, puis le relief
      // retouché que porte le sol se pose aussi sur elles.
      stage.file_walk_heights.reserve(gat->cells().size());
      for (const ro::formats::GatCell& cell : gat->cells()) {
        stage.file_walk_heights.push_back(cell.corner_height);
      }
      ro::mapedit::LiftWalkHeights(stage.scene.ground_edits, stage.scene.gnd.width(),
                                   stage.scene.gnd.height(), *gat);
      stage.ground_light = ro::render::BuildGroundLight(stage.scene.gnd, *gat);
      stage.ground_laid_light = ro::render::BuildGroundLight(
          stage.scene.gnd, *gat, ro::render::GroundLightTerms::kLaidLightOnly);
      stage.gat = std::move(*gat);
    } else {
      // ⚠ LES DEUX ÉCHECS NE PORTENT PAS LE MÊME LIBELLÉ : la fenêtre de
      // journal ne garde que le TEXTE, et « illisible » n'envoie pas chercher
      // au même endroit qu'« absente ».
      ro::core::LogWarning(
          "grille de marche de {} : illisible ({}) — on marchera sans grille, sur la seule "
          "hauteur du terrain",
          map_name, ro::core::Describe(gat.error()));
    }
  } else {
    ro::core::LogWarning(
        "grille de marche de {} : absente ({}) — on marchera sans grille, sur la seule hauteur "
        "du terrain",
        map_name, ro::core::Describe(gat_bytes.error()));
  }
  stage.read_ms = static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                 std::chrono::steady_clock::now() - started)
                                                 .count());
  return stage;
}

std::array<float, 3> GroundPositionAt(const MapStage& stage, float cell_x, float cell_y) {
  std::array<float, 3> ground = ro::render::CellPositionAt(stage.scene.gnd, cell_x, cell_y);
  const bool rounded = stage.scene.ground_subdivisions > ro::render::kFlatGroundSubdivisions;
  if (stage.gat.has_value()) {
    const float walkable = ro::render::WalkableHeightAt(*stage.gat, cell_x, cell_y);
    // LE SOL DESSINÉ, quand il s'arrondit : sans quoi les pieds flottent au
    // creux d'une dune et s'enfoncent sur sa crête.
    ground[1] = rounded ? ro::render::RoundedStandingHeight(stage.scene.gnd, walkable, ground[0],
                                                            ground[2], stage.scene.invisible_tiles)
                        : walkable;
  } else if (rounded) {
    ground[1] = ro::render::RoundedGroundHeightAt(stage.scene.gnd, ground[0], ground[2],
                                                  stage.scene.invisible_tiles);
  }
  return ground;
}

ro::core::Result<void> SyncStageGround(MapStage& stage, ro::render::TerrainRenderer& terrain,
                                       bool rounded) {
  const std::int32_t wanted =
      rounded ? ro::render::kRoundedGroundSubdivisions : ro::render::kFlatGroundSubdivisions;
  if (stage.scene.ground_subdivisions == wanted) {
    return {};
  }
  const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
  ro::render::RebuildSceneGround(stage.scene, wanted);
  if (auto uploaded = terrain.Upload(stage.scene.terrain, stage.scene.terrain_textures,
                                     stage.scene.atlas.image);
      !uploaded) {
    return std::unexpected(uploaded.error());
  }
  ro::core::LogInfo("carte {} : sol {}, {} sommets, rebâti en {} ms", stage.name,
                    rounded ? "arrondi" : "d'origine", stage.scene.terrain.vertices.size(),
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - started)
                        .count());
  return {};
}

bool ReshapeStageGround(MapStage& stage, std::span<const ro::mapedit::GroundEdit> edits) {
  // UN RELIEF APLATI n'en porte aucune : voir `ro::render::ReshapeSceneGround`.
  if (stage.scene.relief_level.has_value() || std::ranges::equal(stage.scene.ground_edits, edits)) {
    return false;
  }
  const ro::mapedit::MapEditReport report = ro::render::ReshapeSceneGround(stage.scene, edits);
  for (const std::string& refusal : report.refused) {
    ro::core::LogWarning("édition de {} refusée — {}", stage.name, refusal);
  }
  if (stage.gat.has_value()) {
    ro::formats::GatMap& gat = *stage.gat;
    std::size_t rank = 0;
    for (std::int32_t y = 0; y < gat.height(); ++y) {
      for (std::int32_t x = 0; x < gat.width(); ++x) {
        if (rank < stage.file_walk_heights.size()) {
          gat.SetCornerHeights(x, y, stage.file_walk_heights[rank]);
        }
        ++rank;
      }
    }
    ro::mapedit::LiftWalkHeights(stage.scene.ground_edits, stage.scene.gnd.width(),
                                 stage.scene.gnd.height(), gat);
  }
  stage.ground_mesh_stale = true;
  return true;
}

std::span<const ro::mapedit::GroundEdit> ShownGroundEdits(const ro::mapedit::MapEdit& document,
                                                          bool real_time_shadows) {
  if (!real_time_shadows) {
    return {};
  }
  return document.ground();
}

bool RemeshesWhileStroking(const MapStage& stage) {
  return !stage.ground_mesh_ms.has_value() || *stage.ground_mesh_ms <= kLiveGroundRemeshMs;
}

ro::core::Result<void> RemeshStageGround(MapStage& stage, ro::render::TerrainRenderer& terrain) {
  if (!stage.ground_mesh_stale) {
    return {};
  }
  const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
  stage.ground_mesh_stale = false;
  ro::render::RebuildSceneTerrain(stage.scene);
  if (stage.gat.has_value()) {
    stage.ground_light = ro::render::BuildGroundLight(stage.scene.gnd, *stage.gat);
    stage.ground_laid_light = ro::render::BuildGroundLight(
        stage.scene.gnd, *stage.gat, ro::render::GroundLightTerms::kLaidLightOnly);
  }
  auto uploaded =
      terrain.Upload(stage.scene.terrain, stage.scene.terrain_textures, stage.scene.atlas.image);
  stage.ground_mesh_ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
  if (!uploaded) {
    return std::unexpected(uploaded.error());
  }
  return {};
}

void ReportMapStage(const MapStage& stage) {
  // ⚠ LE SUJET EN TÊTE, comme partout ailleurs : cette ligne s'ouvrait sur le
  // nom de carte nu, si bien que les deux lignes d'un même chargement ne se
  // cherchaient pas de la même façon.
  ro::core::LogInfo("carte {} : {} sommets, {} modèle(s), {} instance(s), lue en {} ms", stage.name,
                    stage.scene.terrain.vertices.size(), stage.scene.models.size(),
                    stage.scene.model_instance_count, stage.read_ms);
  ro::core::LogDebug("carte {} : textures de modèle, {} citée(s) pour {} lue(s)", stage.name,
                     stage.scene.model_textures_cited, stage.scene.model_textures_read);
  // 🔴 CE QUI MANQUE SE DIT ICI, et non plus seulement dans `roc-cli`. Une
  // texture introuvable ou indécodable (PNG, BMP en bitfields ou RLE) devient
  // une plaque magenta à l'écran, que rien ne distinguait d'une clé de
  // transparence mal appliquée. Les deux se corrigent à des endroits opposés.
  const ro::render::MapScene& scene = stage.scene;
  if (scene.missing_terrain_textures + scene.missing_water_textures +
          scene.missing_effect_textures + scene.missing_model_textures.size() +
          scene.unreadable_models >
      0) {
    ro::core::LogWarning(
        "carte {} : textures manquantes — {} de terrain, {} d'eau, {} d'effet, {} de modèle ; "
        "{} modèle(s) illisible(s)",
        stage.name, scene.missing_terrain_textures, scene.missing_water_textures,
        scene.missing_effect_textures, scene.missing_model_textures.size(),
        scene.unreadable_models);
  }
  // ⚠ LES NOMS SONT EN CP949, tirés du fichier de modèle : transcodés pour le
  // journal, qui est en UTF-8, sans quoi un nom coréen s'y lirait en charabia.
  for (const std::string& texture : scene.missing_model_textures) {
    ro::core::LogWarning("carte {} : texture de modèle manquante « {} »", stage.name,
                         ro::vfs::Cp949ToUtf8Lossy(texture));
  }
}

const ro::sim::WalkMap* PlannedMapOf(const MapStage& stage) {
  return stage.walk_map.width() > 0 ? &stage.walk_map : nullptr;
}

std::array<float, 4> BackgroundOf(const MapStage& stage) {
  return ro::render::MapClearColorOf(stage.scene.background);
}

ro::render::SceneFog FogOf(const MapStage& stage, bool fog_shown) {
  if (!fog_shown || !stage.scene.fog.has_value()) {
    return {};
  }
  return ro::render::SceneFogOf(*stage.scene.fog);
}

std::array<float, 4> ClearColorOf(const MapStage& stage, const std::array<float, 4>& background,
                                  bool fog_shown) {
  if (!fog_shown || !stage.scene.fog.has_value() || !stage.scene.background_fog) {
    return background;
  }
  return ro::render::FoggedClearColor(background, *stage.scene.fog);
}

ro::render::Camera WholeMapCamera(const MapStage& stage, float aspect_ratio) {
  // Champ de vision de la carte entière. Le jeu emploie 15°, ce qui donne à RO
  // son aspect presque orthographique ; 45° montre davantage de la carte.
  constexpr float kEditorFovDegrees = 45.0F;
  return ro::render::FrameBounds(stage.scene.terrain.bounds_min, stage.scene.terrain.bounds_max,
                                 aspect_ratio, kEditorFovDegrees);
}

ro::core::Result<void> UploadStage(const MapStage& stage, ro::render::TerrainRenderer& terrain,
                                   ro::render::WaterRenderer& water,
                                   ro::render::ModelRenderer& models,
                                   ro::core::LoadProgress* progress) {
  ro::core::BeginLoadStep(progress, ro::core::LoadStep::kUpload);
  if (auto uploaded = terrain.Upload(stage.scene.terrain, stage.scene.terrain_textures,
                                     stage.scene.atlas.image);
      !uploaded) {
    return std::unexpected(uploaded.error());
  }
  // 🔴 L'EAU AVEC LE RESTE, et il n'y a pas de raccourci « la carte n'en a
  // pas » : c'est justement le cas qui mord. Sans ce téléversement, une carte
  // sèche garde la nappe de la précédente — sur gonryun, la mer d'à côté
  // flottait entre les plateaux.
  if (auto uploaded = water.Upload(stage.scene.water, stage.scene.water_animations); !uploaded) {
    return std::unexpected(uploaded.error());
  }
  ro::core::EndLoadStep(progress);
  return models.Upload(stage.scene.models, progress);
}

ro::core::Result<void> EnterAnnouncedMap(const MapArrival& arrival) {
  if (arrival.world->map_name() == *arrival.attempted) {
    return {};
  }
  *arrival.attempted = arrival.world->map_name();

  auto arrived = LoadMapStage(*arrival.vfs, *arrival.attempted, ReadOf(arrival));
  if (!arrived) {
    // ⚠ CE QUI RESTE À L'ÉCRAN : on rend un succès juste après, sans toucher à la
    // scène. Le client garde donc la géométrie de la carte PRÉCÉDENTE pendant
    // que le monde se croit ailleurs.
    ro::core::LogError("carte {} : {} — la carte précédente reste affichée", *arrival.attempted,
                       ro::core::Describe(arrived.error()));
    return {};
  }

  *arrival.stage = std::move(*arrived);
  ReportMapStage(*arrival.stage);
  *arrival.planner = ro::sim::WalkPlanner(PlannedMapOf(*arrival.stage));
  arrival.world->SetWalkMap(PlannedMapOf(*arrival.stage));
  *arrival.background = BackgroundOf(*arrival.stage);
  return UploadStage(*arrival.stage, *arrival.terrain, *arrival.water, *arrival.models,
                     arrival.progress);
}

ro::core::Result<bool> ReloadStageRelief(const MapArrival& arrival) {
  // ⚠ LA DEMANDE EST NOTÉE AVANT LA LECTURE, succès ou non : un échec ne se
  // retente pas à chaque image.
  arrival.stage->flatten_asked = arrival.flatten_relief;
  auto reread = LoadMapStage(*arrival.vfs, arrival.stage->name, ReadOf(arrival));
  if (!reread) {
    ro::core::LogError("carte {} : {} — le relief reste celui d'avant", arrival.stage->name,
                       ro::core::Describe(reread.error()));
    return false;
  }
  // LA FUMÉE DES CHEMINÉES se charge à la demande, quand un acteur en porte une
  // sur une carte qui n'en a pas : la relecture ne la retrouverait pas.
  if (!reread->scene.chimney_smoke.has_value()) {
    reread->scene.chimney_smoke = std::move(arrival.stage->scene.chimney_smoke);
  }
  // ⚠ LA MARCHABILITÉ RESTE EN PLACE : le planificateur et le monde la désignent
  // par adresse, et elle ne dépend pas du relief.
  arrival.stage->scene = std::move(reread->scene);
  arrival.stage->gat = std::move(reread->gat);
  arrival.stage->ground_light = std::move(reread->ground_light);
  arrival.stage->ground_laid_light = std::move(reread->ground_laid_light);
  arrival.stage->read_ms = reread->read_ms;
  ReportMapStage(*arrival.stage);
  if (auto uploaded = UploadStage(*arrival.stage, *arrival.terrain, *arrival.water, *arrival.models,
                                  arrival.progress);
      !uploaded) {
    return std::unexpected(uploaded.error());
  }
  return true;
}

}  // namespace moonclient

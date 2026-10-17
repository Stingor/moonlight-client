#include "ro/render/map_scene.h"

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/formats/grf.h"
#include "ro/formats/rsm.h"
#include "ro/mapedit/ground_edit.h"
#include "ro/render/grey_world.h"
#include "ro/render/map_effects.h"
#include "ro/render/map_sky.h"
#include "ro/render/map_weather.h"
#include "ro/vfs/encoding.h"

namespace ro::render {

// Charge une texture citée par un fichier de carte ou de modèle.
//
// Terrain et décor décodent tous deux d'après l'extension du nom : les données
// mêlent 61 460 BMP et 10 431 TGA, et ne gérer qu'un des deux formats laisse
// des plaques magenta à l'écran. Le choix vit ici, à un seul endroit.
//
// ReadRaw : ces noms sont des octets CP949 tirés du fichier lui-même. Les
// passer à Read les transcoderait une seconde fois et rendrait chaque texture à
// nom coréen introuvable — c'est-à-dire presque toutes.
std::optional<formats::Image> LoadSceneTexture(vfs::Vfs& vfs, const std::string& relative,
                                               SceneTextureKey key) {
  auto bytes = vfs.ReadRaw("data\\texture\\" + relative);
  if (!bytes) {
    return std::nullopt;
  }
  auto image = formats::DecodeImage(relative, *bytes);
  if (!image) {
    return std::nullopt;
  }
  if (key == SceneTextureKey::kGroundAndDecor && image->file == formats::ImageFile::kBmp) {
    formats::ApplyTextureColorKey(*image);
  } else {
    formats::ApplyMagentaColorKey(*image);
  }
  return std::move(*image);
}

namespace {

// Le nom en UTF-8 d'un fichier que le RSW cite en CP949 : ce que l'édition
// compare à celui qu'elle désigne.
std::string CitedFileName(std::string_view raw) { return vfs::Cp949ToUtf8Lossy(raw); }

// Le nom qu'un RSW citerait pour un fichier que l'édition nomme en UTF-8.
std::string RawFileName(std::string_view utf8) {
  auto raw = vfs::Utf8ToCp949(utf8);
  return raw ? std::move(*raw) : std::string(utf8);
}

constexpr mapedit::FileNameCodec kFileNames{.to_utf8 = &CitedFileName, .to_raw = &RawFileName};

// Décode les textures du terrain, dans l'ordre du GND.
//
// Une texture absente laisse une image VIDE à sa place plutôt que d'interrompre
// le chargement : le renderer la remplace par du magenta, et le défaut se voit
// à l'écran au lieu de faire disparaître la carte.
std::vector<formats::Image> LoadTerrainTextures(vfs::Vfs& vfs, const formats::GndMap& gnd,
                                                std::size_t& missing) {
  std::vector<formats::Image> textures;
  textures.reserve(gnd.texture_paths().size());

  for (const std::string& relative : gnd.texture_paths()) {
    std::optional<formats::Image> image = LoadSceneTexture(vfs, relative);
    if (!image) {
      textures.emplace_back();
      ++missing;
      continue;
    }
    textures.push_back(std::move(*image));
  }
  return textures;
}

// Charge les images d'eau, une seule fois par TYPE de nappe employé.
//
// ⚠ ELLES NE PASSENT PAS PAR `LoadSceneTexture` : ce sont des JPEG, et la clé
// magenta ne s'applique JAMAIS à un JPEG — le codage avec perte y salit les
// pixels roses et en invente ailleurs, si bien que la clé percerait la nappe au
// hasard. Voir `jpeg.h`.
//
// Et le chemin est écrit en UTF-8 dans notre source, donc `Read` et non
// `ReadRaw` : c'est nous qui le composons, pas le fichier de carte.
std::vector<WaterAnimation> LoadWaterAnimations(vfs::Vfs& vfs, const WaterPlane& plane,
                                                std::size_t& missing) {
  std::vector<WaterAnimation> animations;
  for (const WaterZoneMesh& zone : plane.zones) {
    const std::int32_t type = zone.settings.type;
    if (std::ranges::any_of(animations,
                            [type](const WaterAnimation& one) { return one.type == type; })) {
      continue;
    }

    WaterAnimation animation{.type = type, .frames = {}};
    animation.frames.reserve(kWaterFrameCount);
    for (int frame = 0; frame < kWaterFrameCount; ++frame) {
      auto bytes = vfs.Read(WaterTexturePath(type, frame));
      if (!bytes) {
        ++missing;
        continue;
      }
      auto image = formats::DecodeImage(".jpg", *bytes);
      if (!image) {
        ++missing;
        continue;
      }
      animation.frames.push_back(std::move(*image));
    }
    animations.push_back(std::move(animation));
  }
  return animations;
}

// Charge les modèles du décor, une seule fois par fichier .rsm distinct.
//
// Une carte pose le même arbre des centaines de fois : prontera compte 1303
// instances pour 62 fichiers. Charger par instance multiplierait le travail par
// vingt pour un résultat identique.
//
// ⚠ SEULEMENT CEUX QUI MANQUENT À LA SCÈNE : un modèle que l'éditeur ajoute se
// lit à son ajout, sans relire ceux qui sont déjà là.
// LA CLÉ D'UN JEU DE POSES : son fichier, et la pose dont il est la variante.
//
// ⚠ UNE VUE, ET NON UNE COPIE : elle désigne le nom d'un modèle du RSW ou d'un
// jeu de poses de la scène, et ne vit que le temps d'un repos du décor, où ni
// l'un ni l'autre ne bouge. Reposer le décor demande une clé par pose, à chaque
// image d'un glissé.
using InstancesKey = std::pair<std::string_view, std::optional<std::size_t>>;

// La retouche de maillage de la pose `rank`, réservée à elle seule, ou rien.
const mapedit::MeshEdit* VariantEditOf(const MapScene& scene, std::size_t rank) {
  for (const mapedit::MeshEdit& edit : scene.mesh_edits) {
    if (!edit.all_poses && edit.model_index == rank) {
      return &edit;
    }
  }
  return nullptr;
}

// La retouche de maillage qui vaut pour toutes les poses du fichier `raw_file`.
const mapedit::MeshEdit* FileEditOf(const MapScene& scene, std::string_view raw_file) {
  for (const mapedit::MeshEdit& edit : scene.mesh_edits) {
    if (edit.all_poses && RawFileName(edit.model_file) == raw_file) {
      return &edit;
    }
  }
  return nullptr;
}

// Le jeu de poses où se dessine la pose `rank`.
InstancesKey KeyOf(const MapScene& scene, std::size_t rank) {
  const formats::RswModel& placement = scene.rsw.models()[rank];
  return {placement.model_name,
          VariantEditOf(scene, rank) != nullptr ? std::optional<std::size_t>(rank) : std::nullopt};
}

// Les sommets que l'édition déplace dans le jeu de poses `key`.
const std::vector<mapedit::VertexMove>& MovesOf(const MapScene& scene, const InstancesKey& key) {
  static const std::vector<mapedit::VertexMove> kNoMoves;
  const mapedit::MeshEdit* edit =
      key.second.has_value() ? VariantEditOf(scene, *key.second) : FileEditOf(scene, key.first);
  return edit != nullptr ? edit->vertices : kNoMoves;
}

// Charge les modèles du décor, une seule fois par fichier .rsm distinct — et
// une fois de plus par variante.
//
// Une carte pose le même arbre des centaines de fois : prontera compte 1303
// instances pour 62 fichiers. Charger par instance multiplierait le travail par
// vingt pour un résultat identique.
//
// ⚠ SEULEMENT CE QUI MANQUE À LA SCÈNE : un modèle que l'éditeur ajoute se lit
// à son ajout, et une variante dont les sommets changent se rebâtit seule. Un
// jeu de poses que l'édition ne demande plus, ou dont les sommets ont changé,
// s'en va d'abord.
void LoadMissingSceneModels(vfs::Vfs& vfs, MapScene& scene) {
  std::set<InstancesKey> wanted;
  for (std::size_t rank = 0; rank < scene.rsw.models().size(); ++rank) {
    wanted.insert(KeyOf(scene, rank));
  }
  // Les jeux qui s'en vont gardent leurs textures jusqu'à la fin : une
  // variante qui se rebâtit reprend celles de la précédente.
  const auto stays = [&](const ModelInstances& instances) {
    const InstancesKey key{instances.rsm_file, instances.variant_rank};
    return wanted.contains(key) && instances.applied_moves == MovesOf(scene, key);
  };
  const auto first_leaving = std::stable_partition(scene.models.begin(), scene.models.end(), stays);
  std::vector<ModelInstances> leaving(std::make_move_iterator(first_leaving),
                                      std::make_move_iterator(scene.models.end()));
  scene.models.erase(first_leaving, scene.models.end());
  for (const ModelInstances& instances : scene.models) {
    wanted.erase(InstancesKey{instances.rsm_file, instances.variant_rank});
  }

  std::set<std::string> missing_textures;
  // LES TEXTURES, LUES UNE FOIS PAR NOM pour toute la carte, et leur échec
  // retenu comme leur succès.
  //
  // ⚠ PAR NOM NORMALISÉ, comme le VFS les cherche : deux modèles écrivent la
  // même texture avec des casses différentes, et une clé brute la lirait deux
  // fois.
  std::map<std::string, std::shared_ptr<const formats::Image>> read_textures;
  // ⚠ ET CELLES QUE LA SCÈNE A DÉJÀ NE SE RELISENT PAS : une variante se rebâtit
  // à chaque image d'un glissé de sommet, et décoder ses images à chaque fois
  // coûtait plus que tout le reste.
  if (!wanted.empty()) {
    for (const std::vector<ModelInstances>* models : {&scene.models, &leaving}) {
      for (const ModelInstances& known : *models) {
        for (std::size_t rank = 0;
             rank < known.textures.size() && rank < known.mesh.textures.size(); ++rank) {
          if (known.textures[rank] != nullptr) {
            read_textures.try_emplace(formats::NormalizeGrfPath(known.mesh.textures[rank]),
                                      known.textures[rank]);
          }
        }
      }
    }
  }
  // ⚠ UN MODÈLE MASQUÉ SE LIT AUSSI : l'éditeur peut le démasquer sans relire
  // la carte.
  for (const InstancesKey& key : wanted) {
    const std::vector<mapedit::VertexMove>& moves = MovesOf(scene, key);
    // UNE VARIANTE repart du fichier gardé, s'il l'est déjà.
    std::optional<formats::RsmModel> model;
    const auto source = scene.variant_sources.find(std::string(key.first));
    if (!moves.empty() && source != scene.variant_sources.end()) {
      model = *source->second.model;
    } else {
      // ReadRaw : le nom vient du RSW, donc c'est déjà du CP949.
      auto bytes = vfs.ReadRaw("data\\model\\" + std::string(key.first));
      if (!bytes) {
        continue;
      }
      auto parsed = formats::RsmModel::Parse(*bytes);
      if (!parsed) {
        continue;
      }
      model = std::move(*parsed);
    }
    ModelInstances instances;
    instances.rsm_file = std::string(key.first);
    instances.variant_rank = key.second;
    instances.applied_moves = moves;
    if (!instances.applied_moves.empty()) {
      auto [kept, fresh] = scene.variant_sources.try_emplace(instances.rsm_file);
      if (fresh) {
        const ModelMesh untouched = BuildModelMesh(*model);
        kept->second =
            MapScene::VariantSource{.model = std::make_shared<const formats::RsmModel>(*model),
                                    .bounds = {untouched.bounds_min, untouched.bounds_max}};
      }
      instances.placement_bounds = kept->second.bounds;
      mapedit::MeshEdit edit_moves;
      edit_moves.vertices = instances.applied_moves;
      const mapedit::MapEditReport moved = mapedit::ApplyMeshEdit(edit_moves, *model, kFileNames);
      for (const std::string& refusal : moved.refused) {
        core::LogWarning("{} : sommet refusé — {}", CitedFileName(key.first), refusal);
      }
    }
    instances.mesh = BuildModelMesh(*model);
    instances.rig = BuildModelRig(*model, instances.mesh);
    instances.rsm_version = model->version();
    instances.shade_type = model->shade_type();
    for (const std::string& texture : instances.mesh.textures) {
      ++scene.model_textures_cited;
      auto [kept, fresh] = read_textures.try_emplace(formats::NormalizeGrfPath(texture));
      if (fresh) {
        ++scene.model_textures_read;
        if (std::optional<formats::Image> image = LoadSceneTexture(vfs, texture)) {
          kept->second = std::make_shared<const formats::Image>(std::move(*image));
        }
      }
      if (kept->second == nullptr) {
        missing_textures.insert(texture);
      }
      instances.textures.push_back(kept->second);
    }
    scene.models.push_back(std::move(instances));
  }
  // Une texture manquante ne se compte qu'une fois, si souvent que sa variante
  // se rebâtisse.
  for (const std::string& missing : missing_textures) {
    if (std::ranges::find(scene.missing_model_textures, missing) ==
        scene.missing_model_textures.end()) {
      scene.missing_model_textures.push_back(missing);
    }
  }
}

// Les retouches de maillages de `edit` dont la cible correspond ; les autres
// vont dans les refus de `report`.
//
// UNE POSE DU RSW SE RECONNAÎT À SON PLACEMENT D'ORIGINE, et non à celui que
// l'édition lui donne : déplacer la pose garde la retouche de son maillage.
// Les retouches des poses du RSW se vérifient donc AVANT l'édition, celles des
// modèles ajoutés après — ils n'existent qu'une fois l'édition posée.
enum class MeshTargets { kRswModels, kAddedModels };

void CheckMeshEdits(const mapedit::MapEdit& edit, const formats::RswScene& rsw,
                    std::size_t rsw_model_count, MeshTargets targets,
                    std::vector<mapedit::MeshEdit>& valid, mapedit::MapEditReport& report) {
  for (const mapedit::MeshEdit& mesh : edit.meshes()) {
    if ((mesh.model_index < rsw_model_count) != (targets == MeshTargets::kRswModels)) {
      continue;
    }
    std::string refusal = mapedit::MeshTargetRefusal(mesh, rsw, kFileNames);
    if (refusal.empty()) {
      valid.push_back(mesh);
      ++report.applied;
    } else {
      report.refused.push_back(std::move(refusal));
    }
  }
}

// Les opérations de maillage rejoignent le bilan de l'édition.
void MergeMeshReport(mapedit::MapEditReport mesh_report, mapedit::MapEditReport& report) {
  report.applied += mesh_report.applied;
  for (std::string& refusal : mesh_report.refused) {
    report.refused.push_back(std::move(refusal));
  }
}

// La matrice de la pose `rank`, reprise de `scene.placed_poses` quand rien de
// ce qui la fait n'a changé — placement, boîte, version —, recalculée sinon.
glm::mat4 PoseMatrix(MapScene& scene, std::size_t rank, const formats::RswModel& placement,
                     const ModelMesh& centred_on, std::int32_t rsm_version) {
  if (scene.placed_poses.size() != scene.rsw.models().size()) {
    scene.placed_poses.assign(scene.rsw.models().size(), std::nullopt);
  }
  std::optional<MapScene::PlacedPose>& kept = scene.placed_poses[rank];
  if (kept.has_value() && kept->position == placement.position &&
      kept->rotation == placement.rotation && kept->scale == placement.scale &&
      kept->bounds_min == centred_on.bounds_min && kept->bounds_max == centred_on.bounds_max &&
      kept->rsm_version == rsm_version) {
    return kept->matrix;
  }
  kept = MapScene::PlacedPose{
      .position = placement.position,
      .rotation = placement.rotation,
      .scale = placement.scale,
      .bounds_min = centred_on.bounds_min,
      .bounds_max = centred_on.bounds_max,
      .rsm_version = rsm_version,
      .matrix = ModelInstanceMatrix(placement, scene.gnd.width(), scene.gnd.height(), centred_on,
                                    rsm_version)};
  return kept->matrix;
}

// Pose les modèles du RSW de `scene` sur les maillages chargés : une pose par
// modèle, sauf les masqués, ceux dont le fichier manque et ceux posés hors de
// la carte. Les poses d'avant s'effacent, et les compteurs se refont.
void PlaceSceneInstances(MapScene& scene) {
  std::map<InstancesKey, std::size_t> by_key;
  for (std::size_t index = 0; index < scene.models.size(); ++index) {
    ModelInstances& instances = scene.models[index];
    by_key.emplace(InstancesKey{instances.rsm_file, instances.variant_rank}, index);
    instances.transforms.clear();
    instances.rsw_ranks.clear();
    instances.animated.clear();
  }
  scene.unreadable_models = 0;
  scene.off_map_instances = 0;
  scene.model_instance_count = 0;
  scene.animated_instance_count = 0;
  const std::vector<formats::RswModel>& placements = scene.rsw.models();
  for (std::size_t rank = 0; rank < placements.size(); ++rank) {
    const formats::RswModel& placement = placements[rank];
    const auto found = by_key.find(KeyOf(scene, rank));
    if (found == by_key.end()) {
      ++scene.unreadable_models;
      continue;
    }
    if (std::ranges::binary_search(scene.hidden_models, rank)) {
      continue;
    }
    ModelInstances& instances = scene.models[found->second];
    // La pose se recentre sur la boîte d'origine quand l'édition en a déplacé
    // des sommets : voir `ModelInstances::placement_bounds`.
    ModelMesh bounds_only;
    const ModelMesh* centred_on = &instances.mesh;
    if (instances.placement_bounds.has_value()) {
      bounds_only.bounds_min = (*instances.placement_bounds)[0];
      bounds_only.bounds_max = (*instances.placement_bounds)[1];
      centred_on = &bounds_only;
    }
    const glm::mat4 instance =
        PoseMatrix(scene, rank, placement, *centred_on, instances.rsm_version);
    if (!IsInstanceOnMap(instance, scene.gnd.width(), scene.gnd.height())) {
      ++scene.off_map_instances;
      continue;
    }
    ++scene.model_instance_count;
    if (AddInstance(instances, instance, placement, rank)) {
      ++scene.animated_instance_count;
    }
  }
}

}  // namespace

mapedit::MapEdit LoadMapEdit(vfs::Vfs& vfs, std::string_view map_name) {
  auto bytes = vfs.Read(mapedit::MapEditPath(map_name));
  if (!bytes) {
    return {};
  }
  auto edit = mapedit::ParseMapEdit(
      std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()), map_name);
  if (!edit) {
    core::LogWarning("{} — {} s'ouvre sans édition", core::Describe(edit.error()), map_name);
    return {};
  }
  return std::move(*edit);
}

mapedit::MapEditReport ReplaceSceneModels(MapScene& scene, const mapedit::MapEdit& edit,
                                          vfs::Vfs* vfs) {
  for (std::size_t rank = 0; rank < scene.unfixed_models.size(); ++rank) {
    const formats::RswModel& original = scene.unfixed_models[rank];
    scene.rsw.PlaceModel(rank, original.position, original.rotation, original.scale);
  }
  // Les modèles ajoutés repartent de zéro : l'édition les repose tous.
  scene.rsw.TruncateModels(scene.unfixed_models.size());
  std::vector<mapedit::MeshEdit> mesh_edits;
  mapedit::MapEditReport mesh_report;
  CheckMeshEdits(edit, scene.rsw, scene.unfixed_models.size(), MeshTargets::kRswModels, mesh_edits,
                 mesh_report);
  mapedit::MapEditReport report = mapedit::ApplyMapEdit(edit, scene.rsw, kFileNames);
  CheckMeshEdits(edit, scene.rsw, scene.unfixed_models.size(), MeshTargets::kAddedModels,
                 mesh_edits, mesh_report);
  MergeMeshReport(std::move(mesh_report), report);
  scene.mesh_edits = std::move(mesh_edits);
  if (vfs != nullptr) {
    LoadMissingSceneModels(*vfs, scene);
  }
  scene.hidden_models = report.hidden;
  PlaceSceneInstances(scene);
  return report;
}

std::string GroundFilePath(const vfs::ResNameTable& res_names, const formats::RswScene& rsw,
                           std::string_view map_name) {
  const std::string& ground_file = rsw.header().ground_file;
  if (ground_file.empty()) {
    return res_names.MapFilePath(std::string(map_name) + ".gnd");
  }
  return res_names.MapFilePath(ground_file);
}

void RebuildSceneGround(MapScene& scene, std::int32_t subdivisions) {
  if (scene.ground_subdivisions == subdivisions) {
    return;
  }
  scene.ground_subdivisions = subdivisions;
  scene.terrain = BuildTerrainMesh(scene.gnd, scene.atlas, scene.invisible_tiles, subdivisions);
}

mapedit::MapEditReport ReshapeSceneGround(MapScene& scene,
                                          std::span<const mapedit::GroundEdit> edits) {
  formats::GndMap& gnd = scene.gnd;
  std::size_t rank = 0;
  for (std::int32_t y = 0; y < gnd.height(); ++y) {
    for (std::int32_t x = 0; x < gnd.width(); ++x) {
      if (rank < scene.file_corner_heights.size()) {
        gnd.SetCornerHeights(x, y, scene.file_corner_heights[rank]);
      }
      ++rank;
    }
  }
  scene.ground_edits.clear();
  if (scene.relief_level.has_value()) {
    return {};
  }
  scene.ground_edits.assign(edits.begin(), edits.end());
  return mapedit::ApplyGroundEdits(edits, gnd);
}

void RebuildSceneTerrain(MapScene& scene) {
  scene.terrain =
      BuildTerrainMesh(scene.gnd, scene.atlas, scene.invisible_tiles, scene.ground_subdivisions);
}

core::Result<MapScene> LoadMapScene(vfs::Vfs& vfs, std::string_view map_name,
                                    const MapSceneOptions& options) {
  auto rsw_bytes = vfs.Read(vfs.res_names().MapFilePath(std::string(map_name) + ".rsw"));
  if (!rsw_bytes) {
    return std::unexpected(rsw_bytes.error());
  }
  auto rsw = formats::RswScene::Parse(*rsw_bytes);
  if (!rsw) {
    return std::unexpected(rsw.error());
  }
  // LES RETOUCHES DU STAFF, sur les données lues et avant tout le reste : voir
  // docs/adr/0118. Le placement d'origine se garde pour l'éditeur.
  std::vector<formats::RswModel> unfixed_models = rsw->models();
  const mapedit::MapEdit edit =
      options.edit.has_value() ? *options.edit : LoadMapEdit(vfs, map_name);
  std::vector<mapedit::MeshEdit> mesh_edits;
  mapedit::MapEditReport mesh_report;
  CheckMeshEdits(edit, *rsw, unfixed_models.size(), MeshTargets::kRswModels, mesh_edits,
                 mesh_report);
  mapedit::MapEditReport fixed = mapedit::ApplyMapEdit(edit, *rsw, kFileNames);
  CheckMeshEdits(edit, *rsw, unfixed_models.size(), MeshTargets::kAddedModels, mesh_edits,
                 mesh_report);
  MergeMeshReport(std::move(mesh_report), fixed);
  for (const std::string& refusal : fixed.refused) {
    core::LogWarning("édition de {} refusée — {}", map_name, refusal);
  }
  if (fixed.applied > 0) {
    core::LogInfo("{} : {} opération(s) d'édition posée(s)", map_name, fixed.applied);
  }

  // ReadRaw : `ground_file` vient du RSW, donc c'est déjà du CP949.
  auto gnd_bytes = vfs.ReadRaw(GroundFilePath(vfs.res_names(), *rsw, map_name));
  if (!gnd_bytes) {
    return std::unexpected(gnd_bytes.error());
  }
  auto gnd = formats::GndMap::Parse(*gnd_bytes);
  if (!gnd) {
    return std::unexpected(gnd.error());
  }

  LightmapAtlas atlas = BuildLightmapAtlas(*gnd);
  SceneLight light = MakeSceneLight(rsw->light());

  MapScene scene{
      .gnd = std::move(*gnd),
      .rsw = std::move(*rsw),
      .unfixed_models = std::move(unfixed_models),
      .hidden_models = fixed.hidden,
      .mesh_edits = std::move(mesh_edits),
      .atlas = std::move(atlas),
      .light = light,
  };

  // L'APLATISSEMENT AVANT LE MAILLAGE, qui se bâtit sur les hauteurs. Les objets
  // d'effet se reposent d'abord : il leur faut le sol d'ORIGINE sous eux.
  if (options.flatten_relief) {
    if (const std::optional<float> level = FlatReliefLevel(scene.gnd); level.has_value()) {
      ReseatMapEffects(scene.rsw, scene.gnd, *level);
      scene.gnd.LevelCornerHeights(*level);
      scene.relief_level = level;
    }
  }
  // LE RELIEF RETOUCHÉ, sur les hauteurs du fichier, que l'éditeur garde pour
  // le reposer sans relire la carte.
  scene.file_corner_heights.reserve(scene.gnd.cubes().size());
  for (const formats::GndCube& cube : scene.gnd.cubes()) {
    scene.file_corner_heights.push_back(cube.corner_height);
  }
  const mapedit::MapEditReport reshaped = ReshapeSceneGround(scene, edit.ground());
  for (const std::string& refusal : reshaped.refused) {
    core::LogWarning("édition de {} refusée — {}", map_name, refusal);
  }

  // Les textures AVANT le maillage : c'est en les lisant que l'on sait quels
  // carreaux sont des trous, et un trou ne s'émet pas.
  scene.terrain_textures = LoadTerrainTextures(vfs, scene.gnd, scene.missing_terrain_textures);
  scene.invisible_tiles = FindInvisibleTiles(scene.gnd, scene.terrain_textures);
  scene.ground_subdivisions = options.ground_subdivisions;
  scene.terrain =
      BuildTerrainMesh(scene.gnd, scene.atlas, scene.invisible_tiles, scene.ground_subdivisions);
  // 🔴 L'EAU PARTAGE LA LISTE DES TROUS AVEC LE TERRAIN, et il le faut : le
  // client ne pose d'eau que là où le sol a dessiné un quad. Sans elle, une
  // carte trouée se couvre d'une nappe par-dessus le vide.
  scene.water = BuildWaterPlane(scene.gnd, scene.rsw, scene.invisible_tiles);
  scene.water_animations = LoadWaterAnimations(vfs, scene.water, scene.missing_water_textures);
  // Le décor est chargé APRÈS le terrain : s'il échoue, la carte reste
  // affichable, ce qui vaut mieux qu'un écran noir.
  //
  // ⚠ SAUF SUR UN RELIEF APLATI : voir `MapSceneOptions::flatten_relief`.
  if (!scene.relief_level.has_value()) {
    LoadMissingSceneModels(vfs, scene);
    PlaceSceneInstances(scene);
  }
  // LA TABLE DES EFFETS DE LA CARTE.
  scene.effect_table = LoadMapEffectTable(vfs, map_name);
  // Le sprite des cheminées, seulement pour une carte qui en a.
  if (std::ranges::any_of(scene.rsw.effects(), [](const formats::RswEffect& object) {
        return object.number == kMapChimneySmokeEffect;
      })) {
    scene.chimney_smoke = LoadChimneySmoke(vfs);
  }
  // Les textures des émetteurs et du vent. ⚠ AVEC LA CLÉ MAGENTA : le client
  // les charge par le chargeur d'images commun, celui des textures d'un `.str`,
  // dont le magenta pur est la transparence.
  for (const std::string& texture : MapEffectTextures(scene.effect_table, scene.rsw.effects())) {
    std::optional<formats::Image> image = LoadSceneTexture(vfs, texture, SceneTextureKey::kEffect);
    if (!image) {
      ++scene.missing_effect_textures;
      continue;
    }
    scene.effect_textures.emplace(MapEffectTexturePath(texture), std::move(*image));
  }

  // Le ciel EN DERNIER : une table absente ou illisible ne doit pas empêcher
  // d'afficher une carte, et la plupart des cartes n'y figurent pas.
  const MapSkyLook sky = LoadMapSky(vfs, map_name);
  scene.background = sky.background;
  scene.sky_effects = sky.effects;
  scene.weather_effects = LoadMapWeather(vfs, map_name);
  scene.parameterised_sky_clouds = sky.parameterised_clouds;
  scene.background_fog = sky.background_fog;
  scene.fog = LoadMapFog(vfs, map_name);
  return scene;
}

}  // namespace ro::render

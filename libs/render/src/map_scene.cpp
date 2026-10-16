#include "ro/render/map_scene.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

#include "ro/core/log.h"
#include "ro/formats/grf.h"
#include "ro/formats/rsm.h"
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
std::vector<ModelInstances> LoadSceneModels(vfs::Vfs& vfs, const formats::RswScene& rsw,
                                            MapScene& scene) {
  std::map<std::string, ModelInstances> by_file;
  std::set<std::string> unreadable_files;
  std::set<std::string> missing_textures;
  // LES TEXTURES, LUES UNE FOIS PAR NOM pour toute la carte, et leur échec
  // retenu comme leur succès.
  //
  // ⚠ PAR NOM NORMALISÉ, comme le VFS les cherche : deux modèles écrivent la
  // même texture avec des casses différentes, et une clé brute la lirait deux
  // fois.
  std::map<std::string, std::shared_ptr<const formats::Image>> read_textures;

  // ⚠ UN MODÈLE MASQUÉ SE LIT AUSSI : l'établi peut le démasquer sans relire
  // la carte.
  for (const formats::RswModel& placement : rsw.models()) {
    if (by_file.contains(placement.model_name) || unreadable_files.contains(placement.model_name)) {
      continue;
    }
    {
      // ReadRaw : le nom vient du RSW, donc c'est déjà du CP949.
      auto bytes = vfs.ReadRaw("data\\model\\" + placement.model_name);
      if (!bytes) {
        unreadable_files.insert(placement.model_name);
        continue;
      }
      auto model = formats::RsmModel::Parse(*bytes);
      if (!model) {
        unreadable_files.insert(placement.model_name);
        continue;
      }

      ModelInstances instances;
      instances.rsm_file = placement.model_name;
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
      by_file.emplace(placement.model_name, std::move(instances));
    }
  }

  scene.missing_model_textures.assign(missing_textures.begin(), missing_textures.end());

  std::vector<ModelInstances> models;
  models.reserve(by_file.size());
  for (auto& [name, instances] : by_file) {
    models.push_back(std::move(instances));
  }
  return models;
}

// Le nom en UTF-8 d'un fichier que le RSW cite en CP949 : ce que l'édition
// compare à celui qu'elle désigne.
std::string CitedFileName(std::string_view raw) { return vfs::Cp949ToUtf8Lossy(raw); }

// Pose les modèles du RSW de `scene` sur les maillages chargés : une pose par
// modèle, sauf les masqués, ceux dont le fichier manque et ceux posés hors de
// la carte. Les poses d'avant s'effacent, et les compteurs se refont.
void PlaceSceneInstances(MapScene& scene) {
  std::map<std::string_view, std::size_t> by_file;
  for (std::size_t index = 0; index < scene.models.size(); ++index) {
    ModelInstances& instances = scene.models[index];
    by_file.emplace(instances.rsm_file, index);
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
    const auto found = by_file.find(placement.model_name);
    if (found == by_file.end()) {
      ++scene.unreadable_models;
      continue;
    }
    if (std::ranges::binary_search(scene.hidden_models, rank)) {
      continue;
    }
    ModelInstances& instances = scene.models[found->second];
    const glm::mat4 instance = ModelInstanceMatrix(placement, scene.gnd.width(), scene.gnd.height(),
                                                   instances.mesh, instances.rsm_version);
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

mapedit::MapEditReport ReplaceSceneModels(MapScene& scene, const mapedit::MapEdit& edit) {
  for (std::size_t rank = 0; rank < scene.unfixed_models.size(); ++rank) {
    const formats::RswModel& original = scene.unfixed_models[rank];
    scene.rsw.PlaceModel(rank, original.position, original.rotation, original.scale);
  }
  mapedit::MapEditReport report = mapedit::ApplyMapEdit(edit, scene.rsw, &CitedFileName);
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
  const mapedit::MapEditReport fixed = mapedit::ApplyMapEdit(
      options.edit.has_value() ? *options.edit : LoadMapEdit(vfs, map_name), *rsw, &CitedFileName);
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

  // Les textures AVANT le maillage : c'est en les lisant que l'on sait quels
  // carreaux sont des trous, et un trou ne s'émet pas.
  scene.terrain_textures = LoadTerrainTextures(vfs, scene.gnd, scene.missing_terrain_textures);
  const std::vector<std::uint8_t> invisible_tiles =
      FindInvisibleTiles(scene.gnd, scene.terrain_textures);
  scene.terrain = BuildTerrainMesh(scene.gnd, scene.atlas, invisible_tiles);
  // 🔴 L'EAU PARTAGE LA LISTE DES TROUS AVEC LE TERRAIN, et il le faut : le
  // client ne pose d'eau que là où le sol a dessiné un quad. Sans elle, une
  // carte trouée se couvre d'une nappe par-dessus le vide.
  scene.water = BuildWaterPlane(scene.gnd, scene.rsw, invisible_tiles);
  scene.water_animations = LoadWaterAnimations(vfs, scene.water, scene.missing_water_textures);
  // Le décor est chargé APRÈS le terrain : s'il échoue, la carte reste
  // affichable, ce qui vaut mieux qu'un écran noir.
  //
  // ⚠ SAUF SUR UN RELIEF APLATI : voir `MapSceneOptions::flatten_relief`.
  if (!scene.relief_level.has_value()) {
    scene.models = LoadSceneModels(vfs, scene.rsw, scene);
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

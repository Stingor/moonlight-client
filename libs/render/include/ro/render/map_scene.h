#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/gnd.h"
#include "ro/formats/image.h"
#include "ro/formats/rsm.h"
#include "ro/formats/rsw.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/lightmap_atlas.h"
#include "ro/render/map_effect_table.h"
#include "ro/render/map_fog.h"
#include "ro/render/map_smoke.h"
#include "ro/render/model_mesh.h"
#include "ro/render/scene_light.h"
#include "ro/render/terrain_mesh.h"
#include "ro/render/water_plane.h"
#include "ro/vfs/res_name_table.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// Lit une texture de scène, sous le dossier des textures du jeu.
//
// ⚠ Le nom vient d'un fichier du JEU : ce sont des octets déjà écrits dans la
// page de codes de l'archive, et les passer au chemin transcodé les
// transcoderait une seconde fois — presque toutes les textures ont un nom
// coréen, et presque aucune ne serait retrouvée.
//
// Une texture absente rend RIEN plutôt que d'interrompre : le renderer la
// remplace par du magenta, et le défaut se voit à l'ecran au lieu de faire
// disparaître ce qui la portait.
//
// LA CLÉ dépend de ce qui la porte. Le sol et le décor suivent le jeu, selon
// le fichier : une BMP prend `formats::ApplyTextureColorKey`, le reste le
// magenta pur. Les effets de la carte gardent le magenta pur.
enum class SceneTextureKey : std::uint8_t { kGroundAndDecor, kEffect };

[[nodiscard]] std::optional<formats::Image> LoadSceneTexture(
    vfs::Vfs& vfs, const std::string& relative,
    SceneTextureKey key = SceneTextureKey::kGroundAndDecor);

// Tout ce qu'il faut pour dessiner une carte, une fois lu depuis le VFS.
//
// La structure ne contient AUCUN objet OpenGL : elle est produite hors de tout
// contexte graphique, et deux programmes très différents s'en servent — l'outil
// hors-jeu qui écrit un PNG, et l'application qui ouvre une fenêtre. C'est ce
// qui évite que le second réécrive le chargement du premier.
struct MapScene {
  formats::GndMap gnd;
  // Le RSW, retouches du staff posées.
  formats::RswScene rsw;
  // Ses modèles tels que le fichier les pose, avant toute retouche : ce que
  // l'établi rétablit, et la position d'origine qu'une retouche vérifie.
  std::vector<formats::RswModel> unfixed_models;
  // Les rangs des modèles que les retouches masquent, croissants.
  std::vector<std::size_t> hidden_models;
  // Les retouches de maillages dont la cible correspond : ce que le décor
  // dessine, variantes comprises.
  std::vector<mapedit::MeshEdit> mesh_edits;
  // LA MATRICE DE CHAQUE POSE, par rang, et ce dont elle est faite : reposer le
  // décor ne recalcule que celles dont le placement ou la boîte ont changé.
  // L'éditeur repose le décor à chaque image d'un glissé, où une seule pose
  // bouge.
  struct PlacedPose {
    std::array<float, 3> position{};
    std::array<float, 3> rotation{};
    std::array<float, 3> scale{};
    std::array<float, 3> bounds_min{};
    std::array<float, 3> bounds_max{};
    std::int32_t rsm_version = 0;
    glm::mat4 matrix{1.0F};
  };
  std::vector<std::optional<PlacedPose>> placed_poses;
  // LES FICHIERS DES VARIANTES, tels qu'ils se lisent, et la boîte de leur
  // maillage intact : une variante se rebâtit à chaque image d'un glissé de
  // sommet, sans relire ni réanalyser son fichier. Seuls les fichiers qui ont
  // une variante y entrent, pour ne pas garder tout le décor en double.
  struct VariantSource {
    std::shared_ptr<const formats::RsmModel> model;
    std::array<std::array<float, 3>, 2> bounds{};
  };
  std::map<std::string, VariantSource> variant_sources;
  LightmapAtlas atlas;
  TerrainMesh terrain;
  std::vector<formats::Image> terrain_textures;
  // Les carreaux que le maillage n'émet pas : voir `FindInvisibleTiles`.
  // Gardés pour rebâtir le sol sans relire ses textures.
  std::vector<std::uint8_t> invisible_tiles;
  // Le découpage dont `terrain` est bâti : voir `BuildTerrainMesh`.
  std::int32_t ground_subdivisions = kFlatGroundSubdivisions;
  std::vector<ModelInstances> models;
  SceneLight light;

  // LE NIVEAU DU RELIEF APLATI, dans l'unité et le sens du fichier, ou rien
  // quand la carte garde le sien. Voir `MapSceneOptions::flatten_relief`.
  std::optional<float> relief_level;

  // LA NAPPE D'EAU, vide sur une carte qui n'en a pas — la plupart des
  // souterrains. `water_animations` porte un jeu d'images par TYPE employé.
  WaterPlane water;
  std::vector<WaterAnimation> water_animations;

  // Ce que le client peint AU-DELÀ des bords, quand la carte flotte et qu'on
  // les voit. Vide quand la table du ciel ne cite pas la carte : le fond est
  // alors noir, voir `MapClearColorOf`.
  std::optional<std::array<float, 3>> background;

  // LE CIEL ANIMÉ : les identifiants d'EFFET que cette carte allume — nuages,
  // brume, étoiles. Vide pour les quatre cartes sur cinq qui n'en ont pas.
  // Voir `map_sky.h` : ce ne sont pas des effets à part, mais ceux du moteur.
  std::vector<std::uint32_t> sky_effects;
  // LA MÉTÉO : la neige, le sakura, les feux d'artifice que le client pose sur
  // le joueur à son entrée. Voir `map_weather.h`.
  std::vector<std::uint32_t> weather_effects;
  // Combien de ciels PARAMÉTRÉS la carte déclare, que nous ne portons pas
  // encore. Compté pour que le manque se lise au lieu de se deviner.
  std::size_t parameterised_sky_clouds = 0;

  // LE BROUILLARD DE LA CARTE, vide pour une carte que sa table ne cite pas.
  // Voir `map_fog.h`. Et si son FOND s'y mêle : voir `MapSkyLook`.
  std::optional<MapFog> fog;
  bool background_fog = false;

  // LE SPRITE DES CHEMINÉES (objets 44), chargé seulement quand la carte en a.
  // Voir `MapChimney`.
  std::optional<ChimneySmokeLook> chimney_smoke;

  // LA TABLE DES EFFETS DE LA CARTE, et les textures qu'emploieront ses
  // émetteurs (974) et ses anneaux de vent (2343), rangées par leur chemin dans
  // l'archive (`MapEffectTexturePath`). Voir `MapEffects`.
  MapEffectTable effect_table;
  std::map<std::string, formats::Image> effect_textures;

  // Relevés de chargement. Une donnée absente se voit à l'écran en magenta,
  // mais rien n'en dit la cause : ces compteurs la nomment.
  std::size_t missing_terrain_textures = 0;
  std::size_t missing_water_textures = 0;
  std::size_t missing_effect_textures = 0;
  std::vector<std::string> missing_model_textures;
  // Les textures que les modèles citent, et celles qu'il a fallu LIRE : une par
  // nom, quel que soit le nombre de modèles qui la citent. L'écart est ce que
  // le partage épargne.
  std::size_t model_textures_cited = 0;
  std::size_t model_textures_read = 0;
  std::size_t unreadable_models = 0;
  std::size_t off_map_instances = 0;
  std::size_t model_instance_count = 0;
  // Parmi elles, celles qui s'animent : le .rsw le demande ET un nœud du modèle
  // a de quoi tourner.
  std::size_t animated_instance_count = 0;
};

// Le chemin du fichier de sol d'une carte, le clonage appliqué.
//
// Le RSW le nomme, mais le champ est parfois vide : on retombe alors sur le nom
// de la carte. Règle partagée par le rendu et par le recoupement de cellules,
// donc énoncée à un seul endroit.
//
// ⚠ LE RELIEF SE TRADUIT POUR LUI-MÊME, sans regarder ce qu'est devenue la
// scène : neuf cartes les prennent à deux endroits différents. Voir
// `ro::vfs::ResNameTable`.
[[nodiscard]] std::string GroundFilePath(const vfs::ResNameTable& res_names,
                                         const formats::RswScene& rsw, std::string_view map_name);

// Charge une carte entière : sol, terrain, lumière et décor.
//
// Échoue seulement si le RSW ou le GND manquent — sans eux il n'y a rien à
// montrer. Une texture ou un modèle illisible est COMPTÉ, pas propagé : mieux
// vaut une carte trouée qu'un écran noir, et le relevé dit quoi corriger.
//
// CE QU'ON DEMANDE EN PLUS de la carte elle-même.
struct MapSceneOptions {
  // LE RELIEF RAMENÉ À UNE SEULE ALTITUDE (GreyWorld) : voir `FlatReliefLevel`.
  // Les objets d'effet suivent le sol, et les décors ne se chargent PAS — ils
  // garderaient leur hauteur d'origine et flotteraient au-dessus du plan.
  //
  // ⚠ C'EST AU CHARGEMENT, parce que le maillage du terrain se bâtit sur les
  // hauteurs. Changer d'avis demande de relire la carte.
  bool flatten_relief = false;
  // LE SOL ARRONDI : les sous-carreaux par côté de cube. Voir
  // `BuildTerrainMesh` ; `RebuildSceneGround` en change sans relire la carte.
  std::int32_t ground_subdivisions = kFlatGroundSubdivisions;
  // L'ÉDITION À POSER au lieu de celle de l'installation — voir `LoadMapEdit`.
  // L'éditeur y met le document qu'il tient ; un banc ou un outil, celui qu'il
  // éprouve.
  std::optional<mapedit::MapEdit> edit;
};

[[nodiscard]] core::Result<MapScene> LoadMapScene(vfs::Vfs& vfs, std::string_view map_name,
                                                  const MapSceneOptions& options = {});

// REBÂTIT LE MAILLAGE DU SOL de `scene` en `subdivisions` sous-carreaux par côté
// de cube, sans relire la carte : c'est ce que demande le réglage « Sol
// arrondi » quand il change. Rien n'est fait quand le sol l'est déjà. À
// téléverser ensuite au renderer.
void RebuildSceneGround(MapScene& scene, std::int32_t subdivisions);

// L'ÉDITION DE `map_name` dans l'installation : voir docs/adr/0118. Une carte
// sans document n'en a aucune ; un document illisible n'en donne aucune, et le
// journal le dit.
[[nodiscard]] mapedit::MapEdit LoadMapEdit(vfs::Vfs& vfs, std::string_view map_name);

// REPOSE LE DÉCOR DE `scene` SELON `edit`, sans relire la carte : les modèles
// reprennent leur placement d'origine, les retouches se posent, et les poses se
// recalculent sur les maillages déjà chargés. C'est ce qui rend l'édition en
// direct immédiate. Un modèle que la carte n'a pas pu lire ne revient pas.
//
// `vfs`, quand il est donné, lit les fichiers des modèles ajoutés qui manquent
// encore ; sans lui, un modèle ajouté dont le fichier n'est pas chargé ne se
// montre pas.
//
// À téléverser ensuite au renderer, comme au chargement.
mapedit::MapEditReport ReplaceSceneModels(MapScene& scene, const mapedit::MapEdit& edit,
                                          vfs::Vfs* vfs = nullptr);

}  // namespace ro::render

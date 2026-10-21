#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/core/load_progress.h"
#include "ro/formats/gat.h"
#include "ro/mapedit/map_edit.h"
#include "ro/render/ground_light.h"
#include "ro/render/map_scene.h"
#include "ro/render/model_renderer.h"
#include "ro/render/scene_fog.h"
#include "ro/render/terrain_renderer.h"
#include "ro/render/water_renderer.h"
#include "ro/sim/walk_map.h"
#include "ro/sim/walk_path.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/vfs.h"

#include "world/map_edit_store.h"

namespace moonclient {

// La carte en cours, avec tout ce que le jeu lui demande.
//
// Le décor qu'on dessine et la grille qu'on parcourt viennent de la MÊME carte
// et changent ensemble ; les laisser se charger séparément permettrait de
// dessiner l'une en marchant sur l'autre.
struct MapStage {
  // Le nom sans extension, celui qui a servi à la charger.
  std::string name;
  ro::render::MapScene scene;
  ro::sim::WalkMap walk_map;
  // La grille telle qu'elle a été lue. Elle porte les HAUTEURS, dont la
  // marchabilité ne garde rien : c'est sur elles qu'un acteur se pose.
  std::optional<ro::formats::GatMap> gat;
  // La lumière peinte sur la carte, telle qu'un sprite la reçoit. Vide sans
  // grille : c'est elle qui dit où un acteur se tient au-dessus du sol.
  ro::render::GroundLight ground_light;
  // La même, SANS l'ombre cuite : celle des acteurs quand l'ombre du soleil
  // remplace celle de la carte.
  ro::render::GroundLight ground_laid_light;
  // LES HAUTEURS DE LA GRILLE TELLES QUE LE FICHIER LES PORTE, cellule par
  // cellule, avant les retouches du relief — aplanies si la carte l'est. Voir
  // `ReshapeStageGround`.
  std::vector<std::array<float, 4>> file_walk_heights;
  // LE MAILLAGE DU SOL NE PORTE PAS ENCORE le relief des hauteurs : un glissé
  // de brosse trop coûteux à remailler image par image attend son relâcher.
  bool ground_mesh_stale = false;
  // Ce qu'a coûté le dernier remaillage du relief, en millisecondes ; rien
  // avant le premier.
  std::optional<double> ground_mesh_ms;
  // LE RELIEF QU'ON A DEMANDÉ À LA LECTURE : aplati (GreyWorld) ou non.
  //
  // ⚠ PAS FORCÉMENT CELUI QU'ON A : une carte sans aucun sol ne s'aplatit pas,
  // et une relecture peut échouer. C'est la DEMANDE qu'on compare au réglage —
  // comparer le résultat redemanderait la même lecture à chaque image.
  bool flatten_asked = false;
  // Ce que la lecture a coûté, en millisecondes. Un chargement se VOIT — le jeu
  // s'arrête pendant qu'il dure —, et le mesurer est le seul moyen de savoir
  // s'il empire.
  std::uint32_t read_ms = 0;
};

// Charge une carte : sa géométrie, son décor, sa lumière, sa grille.
//
// ⚠ Échoue si la carte ne produit AUCUNE géométrie : il n'y aurait rien à
// montrer, et le dire vaut mieux qu'un écran vide sans explication.
//
// La grille, elle, est FACULTATIVE : sans elle on marche en ligne droite, ce
// qui traverse ce que le serveur contourne mais laisse le jeu jouable. Son
// absence se dit sur la sortie d'erreur, elle n'empêche pas la carte.
//
// Ce que la lecture demande en plus de la carte : voir `MapStageRead`.
struct MapStageRead {
  // Le sol ET la grille ramenés au même niveau (GreyWorld) : n'en aplatir qu'un
  // ferait flotter ou enfoncer tout le monde. ⚠ LA MARCHABILITÉ SE DÉRIVE
  // AVANT, du relief d'origine : c'est celle du serveur, et un sol aplati sous
  // le niveau d'eau changerait sinon des cellules en eau.
  bool flatten_relief = false;
  // Quand il est donné, le document qu'il tient pour cette carte, lu à sa
  // première entrée, se pose au lieu de celui de l'installation.
  MapEditStore* edits = nullptr;
  // « Ombres en temps réel » est allumé, et le relief retouché du document se
  // pose — voir `ShownGroundEdits`.
  bool real_time_shadows = false;
  // LE SOL ARRONDI DÈS LA LECTURE, et non à l'image suivante : le chargement
  // compte alors tout ce qu'il coûte. Voir `SyncStageGround`.
  bool rounded_ground = false;
  // L'avancement, dit à qui le montre. Nul : la lecture se tait.
  ro::core::LoadProgress* progress = nullptr;
};
[[nodiscard]] ro::core::Result<MapStage> LoadMapStage(ro::vfs::Vfs& vfs, std::string_view map_name,
                                                      const MapStageRead& read = {});

// Où se pose un acteur, en coordonnées du monde.
//
// L'altitude vient de la grille de MARCHABILITÉ quand la carte en a une : un
// acteur ne se tient pas sur le décor mais sur le sol que le serveur lui
// donne, et les deux diffèrent là où l'on marche AU-DESSUS du terrain. Sans
// grille, celle du terrain visible, qui vaut mieux que rien.
//
// Sur le SOL ARRONDI, l'acteur suit le sol dessiné là où la grille suit le
// terrain : voir `ro::render::RoundedStandingHeight`.
[[nodiscard]] std::array<float, 3> GroundPositionAt(const MapStage& stage, float cell_x,
                                                    float cell_y);

// LE SOL ARRONDI SELON LE RÉGLAGE : rebâtit le maillage du sol de `stage` et le
// téléverse dans `terrain` quand `rounded` ne correspond plus à ce qu'il porte.
// Rien sinon, ce qui permet de l'appeler à chaque image. L'erreur rendue est
// celle du téléversement.
[[nodiscard]] ro::core::Result<void> SyncStageGround(MapStage& stage,
                                                     ro::render::TerrainRenderer& terrain,
                                                     bool rounded);

// LE RELIEF SELON L'ÉDITION : quand `edits` diffère de ce que la scène porte,
// repose les hauteurs du sol et celles de la grille de marche — pour que les
// pieds suivent —, et marque le maillage périmé. Bon marché : seules des
// hauteurs se recopient, et l'éditeur l'appelle à chaque image. Rend vrai
// quand le relief a changé. La marchabilité ne bouge jamais.
bool ReshapeStageGround(MapStage& stage, std::span<const ro::mapedit::GroundEdit> edits);

// LES RETOUCHES DU RELIEF QUI SE VOIENT : celles du document sous « Ombres en
// temps réel », aucune sinon. L'ombre cuite de la carte est peinte pour le sol
// d'origine et ne suit pas un relief retouché ; les ombres en temps réel le
// suivent. Sans elles, le sol reste celui du fichier, cohérent avec ses ombres.
// Le document, lui, garde toujours ses retouches.
[[nodiscard]] std::span<const ro::mapedit::GroundEdit> ShownGroundEdits(
    const ro::mapedit::MapEdit& document, bool real_time_shadows);

// LE REMAILLAGE EN DIRECT : un glissé de brosse remaille le sol à chaque image
// tant que le dernier remaillage a coûté moins que cela, en millisecondes —
// deux images à 60 par seconde. Au-delà, le sol ne se remaille qu'au
// relâcher : les pieds et la brosse suivent déjà les hauteurs, seul le dessin
// attend. Le sol arrondi d'une carte de 200 × 200 cubes coûte environ 230 ms
// en Release ; le sol d'origine, une quinzaine.
inline constexpr double kLiveGroundRemeshMs = 33.0;
[[nodiscard]] bool RemeshesWhileStroking(const MapStage& stage);

// REMAILLE LE SOL PÉRIMÉ sur ses hauteurs, rebâtit la lumière que les acteurs
// reçoivent du sol, téléverse, et mesure ce que cela a coûté. Rien quand le
// maillage est à jour. L'erreur rendue est celle du téléversement.
[[nodiscard]] ro::core::Result<void> RemeshStageGround(MapStage& stage,
                                                       ro::render::TerrainRenderer& terrain);

// LE NOM DU JEU, que le joueur voit dans la barre de titre.
//
// ⚠ CE N'EST PAS LE NOM DU FICHIER : le joueur lance `MoonClient.exe` (voir
// OUTPUT_NAME dans app/CMakeLists.txt), et la fenêtre porte le nom du serveur
// dont elle est le client.
inline constexpr std::string_view kWindowTitle = "Moonlight-Destiny";

// LA MARQUE D'UN SERVEUR TEST, en tête du titre : on la voit jusque dans la
// barre des tâches, où une fenêtre de production lui ressemble trait pour trait.
inline constexpr std::string_view kTestServerTitleMark = "[TEST] ";

// LE TITRE DE LA FENÊTRE : le nom, puis la version du client — précédés de
// `kTestServerTitleMark` quand `test_server`.
//
// ⚠ LA VERSION NE VA PAS DANS `kWindowTitle` : celui-là est aussi le nom du
// fichier sur le disque, et l'exécutable que le joueur lance ne change pas de
// nom d'une publication à l'autre.
[[nodiscard]] std::string WindowTitle(bool test_server);

// LE TITRE EN PARTIE : le pseudo et la carte en tête quand `names_shown`, et
// tant que le monde les connaît — « Stingor - prontera - Moonlight-Destiny
// 1.2.3 ». Sans eux, le titre seul. Voir `WindowTitleOptions`.
[[nodiscard]] std::string WindowTitle(bool names_shown, const ro::sim::World& world,
                                      bool test_server);

// Ce qu'une carte chargée pèse, pour le journal.
void ReportMapStage(const MapStage& stage);

// La grille que le planificateur doit viser, ou rien s'il n'y en a pas : sans
// elle il marche en ligne droite, ce qui vaut mieux que de lire une grille vide.
[[nodiscard]] const ro::sim::WalkMap* PlannedMapOf(const MapStage& stage);

// Le fond peint AU-DELÀ des bords d'une carte : son ciel, ou du noir. Voir
// `ro::render::MapClearColorOf`.
[[nodiscard]] std::array<float, 4> BackgroundOf(const MapStage& stage);

// LE BROUILLARD DE LA CARTE, tel que les renderers le peignent : éteint quand
// le réglage l'est, ou que la carte n'en déclare pas.
[[nodiscard]] ro::render::SceneFog FogOf(const MapStage& stage, bool fog_shown);

// L'EFFACEMENT DE L'IMAGE : `background`, mêlé au brouillard de la carte quand
// son ciel le demande et que ce brouillard est allumé.
[[nodiscard]] std::array<float, 4> ClearColorOf(const MapStage& stage,
                                                const std::array<float, 4>& background,
                                                bool fog_shown);

// La caméra qui cadre la carte ENTIÈRE.
//
// C'est la pose de base d'une carte qu'on ouvre, et celle où la touche de
// recadrage revient.
[[nodiscard]] ro::render::Camera WholeMapCamera(const MapStage& stage, float aspect_ratio);

// Tout ce qu'une carte nouvellement annoncée fait bouger.
//
// Passé par pointeurs : poser une carte ne POSSÈDE rien, cela met à jour ce que
// la boucle tient déjà. Les regrouper évite neuf paramètres dont l'ordre
// deviendrait un piège.
struct MapArrival {
  ro::vfs::Vfs* vfs = nullptr;
  MapStage* stage = nullptr;
  ro::sim::WalkPlanner* planner = nullptr;
  ro::sim::World* world = nullptr;
  ro::render::TerrainRenderer* terrain = nullptr;
  // 🔴 LA NAPPE D'EAU EN FAIT PARTIE, et l'oublier ne laisse pas la carte sans
  // eau : elle laisse celle de la carte PRÉCÉDENTE. Sur gonryun, qui n'en a
  // aucune, la mer de la ville d'avant restait suspendue entre les plateaux —
  // un défaut qui ne se voit qu'en TÉLÉPORTANT, jamais au chargement.
  ro::render::WaterRenderer* water = nullptr;
  ro::render::ModelRenderer* models = nullptr;
  std::array<float, 4>* background = nullptr;
  // La dernière carte dont on a TENTÉ le chargement, réussi ou non.
  //
  // Une seule tentative par nom : réessayer à chaque image remplirait la
  // console d'une même erreur soixante fois par seconde.
  std::string* attempted = nullptr;
  // Le relief aplati de GreyWorld : voir `LoadMapStage`.
  bool flatten_relief = false;
  // Les documents d'édition que le jeu tient : voir `LoadMapStage`.
  MapEditStore* map_edits = nullptr;
  // « Ombres en temps réel » : le relief retouché se pose. Voir `LoadMapStage`.
  bool real_time_shadows = false;
  // Le sol arrondi du réglage, bâti dès la lecture : voir `MapStageRead`.
  bool rounded_ground = false;
  // L'avancement de la lecture ET du téléversement. Nul : rien ne se dit.
  ro::core::LoadProgress* progress = nullptr;
};

// LA CARTE ENVOYÉE À LA CARTE GRAPHIQUE : terrain, eau et modèles, ensemble. Un
// renderer oublié garderait la géométrie de la carte précédente. L'erreur rendue
// est celle d'un téléversement refusé.
[[nodiscard]] ro::core::Result<void> UploadStage(const MapStage& stage,
                                                 ro::render::TerrainRenderer& terrain,
                                                 ro::render::WaterRenderer& water,
                                                 ro::render::ModelRenderer& models,
                                                 ro::core::LoadProgress* progress = nullptr);

// Charge la carte que le monde annonce, la téléverse, et vise sa grille.
//
// Faute de pouvoir la charger, on garde ce qu'on a : le décor sera celui
// d'ailleurs, mais le client reste debout et le journal dit pourquoi. L'erreur
// rendue est FATALE — un téléversement refusé par la carte graphique — et elle
// seule ; tout le reste se rattrape.
[[nodiscard]] ro::core::Result<void> EnterAnnouncedMap(const MapArrival& arrival);

// LA CARTE COURANTE, RELUE SUR PLACE avec le relief que `arrival` demande.
//
// 🔴 SANS RIEN DEMANDER AU SERVEUR : la carte se relit et se téléverse, les
// acteurs, leurs chemins et la marchabilité restent ce qu'ils sont — elle ne
// dépend pas du relief. Un aller-retour par le serveur infligerait un écran de
// chargement, serait refusé en combat, et ferait perdre leur cible aux monstres.
//
// Rend vrai quand la carte a été relue. Faute de pouvoir la relire, on garde
// celle qu'on a, le journal dit pourquoi, et l'on rend faux. L'erreur rendue est
// FATALE — un téléversement refusé.
[[nodiscard]] ro::core::Result<bool> ReloadStageRelief(const MapArrival& arrival);

}  // namespace moonclient

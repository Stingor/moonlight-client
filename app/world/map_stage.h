#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "ro/core/error.h"
#include "ro/formats/gat.h"
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
// `flatten_relief` ramène le sol ET la grille au même niveau (GreyWorld) : n'en
// aplatir qu'un ferait flotter ou enfoncer tout le monde. ⚠ LA MARCHABILITÉ SE
// DÉRIVE AVANT, du relief d'origine : c'est celle du serveur, et un sol aplati
// sous le niveau d'eau changerait sinon des cellules en eau.
//
// `fixes`, quand il est donné, pose ses retouches au lieu de celles de
// l'installation : c'est la table que l'établi tient.
[[nodiscard]] ro::core::Result<MapStage> LoadMapStage(
    ro::vfs::Vfs& vfs, std::string_view map_name, bool flatten_relief = false,
    const ro::render::MapFixTable* fixes = nullptr);

// Où se pose un acteur, en coordonnées du monde.
//
// L'altitude vient de la grille de MARCHABILITÉ quand la carte en a une : un
// acteur ne se tient pas sur le décor mais sur le sol que le serveur lui
// donne, et les deux diffèrent là où l'on marche AU-DESSUS du terrain. Sans
// grille, celle du terrain visible, qui vaut mieux que rien.
[[nodiscard]] std::array<float, 3> GroundPositionAt(const MapStage& stage, float cell_x,
                                                    float cell_y);

// LE NOM QUE LE JOUEUR VOIT, et celui du fichier qu'il lance.
//
// ⚠ CE N'EST PAS LE NOM DU DÉPÔT, et les deux n'ont pas à coïncider. Le
// programme s'appelle `moonclient` dans son arbre, ses cibles, son espace de
// noms et son journal ; il s'appelle `Moonlight-Destiny` sur l'écran et sur le
// disque, parce que c'est le client de CE serveur qu'il remplace — et parce que
// c'est sous ce nom-là que les mises à jour livrent l'exécutable.
inline constexpr std::string_view kWindowTitle = "Moonlight-Destiny";

// LE TITRE DE LA FENÊTRE : le nom, puis la version du client.
//
// ⚠ LA VERSION NE VA PAS DANS `kWindowTitle` : celui-là est aussi le nom du
// fichier sur le disque, et l'exécutable que le joueur lance ne change pas de
// nom d'une publication à l'autre.
[[nodiscard]] std::string WindowTitle();

// LE TITRE EN PARTIE : le pseudo et la carte en tête quand `names_shown`, et
// tant que le monde les connaît — « Stingor - prontera - Moonlight-Destiny
// 1.2.3 ». Sans eux, le titre seul. Voir `WindowTitleOptions`.
[[nodiscard]] std::string WindowTitle(bool names_shown, const ro::sim::World& world);

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
  // La table de retouches que l'établi tient : voir `LoadMapStage`.
  const ro::render::MapFixTable* map_fixes = nullptr;
};

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

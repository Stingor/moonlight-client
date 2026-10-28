#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/core/load_progress.h"
#include "ro/formats/rsw.h"
#include "ro/render/map_scene.h"
#include "ro/sim/survivors_terrain.h"
#include "ro/vfs/vfs.h"

#include "survivors/arena_ground_compose.h"
#include "survivors/arena_ground_look.h"
#include "survivors/arena_ground_plan.h"
#include "world/map_stage.h"

namespace moonclient {

// LE SOL GÉNÉRÉ DESSINÉ : la fenêtre de 160 × 160 cases que l'arène montre
// autour du héros, assemblée en une `MapStage` synthétique — un GND, un GAT et
// un RSW bâtis en mémoire depuis le plan (`arena_ground_plan.h`), puis la scène
// et la grille comme celles d'une carte lue. `LitStage` la peint telle quelle :
// sol, décor, ombres du soleil, lumières ponctuelles, nuit et lanterne.

// CE QUI NE CHANGE PAS D'UNE FENÊTRE À L'AUTRE (ADR 0134, T5) : la règle, la
// graine, l'aspect, la lumière de la carte de référence, et une scène qui a lu
// une fois toutes les textures du sol et tous les modèles du biome. Bâtir une
// fenêtre ne lit plus aucun fichier.
struct GroundSources {
  const ro::sim::TerrainRule* rule = nullptr;
  std::uint64_t run_seed = 0;
  const BiomeLook* look = nullptr;
  // La lumière du RSW de la carte de référence.
  ro::formats::RswLight light;
  GroundPlanLight plan_light;
  // Les chemins des textures, dans l'ordre de `BiomeLook::textures`, puis les
  // pages de la terre composée ; ceux des modèles, tels que l'installation les
  // écrit (CP949).
  std::vector<std::string> texture_paths;
  std::map<std::string_view, std::string> model_files;
  // Les textures, les modèles et les tables de la carte de référence. Les
  // pages de la terre composée y tiennent leur place d'un texel : une fenêtre
  // les y trouve sans chercher de fichier, puis y pose les siennes.
  ro::render::MapScene library;
  // LA TERRE COMPOSÉE (`arena_ground_compose.h`) : l'herbe et la terre du
  // biome, et le rang de la première page dans `texture_paths`. Rien quand
  // elles ne se composent pas : les chemins gardent leurs carreaux.
  std::optional<GroundComposeSources> compose;
  std::size_t first_composed_page = 0;
  // LES TACHES RECOMPOSÉES (`ComposePatchTexture`), par rang dans
  // `BiomeLook::textures` : le rang, dans `texture_paths`, de la version sans
  // quart de tour, les trois autres à sa suite. Rien : la tache telle quelle.
  std::vector<std::optional<std::size_t>> recomposed_patches;
  // LES MOTIFS BORDÉS (`BorderMotif`), par rang dans `texture_paths` : la part
  // de la texture que prend son bord sur chaque côté, nulle pour une texture
  // lue telle quelle.
  std::vector<float> motif_borders;
};

// LIT LES RESSOURCES DU BIOME `look` dans l'installation, derrière l'écran de
// chargement. Échoue si la carte de référence ne se lit pas ou si un nom ne
// s'écrit pas dans l'encodage de l'installation ; une texture ou un modèle
// absent se compte dans la scène, comme pour une carte.
[[nodiscard]] ro::core::Result<GroundSources> OpenGroundSources(
    ro::vfs::Vfs& vfs, const ro::sim::TerrainRule& rule, std::uint64_t run_seed,
    const BiomeLook& look, ro::core::LoadProgress* progress = nullptr);

// LA FENÊTRE `origin`, nommée `name`, bâtie sans fichier : plan, GND, GAT, RSW,
// scène et part pure du téléversement (`PrepareSceneUpload`), grille et lumière
// des acteurs. Le VFS n'est passé qu'à l'assemblage de la scène, qui y trouve
// tout dans `sources.library`. Le sol se maille en `ground_subdivisions`
// sous-carreaux par côté de cube : arrondi d'emblée quand le réglage le
// demande, plutôt qu'à la première image.
[[nodiscard]] ro::core::Result<MapStage> BuildGroundWindow(
    ro::vfs::Vfs& vfs, const GroundSources& sources, GroundWindowOrigin origin, std::string name,
    std::int32_t ground_subdivisions = ro::render::kFlatGroundSubdivisions);

// Les mêmes, depuis un plan déjà fait : ce que le banc mesure à part.
[[nodiscard]] ro::core::Result<MapStage> AssembleGroundWindow(
    ro::vfs::Vfs& vfs, const GroundSources& sources, const GroundPlan& plan, std::string name,
    std::int32_t ground_subdivisions = ro::render::kFlatGroundSubdivisions);

}  // namespace moonclient

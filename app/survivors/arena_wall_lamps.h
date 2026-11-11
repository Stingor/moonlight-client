#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "ro/sim/survivors_terrain.h"

#include "survivors/arena_ground_look.h"
#include "survivors/arena_ground_plan.h"

// LES LAMPES MURALES D'UN BIOME EN PANNE (`ArenaDarkness::kBlackout`) : un
// modèle posé au pied d'un mur de salle, contre la face qui borde le sol,
// tourné vers lui, à intervalle régulier, et qui porte une lumière ponctuelle
// (`arena_blackout.h`).
//
// Tout se lit sur le CUBE DU MONDE et la graine de la partie, comme le semis :
// deux fenêtres qui se recouvrent posent les mêmes lampes, et le plan du sol
// comme la scène les retrouvent sans se parler.
namespace moonclient {

// UN MODÈLE DE LAMPE, et sa part parmi les autres. Une lampe de sol se pose
// au sol ; une applique monte de `lift_world` le long de la face. Sa lumière
// luit à `light_height_world` au-dessus du sol. En unités du monde.
struct WallLampLook {
  std::string_view model;
  float scale = 1.0F;
  std::uint32_t weight = 1;
  float lift_world = 0.0F;
  float light_height_world = 0.0F;
};

// LA FACE DU MUR contre laquelle se pose une lampe : celle qui regarde ce
// point cardinal.
enum class WallFace : std::uint8_t { kSouth, kNorth, kEast, kWest };

// Les lampes d'un biome en panne ; aucune pour un biome dont le ciel tombe.
[[nodiscard]] std::span<const WallLampLook> WallLampLooksOf(const BiomeLook& biome) noexcept;

// UN CUBE DE MUR PORTE UNE LAMPE quand il borde le sol par un côté et que
// son rang le long de ce côté tombe sur la trame : un cube sur tant, à partir
// d'un décalage que tire la graine. Un mur qui longe le sol vers le nord ou le
// sud se trame d'ouest en est, un mur qui le longe vers l'est ou l'ouest du
// sud au nord. Un cube qui borde le sol de plusieurs côtés prend la face
// sud, puis nord, puis est, puis ouest.
inline constexpr std::int32_t kWallLampSpacingCubes = 4;
// L'écart entre la face du mur et le point où se pose la lampe, en cases.
inline constexpr float kWallLampGapCells = 0.25F;

// UNE LAMPE POSÉE.
struct WallLamp {
  // Le cube de mur qui la porte, en cubes du monde, et la face qu'elle borde.
  std::int32_t cube_x = 0;
  std::int32_t cube_y = 0;
  WallFace face = WallFace::kSouth;
  // Son modèle, rang dans `WallLampLooksOf`.
  std::uint8_t look = 0;
  // SON RANG DANS LA PANNE, dans [0, 1) : elle s'éteint dès que la part de
  // lampes éteintes le dépasse (`ArenaLampsOutShare`). Tiré par lampe : la
  // panne gagne la salle au hasard, et une lampe éteinte le reste.
  float blackout_rank = 0.0F;
  float yaw_degrees = 0.0F;
  [[nodiscard]] bool operator==(const WallLamp&) const = default;
};

// Là où se pose la lampe, en cases du monde : au milieu de la face, à
// `kWallLampGapCells` d'elle, sur le sol.
[[nodiscard]] glm::vec2 WallLampCell(const WallLamp& lamp) noexcept;

// LES LAMPES DES MURS de chaque morceau, lues une fois. Le mur d'un morceau
// borde aussi le sol de ses voisins : leurs murs se lisent au besoin, par
// génération du morceau, ou par ceux que l'appelant a déjà générés
// (`Remember`). Le résultat ne dépend pas de l'ordre des lectures.
class WallLampField {
 public:
  WallLampField(const ro::sim::TerrainRule& rule, std::uint64_t run_seed, const BiomeLook& biome);

  // Retient les murs d'un morceau déjà généré.
  void Remember(const ro::sim::TerrainChunk& chunk);

  // Les lampes des cubes de mur du morceau (chunk_x, chunk_y), d'ouest en est
  // puis du sud au nord ; aucune hors d'un biome en panne. La vue vaut
  // jusqu'au prochain appel.
  [[nodiscard]] std::span<const WallLamp> LampsOfChunk(std::int32_t chunk_x, std::int32_t chunk_y);

  // Les lampes de la fenêtre `origin`, morceau par morceau, rangées dans `out`
  // (vidé).
  void LampsOfWindow(GroundWindowOrigin origin, std::vector<WallLamp>& out);

  // Vrai pour un biome en panne, qui porte des lampes.
  [[nodiscard]] bool has_lamps() const noexcept { return !looks_.empty(); }
  // Les modèles du biome, dans l'ordre de `WallLamp::look`.
  [[nodiscard]] std::span<const WallLampLook> looks() const noexcept { return looks_; }

 private:
  using ChunkKey = std::pair<std::int32_t, std::int32_t>;
  using ChunkWalls = std::array<std::uint8_t, ro::sim::kTerrainChunkArea>;

  [[nodiscard]] const ChunkWalls& WallsOf(std::int32_t chunk_x, std::int32_t chunk_y);
  [[nodiscard]] bool WallCube(std::int32_t cube_x, std::int32_t cube_y);
  [[nodiscard]] std::vector<WallLamp> PlanChunk(std::int32_t chunk_x, std::int32_t chunk_y);
  void Trim();

  const ro::sim::TerrainRule* rule_ = nullptr;
  std::uint64_t run_seed_ = 0;
  std::span<const WallLampLook> looks_;
  std::int32_t phase_cubes_ = 0;
  std::map<ChunkKey, ChunkWalls> walls_;
  std::map<ChunkKey, std::vector<WallLamp>> lamps_;
};

// LES POSES DES LAMPES de la fenêtre `origin`, ajoutées à `poses` comme un
// semis : rien ne bloque.
void AppendWallLampPoses(WallLampField& field, GroundWindowOrigin origin,
                         std::vector<PlannedPose>& poses);

}  // namespace moonclient

// LE PLAN DU SOL GÉNÉRÉ DESSINÉ : il montre ce que `sim` décide, sans rien y
// ajouter qui bloque — chaque case bloquée sous une pièce dessinée, chaque
// modèle sur son emprise, un fourré sur des cases praticables, aucun semis
// sur une case bloquée ou couverte —, et ce qu'il
// dessine ne dépend que de la case du monde : même graine, même plan ; deux
// fenêtres qui se recouvrent, mêmes cubes.

#include "survivors/arena_ground_plan.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/rsw.h"
#include "ro/render/model_mesh.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors/arena_ground_look.h"
#include "survivors/arena_ground_paths.h"

using moonclient::BiomeLook;
using moonclient::GroundPlan;
using moonclient::GroundPlanLight;
using moonclient::GroundTextureRole;
using moonclient::GroundWindowAround;
using moonclient::GroundWindowOrigin;
using moonclient::kGroundCellsPerCube;
using moonclient::kGroundWindowCells;
using moonclient::kGroundWindowCubes;
using moonclient::kSeedPose;
using moonclient::PieceModelYawDegrees;
using moonclient::PlanGroundWindow;
using moonclient::PlannedCube;
using moonclient::PlannedPiece;
using moonclient::PlannedPose;

namespace {

constexpr std::uint64_t kSeed = 20261084;
constexpr std::uint64_t kOtherSeed = 7;
// Un soleil bas, au sud-est : l'ombre part vers le nord-ouest.
const GroundPlanLight kLowSun{.toward_sun = glm::normalize(glm::vec3(0.6F, 0.5F, 0.6F))};
// Un point sur le bord d'une case touche les deux : on le cherche à cette
// distance de part et d'autre.
constexpr float kEdgeProbe = 1.0e-3F;
// Une fenêtre loin du départ, et une qui recouvre celle du départ d'un et deux
// morceaux.
constexpr std::int32_t kFarCells = 4096;
constexpr std::int32_t kOverlapEastCells = ro::sim::kTerrainChunkCells;
constexpr std::int32_t kOverlapNorthCells = 2 * ro::sim::kTerrainChunkCells;
// Un point du monde à 32 768 cases ne se compte en flottant qu'à quelques
// millièmes près : la même place, vue de deux fenêtres, s'arrondit autrement.
constexpr float kWorldPointSlack = 0.01F;

// La règle de terrain livrée et son aspect.
struct Plains {
  const ro::sim::TerrainRule* rule = nullptr;
  const BiomeLook* look = nullptr;
};

Plains ShippedPlains() {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  Plains plains;
  for (const ro::sim::ArenaRule& arena : rules.arenas) {
    if (arena.terrain_id == 0 || plains.rule != nullptr) continue;
    plains.rule = ro::sim::FindTerrain(rules, arena.terrain_id);
  }
  REQUIRE(plains.rule != nullptr);
  plains.look = moonclient::FindBiomeLook(plains.rule->key);
  REQUIRE(plains.look != nullptr);
  return plains;
}

GroundWindowOrigin StartWindow() {
  return GroundWindowAround(ro::sim::kTerrainStartCell, ro::sim::kTerrainStartCell);
}

GroundPlan Plan(const Plains& plains, std::uint64_t seed, GroundWindowOrigin origin) {
  return PlanGroundWindow(*plains.rule, seed, *plains.look, origin, kLowSun);
}

bool PassableAt(const GroundPlan& plan, std::int32_t x, std::int32_t y) {
  return plan.passable[static_cast<std::size_t>(y) * kGroundWindowCells +
                       static_cast<std::size_t>(x)] != 0;
}

bool OnPathAt(const GroundPlan& plan, std::int32_t x, std::int32_t y) {
  return plan.path[static_cast<std::size_t>(y) * kGroundWindowCells +
                   static_cast<std::size_t>(x)] != 0;
}

// Vrai si l'emprise de `piece` couvre la case (x, y) de la fenêtre.
bool PieceCovers(const PlannedPiece& piece, GroundWindowOrigin origin, std::int32_t x,
                 std::int32_t y) {
  return piece.rule != nullptr && ro::sim::FootprintCovers(*piece.rule, piece.placed.quarter,
                                                           origin.x_cells + x - piece.placed.x,
                                                           origin.y_cells + y - piece.placed.y);
}

// Les cases que le point touche : une, deux sur un bord, quatre sur un coin.
std::set<std::pair<std::int32_t, std::int32_t>> CellsTouching(float east, float north) {
  std::set<std::pair<std::int32_t, std::int32_t>> cells;
  for (const float dx : {-kEdgeProbe, kEdgeProbe}) {
    for (const float dy : {-kEdgeProbe, kEdgeProbe}) {
      cells.emplace(static_cast<std::int32_t>(std::floor(east + dx)),
                    static_cast<std::int32_t>(std::floor(north + dy)));
    }
  }
  return cells;
}

void CheckDrawsSim(const GroundPlan& plan) {
  REQUIRE(plan.passable.size() ==
          static_cast<std::size_t>(kGroundWindowCells) * kGroundWindowCells);
  // Chaque pièce posée a un dessin et au moins un modèle.
  std::vector<std::size_t> poses_of(plan.pieces.size(), 0);
  for (const PlannedPose& pose : plan.poses) {
    if (pose.piece != kSeedPose) ++poses_of.at(pose.piece);
  }
  for (std::size_t rank = 0; rank < plan.pieces.size(); ++rank) {
    CHECK(plan.pieces[rank].look != nullptr);
    CHECK(poses_of[rank] > 0);
  }
  // Chaque case bloquée est sous l'emprise d'une pièce posée qui n'est pas
  // franchissable ; chaque case couverte par une pièce franchissable est
  // praticable.
  std::size_t blocked = 0;
  for (std::int32_t y = 0; y < kGroundWindowCells; ++y) {
    for (std::int32_t x = 0; x < kGroundWindowCells; ++x) {
      if (PassableAt(plan, x, y)) continue;
      ++blocked;
      bool covered = false;
      for (const PlannedPiece& piece : plan.pieces) {
        covered = covered || (PieceCovers(piece, plan.origin, x, y) && !piece.rule->walkable);
      }
      CHECK(covered);
    }
  }
  CHECK(blocked > 0);
  for (const PlannedPiece& piece : plan.pieces) {
    if (piece.rule == nullptr || !piece.rule->walkable) continue;
    for (std::int32_t y = 0; y < kGroundWindowCells; ++y) {
      for (std::int32_t x = 0; x < kGroundWindowCells; ++x) {
        if (PieceCovers(piece, plan.origin, x, y)) CHECK(PassableAt(plan, x, y));
      }
    }
  }
  // Un modèle de pièce se pose sur son emprise : chaque case que touche son
  // pied est couverte par sa pièce. Un semis ne touche que des cases libres.
  for (const PlannedPose& pose : plan.poses) {
    for (const auto& [x, y] : CellsTouching(pose.east, pose.north)) {
      REQUIRE(x >= 0);
      REQUIRE(y >= 0);
      REQUIRE(x < kGroundWindowCells);
      REQUIRE(y < kGroundWindowCells);
      // Aucun modèle, de pièce ou de semis, sur un chemin.
      CHECK_FALSE(OnPathAt(plan, x, y));
      if (pose.piece == kSeedPose) {
        CHECK(PassableAt(plan, x, y));
      } else {
        CHECK(PieceCovers(plan.pieces.at(pose.piece), plan.origin, x, y));
      }
    }
  }
  // Une case de chemin est praticable, et aucune pièce ordinaire n'en touche
  // le rectangle d'emprise.
  REQUIRE(plan.path.size() == plan.passable.size());
  for (std::size_t cell = 0; cell < plan.path.size(); ++cell) {
    if (plan.path[cell] != 0) CHECK(plan.passable[cell] != 0);
  }
  for (const PlannedPiece& piece : plan.pieces) {
    if (piece.rule == nullptr || piece.rule->landmark) continue;
    const ro::sim::FootprintSize size = ro::sim::FootprintSizeOf(*piece.rule, piece.placed.quarter);
    for (std::int32_t north = 0; north < size.height; ++north) {
      for (std::int32_t east = 0; east < size.width; ++east) {
        const std::int32_t x = piece.placed.x - plan.origin.x_cells + east;
        const std::int32_t y = piece.placed.y - plan.origin.y_cells + north;
        if (x < 0 || y < 0 || x >= kGroundWindowCells || y >= kGroundWindowCells) continue;
        CHECK_FALSE(OnPathAt(plan, x, y));
      }
    }
  }
}

}  // namespace

TEST_CASE("Sol dessiné : chaque case bloquée sous une pièce, chaque modèle sur son emprise",
          "[app][survivors]") {
  const Plains plains = ShippedPlains();
  const GroundWindowOrigin start = StartWindow();
  CheckDrawsSim(Plan(plains, kSeed, start));
  CheckDrawsSim(Plan(plains, kOtherSeed,
                     GroundWindowOrigin{.x_cells = start.x_cells + kFarCells,
                                        .y_cells = start.y_cells - kFarCells}));
}

TEST_CASE("Sol dessiné : un fourré se dessine sur des cases que l'on traverse",
          "[app][survivors]") {
  const Plains plains = ShippedPlains();
  const GroundPlan plan = Plan(plains, kSeed, StartWindow());
  std::size_t thickets = 0;
  for (std::size_t rank = 0; rank < plan.pieces.size(); ++rank) {
    const PlannedPiece& piece = plan.pieces[rank];
    if (piece.rule == nullptr || std::string_view(piece.rule->key) != "thicket") continue;
    CHECK(piece.rule->walkable);
    // Ses buissons sont dessinés, sur ses cases, qui restent praticables.
    std::size_t poses = 0;
    for (const PlannedPose& pose : plan.poses) {
      if (pose.piece != rank) continue;
      ++poses;
      for (const auto& [x, y] : CellsTouching(pose.east, pose.north)) {
        CHECK(PieceCovers(piece, plan.origin, x, y));
        CHECK(PassableAt(plan, x, y));
      }
    }
    CHECK(poses > 0);
    ++thickets;
  }
  CHECK(thickets > 0);
}

TEST_CASE("Sol dessiné : aucun semis ne tombe sur une case bloquée ou couverte ni ne la frôle",
          "[app][survivors]") {
  const Plains plains = ShippedPlains();
  const GroundPlan plan = Plan(plains, kSeed, StartWindow());
  std::size_t seeds = 0;
  for (const PlannedPose& pose : plan.poses) {
    if (pose.piece != kSeedPose) continue;
    ++seeds;
    const auto x = static_cast<std::int32_t>(std::floor(pose.east));
    const auto y = static_cast<std::int32_t>(std::floor(pose.north));
    for (std::int32_t dy = -1; dy <= 1; ++dy) {
      for (std::int32_t dx = -1; dx <= 1; ++dx) {
        if (x + dx < 0 || y + dy < 0 || x + dx >= kGroundWindowCells ||
            y + dy >= kGroundWindowCells) {
          continue;
        }
        CHECK(PassableAt(plan, x + dx, y + dy));
        for (const PlannedPiece& piece : plan.pieces) {
          CHECK_FALSE(PieceCovers(piece, plan.origin, x + dx, y + dy));
        }
      }
    }
  }
  CHECK(seeds > 0);
}

TEST_CASE("Sol dessiné : même graine, même plan ; autre graine, autre sol", "[app][survivors]") {
  const Plains plains = ShippedPlains();
  const GroundPlan first = Plan(plains, kSeed, StartWindow());
  const GroundPlan again = Plan(plains, kSeed, StartWindow());
  CHECK(first.cubes == again.cubes);
  CHECK(first.passable == again.passable);
  CHECK(first.cell_heights == again.cell_heights);
  const bool same_shades = first.lightmaps == again.lightmaps;
  CHECK(same_shades);
  CHECK(first.poses == again.poses);
  REQUIRE(first.pieces.size() == again.pieces.size());
  for (std::size_t rank = 0; rank < first.pieces.size(); ++rank) {
    CHECK(first.pieces[rank].placed == again.pieces[rank].placed);
  }
  // Une ombre cuite au moins, et un sol qui ondule.
  CHECK(first.lightmaps.size() > 1);
  CHECK(first.cubes.front().corner_height != first.cubes.back().corner_height);

  const GroundPlan other = Plan(plains, kOtherSeed, StartWindow());
  CHECK(other.passable != first.passable);
  CHECK(other.cubes != first.cubes);
}

TEST_CASE("Sol dessiné : un modèle à angle imposé tourne comme l'emprise de sa pièce",
          "[app][survivors]") {
  // UN MODÈLE ASYMÉTRIQUE : un point de son maillage pointe vers l'est du
  // fichier. Posé par la matrice du décor, il doit tourner d'un quart dans le
  // sens trigonométrique vu du ciel par quart de tour de la pièce, comme
  // l'emprise : l'est vers le nord, le nord vers l'ouest.
  constexpr std::int32_t kFirstFamilyRsm = 0x0104;
  constexpr float kSlotYawDegrees = 30.0F;
  constexpr float kDirectionSlack = 1.0e-5F;
  constexpr float kFullTurnDegrees = 360.0F;
  const ro::render::ModelMesh mesh;
  const glm::vec4 tip{1.0F, 0.0F, 0.0F, 0.0F};
  // Le point posé, en (est, nord) : le nord va vers les z décroissants.
  const auto posed = [&](std::uint8_t quarter) {
    ro::formats::RswModel placement;
    placement.rotation = {0.0F, PieceModelYawDegrees(quarter, kSlotYawDegrees), 0.0F};
    const glm::vec4 moved =
        ro::render::ModelInstanceMatrix(placement, 0, 0, mesh, kFirstFamilyRsm) * tip;
    return glm::vec2(moved.x, -moved.z);
  };
  glm::vec2 expected = posed(0);
  for (std::uint8_t quarter = 1; quarter < ro::sim::kTerrainQuarterTurns; ++quarter) {
    // Le quart de tour de l'emprise : (x, y) → (H − y, x), soit une direction
    // (est, nord) → (−nord, est).
    expected = glm::vec2(-expected.y, expected.x);
    const glm::vec2 got = posed(quarter);
    CHECK(std::abs(got.x - expected.x) < kDirectionSlack);
    CHECK(std::abs(got.y - expected.y) < kDirectionSlack);
    const float yaw = PieceModelYawDegrees(quarter, kSlotYawDegrees);
    CHECK(yaw >= 0.0F);
    CHECK(yaw < kFullTurnDegrees);
  }
}

TEST_CASE("Sol dessiné : deux fenêtres qui se recouvrent dessinent les mêmes cubes",
          "[app][survivors]") {
  const Plains plains = ShippedPlains();
  const GroundWindowOrigin start = StartWindow();
  const GroundWindowOrigin moved{.x_cells = start.x_cells + kOverlapEastCells,
                                 .y_cells = start.y_cells + kOverlapNorthCells};
  const GroundPlan left = Plan(plains, kSeed, start);
  const GroundPlan right = Plan(plains, kSeed, moved);
  const std::int32_t shift_i = kOverlapEastCells / kGroundCellsPerCube;
  const std::int32_t shift_j = kOverlapNorthCells / kGroundCellsPerCube;
  std::size_t compared = 0;
  for (std::int32_t j = shift_j; j < kGroundWindowCubes; ++j) {
    for (std::int32_t i = shift_i; i < kGroundWindowCubes; ++i) {
      const auto& a = left.cubes[static_cast<std::size_t>(j) * kGroundWindowCubes +
                                 static_cast<std::size_t>(i)];
      const auto& b = right.cubes[static_cast<std::size_t>(j - shift_j) * kGroundWindowCubes +
                                  static_cast<std::size_t>(i - shift_i)];
      // Le cube entier, quart de tour du motif compris, sauf le rang de sa
      // lightmap, qui dépend de la fenêtre : l'ombre se compare par son contenu.
      CHECK(a.texture == b.texture);
      CHECK(a.quarter_east == b.quarter_east);
      CHECK(a.quarter_north == b.quarter_north);
      CHECK(a.turn == b.turn);
      PlannedCube b_ranked_as_a = b;
      b_ranked_as_a.lightmap = a.lightmap;
      const bool same_cube = a == b_ranked_as_a;
      CHECK(same_cube);
      const bool same_shade = left.lightmaps.at(a.lightmap) == right.lightmaps.at(b.lightmap);
      CHECK(same_shade);
      ++compared;
    }
  }
  CHECK(compared > 0);
  // Les mêmes modèles aux mêmes places du monde.
  const auto world_poses = [](const GroundPlan& plan, std::int32_t from_x, std::int32_t from_y) {
    std::vector<PlannedPose> kept;
    for (PlannedPose pose : plan.poses) {
      pose.east += static_cast<float>(plan.origin.x_cells);
      pose.north += static_cast<float>(plan.origin.y_cells);
      pose.piece = pose.piece == kSeedPose ? kSeedPose : 0;
      if (pose.east >= static_cast<float>(from_x) && pose.north >= static_cast<float>(from_y) &&
          pose.east < static_cast<float>(from_x + kGroundWindowCells - kOverlapEastCells) &&
          pose.north < static_cast<float>(from_y + kGroundWindowCells - kOverlapNorthCells)) {
        kept.push_back(pose);
      }
    }
    return kept;
  };
  const std::vector<PlannedPose> shared_left = world_poses(left, moved.x_cells, moved.y_cells);
  const std::vector<PlannedPose> shared_right = world_poses(right, moved.x_cells, moved.y_cells);
  CHECK_FALSE(shared_left.empty());
  REQUIRE(shared_left.size() == shared_right.size());
  for (std::size_t rank = 0; rank < shared_left.size(); ++rank) {
    const PlannedPose& a = shared_left[rank];
    const PlannedPose& b = shared_right[rank];
    CHECK(a.model == b.model);
    CHECK(a.piece == b.piece);
    CHECK(std::abs(a.east - b.east) < kWorldPointSlack);
    CHECK(std::abs(a.north - b.north) < kWorldPointSlack);
    CHECK(a.yaw_degrees == b.yaw_degrees);
    CHECK(a.scale == b.scale);
  }
  // Les mêmes chemins.
  for (std::int32_t y = kOverlapNorthCells; y < kGroundWindowCells; ++y) {
    for (std::int32_t x = kOverlapEastCells; x < kGroundWindowCells; ++x) {
      CHECK(OnPathAt(left, x, y) == OnPathAt(right, x - kOverlapEastCells, y - kOverlapNorthCells));
    }
  }
}

TEST_CASE("Sol dessiné : les chemins se peignent de terre, bordés d'un semis", "[app][survivors]") {
  const Plains plains = ShippedPlains();
  const GroundPlan plan = Plan(plains, kSeed, StartWindow());
  const auto role_of = [&plains](const PlannedCube& cube) {
    return plains.look->textures[cube.texture].role;
  };
  std::size_t path_cells = 0;
  std::size_t dirt_cubes = 0;
  for (std::int32_t j = 0; j < kGroundWindowCubes; ++j) {
    for (std::int32_t i = 0; i < kGroundWindowCubes; ++i) {
      const PlannedCube& cube = plan.cubes[static_cast<std::size_t>(j) * kGroundWindowCubes +
                                           static_cast<std::size_t>(i)];
      std::int32_t on_path = 0;
      for (std::int32_t dy = 0; dy < kGroundCellsPerCube; ++dy) {
        for (std::int32_t dx = 0; dx < kGroundCellsPerCube; ++dx) {
          on_path +=
              OnPathAt(plan, i * kGroundCellsPerCube + dx, j * kGroundCellsPerCube + dy) ? 1 : 0;
        }
      }
      path_cells += static_cast<std::size_t>(on_path);
      // Un cube tout entier de chemin a ses quatre coins de terre ou presque :
      // jamais l'herbe ni une tache.
      const GroundTextureRole role = role_of(cube);
      if (on_path == kGroundCellsPerCube * kGroundCellsPerCube) {
        CHECK(role != GroundTextureRole::kGrass);
        CHECK(role != GroundTextureRole::kPatch);
        CHECK(role != GroundTextureRole::kScatter);
      }
      if (role == GroundTextureRole::kPathFull) ++dirt_cubes;
    }
  }
  CHECK(path_cells > 0);
  CHECK(dirt_cubes > 0);
  // LE SEMIS DE BORDURE se distingue du semis ordinaire, qui peut lui aussi
  // tomber près d'un chemin : le même biome sans bordure rend les mêmes poses,
  // et la bordure n'en ajoute qu'après elles, chacune sur une case libre, hors
  // chemin, qu'un chemin touche par un côté et qu'aucune case bloquée ne touche.
  BiomeLook bare = *plains.look;
  bare.verge_per_mille = 0;
  const GroundPlan without_verges =
      PlanGroundWindow(*plains.rule, kSeed, bare, StartWindow(), kLowSun);
  REQUIRE(plan.poses.size() > without_verges.poses.size());
  for (std::size_t rank = 0; rank < without_verges.poses.size(); ++rank) {
    CHECK(plan.poses[rank] == without_verges.poses[rank]);
  }
  const auto in_window = [](std::int32_t x, std::int32_t y) {
    return x >= 0 && y >= 0 && x < kGroundWindowCells && y < kGroundWindowCells;
  };
  for (std::size_t rank = without_verges.poses.size(); rank < plan.poses.size(); ++rank) {
    const PlannedPose& pose = plan.poses[rank];
    CHECK(pose.piece == kSeedPose);
    const auto x = static_cast<std::int32_t>(std::floor(pose.east));
    const auto y = static_cast<std::int32_t>(std::floor(pose.north));
    REQUIRE(in_window(x, y));
    CHECK(PassableAt(plan, x, y));
    CHECK_FALSE(OnPathAt(plan, x, y));
    // Au bord de la fenêtre, le plan a lu le tour de morceaux, que l'essai ne
    // voit pas : on n'y juge que les cases visibles.
    bool beside_path = false;
    for (const auto& [dx, dy] :
         {std::pair{-1, 0}, std::pair{1, 0}, std::pair{0, -1}, std::pair{0, 1}}) {
      if (!in_window(x + dx, y + dy)) {
        beside_path = true;
        continue;
      }
      beside_path = beside_path || OnPathAt(plan, x + dx, y + dy);
    }
    CHECK(beside_path);
    for (std::int32_t dy = -1; dy <= 1; ++dy) {
      for (std::int32_t dx = -1; dx <= 1; ++dx) {
        if (in_window(x + dx, y + dy)) CHECK(PassableAt(plan, x + dx, y + dy));
      }
    }
  }
}

TEST_CASE("Sol dessiné : un carreau de chemin pose ses coins de terre sur ceux du cube",
          "[app][survivors]") {
  using moonclient::kDirtCorners;
  using moonclient::PathTile;
  // Les coins de terre de chaque carreau à son quart de tour nul.
  const auto base_corners = [](GroundTextureRole role) -> std::uint8_t {
    switch (role) {
      case GroundTextureRole::kPathEdge:
        return moonclient::kDirtSouthWest | moonclient::kDirtNorthWest;
      case GroundTextureRole::kPathOuter:
        return moonclient::kDirtNorthWest;
      case GroundTextureRole::kPathInner:
        return moonclient::kDirtSouthEast | moonclient::kDirtNorthWest | moonclient::kDirtNorthEast;
      default:
        return kDirtCorners;
    }
  };
  CHECK(moonclient::PathTileOf(0) == PathTile{});
  for (std::uint8_t corners = 1; corners <= kDirtCorners; ++corners) {
    INFO("coins " << static_cast<int>(corners));
    const PathTile tile = moonclient::PathTileOf(corners);
    REQUIRE(tile.role != GroundTextureRole::kGrass);
    if (tile.role == GroundTextureRole::kPathFull) {
      // La terre pleine : quatre coins, ou deux opposés.
      CHECK((corners == kDirtCorners ||
             corners == (moonclient::kDirtSouthWest | moonclient::kDirtNorthEast) ||
             corners == (moonclient::kDirtSouthEast | moonclient::kDirtNorthWest)));
      continue;
    }
    std::uint8_t turned = base_corners(tile.role);
    for (std::uint8_t turn = 0; turn < tile.turn; ++turn) {
      turned = moonclient::TurnDirtCorners(turned);
    }
    CHECK(turned == corners);
  }
  // Un quart de tour, quatre fois, ramène les coins ; la place dans le motif
  // et le cube de texture se répondent sous chaque quart de tour.
  CHECK(moonclient::TurnDirtCorners(moonclient::TurnDirtCorners(moonclient::TurnDirtCorners(
            moonclient::TurnDirtCorners(moonclient::kDirtNorthWest)))) ==
        moonclient::kDirtNorthWest);
  for (std::uint8_t turn = 0; turn < ro::sim::kTerrainQuarterTurns; ++turn) {
    for (std::uint8_t u = 0; u < moonclient::kGroundTextureCubes; ++u) {
      for (std::uint8_t v = 0; v < moonclient::kGroundTextureCubes; ++v) {
        const moonclient::MotifCube place = moonclient::MotifCubeShowing({.u = u, .v = v}, turn);
        CHECK(moonclient::TextureCubeOf(place.quarter_east, place.quarter_north, turn) ==
              moonclient::TextureCube{.u = u, .v = v});
      }
    }
  }
}

TEST_CASE(
    "Sol dessiné : la place d'un repère est de terre, son repère au bord, vue de deux fenêtres",
    "[app][survivors]") {
  const Plains plains = ShippedPlains();
  // La place la plus proche du départ, cellule par cellule autour de la sienne.
  constexpr std::int64_t kSearchCells = 4;
  const std::int64_t start_cell =
      ro::sim::TerrainPathCellOf(*plains.rule, ro::sim::TerrainChunkOf(ro::sim::kTerrainStartCell));
  ro::sim::TerrainPlaza plaza;
  for (std::int64_t dy = -kSearchCells; dy <= kSearchCells && !plaza.exists; ++dy) {
    for (std::int64_t dx = -kSearchCells; dx <= kSearchCells && !plaza.exists; ++dx) {
      plaza = ro::sim::TerrainPathPlaza(*plains.rule, kSeed, start_cell + dx, start_cell + dy);
    }
  }
  REQUIRE(plaza.exists);
  const auto centre_x = static_cast<std::int32_t>(plaza.centre.x);
  const auto centre_y = static_cast<std::int32_t>(plaza.centre.y);
  const GroundWindowOrigin around = GroundWindowAround(centre_x, centre_y);
  const GroundWindowOrigin shifted{.x_cells = around.x_cells + kOverlapEastCells,
                                   .y_cells = around.y_cells + kOverlapNorthCells};
  constexpr std::int32_t kPlazaRadius = ro::sim::kTerrainPlazaCells / 2;
  // Le semis de bordure se cherche sur l'anneau qui entoure la place.
  constexpr std::int32_t kVergeRingCells = kPlazaRadius + 2;
  const std::array<GroundPlan, 2> plans{Plan(plains, kSeed, around), Plan(plains, kSeed, shifted)};
  for (const GroundPlan& plan : plans) {
    const GroundWindowOrigin origin = plan.origin;
    INFO("fenêtre (" << origin.x_cells << ", " << origin.y_cells << ")");
    CheckDrawsSim(plan);
    const std::int32_t x = centre_x - origin.x_cells;
    const std::int32_t y = centre_y - origin.y_cells;
    REQUIRE(x - kVergeRingCells >= 0);
    REQUIRE(y - kVergeRingCells >= 0);
    // La place entière est de terre praticable, et son centre se peint de
    // terre pleine.
    for (std::int32_t dy = -kPlazaRadius; dy <= kPlazaRadius; ++dy) {
      for (std::int32_t dx = -kPlazaRadius; dx <= kPlazaRadius; ++dx) {
        if (dx * dx + dy * dy > kPlazaRadius * kPlazaRadius) continue;
        CHECK(OnPathAt(plan, x + dx, y + dy));
        CHECK(PassableAt(plan, x + dx, y + dy));
      }
    }
    const PlannedCube& middle =
        plan.cubes[static_cast<std::size_t>(y / kGroundCellsPerCube) * kGroundWindowCubes +
                   static_cast<std::size_t>(x / kGroundCellsPerCube)];
    CHECK(plains.look->textures[middle.texture].role == GroundTextureRole::kPathFull);
    // Son repère est dessiné, au bord de la place, du côté de son site.
    std::size_t landmarks = 0;
    for (const PlannedPiece& piece : plan.pieces) {
      if (piece.rule == nullptr || !piece.rule->landmark) continue;
      if (ro::sim::TerrainChunkOf(piece.placed.x) != ro::sim::TerrainChunkOf(centre_x) ||
          ro::sim::TerrainChunkOf(piece.placed.y) != ro::sim::TerrainChunkOf(centre_y)) {
        continue;
      }
      ++landmarks;
      CHECK((piece.placed.x > centre_x) == plaza.site_east);
      CHECK((piece.placed.y > centre_y) == plaza.site_north);
    }
    CHECK(landmarks == 1);
    // Un semis de bordure autour de la place.
    std::size_t verges = 0;
    for (const PlannedPose& pose : plan.poses) {
      if (pose.piece != kSeedPose) continue;
      const float east = pose.east - static_cast<float>(x);
      const float north = pose.north - static_cast<float>(y);
      const auto ring = static_cast<float>(kVergeRingCells);
      if (east * east + north * north <= ring * ring) ++verges;
    }
    CHECK(verges > 0);
  }
  // Les deux fenêtres dessinent la place pareil : les cubes du carré qui
  // entoure son anneau, au cube près, et le semis posé sur cet anneau, ramené
  // en cases du monde.
  const auto cube_at = [](const GroundPlan& plan, std::int32_t x_cells, std::int32_t y_cells) {
    const std::int32_t i = (x_cells - plan.origin.x_cells) / kGroundCellsPerCube;
    const std::int32_t j = (y_cells - plan.origin.y_cells) / kGroundCellsPerCube;
    return plan
        .cubes[static_cast<std::size_t>(j) * kGroundWindowCubes + static_cast<std::size_t>(i)];
  };
  std::size_t compared = 0;
  for (std::int32_t y = centre_y - kVergeRingCells; y <= centre_y + kVergeRingCells;
       y += kGroundCellsPerCube) {
    for (std::int32_t x = centre_x - kVergeRingCells; x <= centre_x + kVergeRingCells;
         x += kGroundCellsPerCube) {
      INFO("case (" << x << ", " << y << ")");
      const PlannedCube a = cube_at(plans[0], x, y);
      PlannedCube b = cube_at(plans[1], x, y);
      // Le rang de la lightmap dépend de la fenêtre : l'ombre se compare par
      // son contenu.
      const bool same_shade =
          plans[0].lightmaps.at(a.lightmap) == plans[1].lightmaps.at(b.lightmap);
      CHECK(same_shade);
      b.lightmap = a.lightmap;
      const bool same_cube = a == b;
      CHECK(same_cube);
      ++compared;
    }
  }
  CHECK(compared > 0);
  const auto ring_seeds = [&](const GroundPlan& plan) {
    std::vector<PlannedPose> kept;
    for (PlannedPose pose : plan.poses) {
      if (pose.piece != kSeedPose) continue;
      pose.east += static_cast<float>(plan.origin.x_cells);
      pose.north += static_cast<float>(plan.origin.y_cells);
      const float east = pose.east - static_cast<float>(centre_x);
      const float north = pose.north - static_cast<float>(centre_y);
      const auto ring = static_cast<float>(kVergeRingCells);
      if (east * east + north * north <= ring * ring) kept.push_back(pose);
    }
    return kept;
  };
  const std::vector<PlannedPose> seeds_around = ring_seeds(plans[0]);
  const std::vector<PlannedPose> seeds_shifted = ring_seeds(plans[1]);
  CHECK_FALSE(seeds_around.empty());
  REQUIRE(seeds_around.size() == seeds_shifted.size());
  for (std::size_t rank = 0; rank < seeds_around.size(); ++rank) {
    const PlannedPose& a = seeds_around[rank];
    const PlannedPose& b = seeds_shifted[rank];
    CHECK(a.model == b.model);
    CHECK(std::abs(a.east - b.east) < kWorldPointSlack);
    CHECK(std::abs(a.north - b.north) < kWorldPointSlack);
    CHECK(a.yaw_degrees == b.yaw_degrees);
    CHECK(a.scale == b.scale);
  }
}

// LES CUVES QUI CÈDENT : une cuve rompue du laboratoire perd sa cloche de
// verre, sa machine reste debout ; une cuve intacte garde la sienne. Le
// masquage se lit au coin de la pièce en cases du monde : une fenêtre
// recentrée retrouve la même cloche. Aucune autre pièce, aucun autre biome
// n'a de pose qui se masque.

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/camera.h"
#include "ro/render/map_scene.h"
#include "ro/render/model_mesh.h"
#include "ro/render/model_veil.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors/arena_decor_veil.h"
#include "survivors/arena_ground_look.h"
#include "survivors/arena_ground_plan.h"
#include "survivors/arena_ground_window.h"
#include "world/map_stage.h"

using moonclient::BrokenPoseRanks;
using moonclient::GroundBreakablePose;
using moonclient::GroundBreakablePoses;
using moonclient::GroundPlan;
using moonclient::GroundWindowAround;
using moonclient::GroundWindowOrigin;
using moonclient::PlannedPose;

namespace {

constexpr std::uint64_t kSeed = 7;
constexpr std::string_view kLabKey = "lighthalzen_biolab";
constexpr std::string_view kTankKey = "tank";
// Un pas de recentrage : un morceau vers l'est.
constexpr std::int32_t kRecentreEastCells = ro::sim::kTerrainChunkCells;
// La fenêtre du décor synthétique : quatre poses, deux machines, deux cloches.
constexpr std::int32_t kViewportPixels = 64;
constexpr std::uint32_t kFrameMs = 16;
constexpr std::int32_t kGroundSideCubes = 2;

const moonclient::GroundPlanLight kLight{};

[[nodiscard]] const ro::sim::TerrainRule* TerrainOf(std::string_view key) {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  for (const ro::sim::ArenaRule& arena : rules.arenas) {
    if (arena.terrain_id == 0) continue;
    const ro::sim::TerrainRule* rule = ro::sim::FindTerrain(rules, arena.terrain_id);
    if (rule != nullptr && rule->key == key) return rule;
  }
  return nullptr;
}

[[nodiscard]] GroundPlan PlanAt(const ro::sim::TerrainRule& rule, GroundWindowOrigin origin) {
  const moonclient::BiomeLook* look = moonclient::FindBiomeLook(rule.key);
  REQUIRE(look != nullptr);
  return moonclient::PlanGroundWindow(rule, kSeed, *look, origin, kLight);
}

[[nodiscard]] GroundWindowOrigin StartWindow() {
  return GroundWindowAround(ro::sim::kTerrainStartCell, ro::sim::kTerrainStartCell);
}

using ModelFiles = std::map<std::string_view, std::string>;

// Les fichiers des modèles : chaque modèle du plan a le sien, sauf `missing`.
[[nodiscard]] ModelFiles FilesOf(const GroundPlan& plan, std::string_view missing = {}) {
  ModelFiles files;
  for (const PlannedPose& pose : plan.poses) {
    if (pose.model != missing) files.emplace(pose.model, std::string(pose.model));
  }
  return files;
}

[[nodiscard]] bool IsTank(const moonclient::PlannedPiece& piece) {
  return piece.rule != nullptr && piece.rule->key == kTankKey;
}

// Le rang, dans `plan.poses`, de la pose de la pièce `piece` qui se masque
// (`hides`), ou de celle qui reste.
[[nodiscard]] std::optional<std::size_t> PoseOfPiece(const GroundPlan& plan, std::size_t piece,
                                                     bool hides) {
  for (std::size_t rank = 0; rank < plan.poses.size(); ++rank) {
    if (plan.poses[rank].piece == piece && plan.poses[rank].hides_when_burst == hides) {
      return rank;
    }
  }
  return std::nullopt;
}

[[nodiscard]] ro::sim::CellPosition CornerOf(const moonclient::PlannedPiece& piece) {
  return ro::sim::CellPosition{.x = static_cast<std::uint16_t>(piece.placed.x),
                               .y = static_cast<std::uint16_t>(piece.placed.y)};
}

// UN DÉCOR SYNTHÉTIQUE sur un sol plat de deux cubes : la machine aux rangs 0
// et 2 du RSW, la cloche aux rangs 1 et 3.
[[nodiscard]] moonclient::MapStage TwoTanksStage() {
  ro::formats::GndParts parts{.width = kGroundSideCubes, .height = kGroundSideCubes};
  parts.cubes.resize(static_cast<std::size_t>(parts.width) *
                     static_cast<std::size_t>(parts.height));
  auto gnd = ro::formats::GndMap::Assemble(parts);
  REQUIRE(gnd.has_value());
  moonclient::MapStage stage{.name = "essai",
                             .scene = ro::render::MapScene{
                                 .gnd = std::move(*gnd),
                                 .rsw = ro::formats::RswScene::Assemble(
                                     ro::formats::RswSceneHeader{}, ro::formats::RswLight{}, {})}};
  ro::render::ModelInstances machine;
  machine.transforms.assign(2, glm::mat4(1.0F));
  machine.rsw_ranks = {0, 2};
  ro::render::ModelInstances glass;
  glass.transforms.assign(2, glm::mat4(1.0F));
  glass.rsw_ranks = {1, 3};
  stage.scene.models.push_back(std::move(machine));
  stage.scene.models.push_back(std::move(glass));
  return stage;
}

[[nodiscard]] std::uint8_t OpacityOf(const ro::render::ModelOpacities& opacities, std::size_t model,
                                     std::size_t slot) {
  if (model >= opacities.size() || slot >= opacities[model].size()) return ro::render::kVeilOpaque;
  return opacities[model][slot];
}

}  // namespace

TEST_CASE("Cuves rompues : la cloche se masque, la machine reste, l'intacte garde la sienne",
          "[app][survivors][tanks]") {
  const ro::sim::TerrainRule* lab = TerrainOf(kLabKey);
  REQUIRE(lab != nullptr);
  const GroundPlan plan = PlanAt(*lab, StartWindow());
  std::vector<std::size_t> tanks;
  for (std::size_t piece = 0; piece < plan.pieces.size(); ++piece) {
    if (IsTank(plan.pieces[piece])) tanks.push_back(piece);
  }
  REQUIRE(tanks.size() >= 2);
  // Chaque cuve : une machine qui reste, une cloche qui se masque.
  for (const std::size_t tank : tanks) {
    CHECK(PoseOfPiece(plan, tank, /*hides=*/false).has_value());
    CHECK(PoseOfPiece(plan, tank, /*hides=*/true).has_value());
  }

  const std::vector<GroundBreakablePose> breakables = GroundBreakablePoses(FilesOf(plan), plan);
  CHECK(breakables.size() == tanks.size());

  const moonclient::PlannedPiece& broken = plan.pieces[tanks.front()];
  const moonclient::PlannedPiece& intact = plan.pieces[tanks.back()];
  const std::vector<ro::sim::CellPosition> burst{CornerOf(broken)};
  std::vector<std::size_t> hidden{99};
  BrokenPoseRanks(breakables, burst, hidden);
  // Toutes les poses entrent dans le RSW : le rang est celui du plan.
  REQUIRE(hidden.size() == 1);
  CHECK(hidden.front() == PoseOfPiece(plan, tanks.front(), /*hides=*/true));
  CHECK(hidden.front() != PoseOfPiece(plan, tanks.front(), /*hides=*/false));
  CHECK(hidden.front() != PoseOfPiece(plan, tanks.back(), /*hides=*/true));
  CHECK(!(CornerOf(intact) == burst.front()));

  // Sans cuve rompue, rien ne se masque.
  BrokenPoseRanks(breakables, {}, hidden);
  CHECK(hidden.empty());
}

TEST_CASE("Cuves rompues : un modèle sans fichier décale les rangs du RSW comme le RSW",
          "[app][survivors][tanks]") {
  const ro::sim::TerrainRule* lab = TerrainOf(kLabKey);
  REQUIRE(lab != nullptr);
  const GroundPlan plan = PlanAt(*lab, StartWindow());
  // La machine n'a pas de fichier : elle n'entre pas dans le RSW, et chaque
  // cloche y prend le rang des poses qui y entrent avant elle.
  std::string_view machine;
  for (const PlannedPose& pose : plan.poses) {
    if (pose.piece < plan.pieces.size() && IsTank(plan.pieces[pose.piece]) &&
        !pose.hides_when_burst) {
      machine = pose.model;
    }
  }
  REQUIRE(!machine.empty());
  const std::vector<GroundBreakablePose> breakables =
      GroundBreakablePoses(FilesOf(plan, machine), plan);
  REQUIRE(!breakables.empty());
  for (const GroundBreakablePose& breakable : breakables) {
    std::size_t entered = 0;
    std::optional<std::size_t> found;
    for (const PlannedPose& pose : plan.poses) {
      if (pose.model == machine) continue;
      if (pose.hides_when_burst && plan.pieces[pose.piece].placed.x == breakable.piece_x_cells &&
          plan.pieces[pose.piece].placed.y == breakable.piece_y_cells) {
        found = entered;
      }
      ++entered;
    }
    CHECK(found == breakable.rsw_rank);
  }
}

TEST_CASE("Cuves rompues : le masquage survit au recentrage", "[app][survivors][tanks]") {
  const ro::sim::TerrainRule* lab = TerrainOf(kLabKey);
  REQUIRE(lab != nullptr);
  const GroundWindowOrigin start = StartWindow();
  const GroundWindowOrigin moved{.x_cells = start.x_cells + kRecentreEastCells,
                                 .y_cells = start.y_cells};
  const GroundPlan before = PlanAt(*lab, start);
  const GroundPlan after = PlanAt(*lab, moved);
  // Une cuve que les deux fenêtres montrent.
  std::optional<std::size_t> shared_before;
  std::optional<std::size_t> shared_after;
  for (std::size_t piece = 0; piece < before.pieces.size() && !shared_after; ++piece) {
    if (!IsTank(before.pieces[piece])) continue;
    for (std::size_t other = 0; other < after.pieces.size(); ++other) {
      if (after.pieces[other].placed == before.pieces[piece].placed) {
        shared_before = piece;
        shared_after = other;
        break;
      }
    }
  }
  REQUIRE(shared_after.has_value());
  const std::vector<ro::sim::CellPosition> burst{CornerOf(before.pieces[*shared_before])};
  std::vector<std::size_t> hidden;
  BrokenPoseRanks(GroundBreakablePoses(FilesOf(after), after), burst, hidden);
  REQUIRE(hidden.size() == 1);
  CHECK(hidden.front() == PoseOfPiece(after, *shared_after, /*hides=*/true));
}

TEST_CASE("Cuves rompues : les autres pièces et les autres biomes n'ont rien qui se masque",
          "[app][survivors][tanks]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  std::size_t biomes = 0;
  for (const ro::sim::ArenaRule& arena : rules.arenas) {
    if (arena.terrain_id == 0) continue;
    const ro::sim::TerrainRule* rule = ro::sim::FindTerrain(rules, arena.terrain_id);
    REQUIRE(rule != nullptr);
    if (moonclient::FindBiomeLook(rule->key) == nullptr) continue;
    ++biomes;
    const GroundPlan plan = PlanAt(*rule, StartWindow());
    for (const PlannedPose& pose : plan.poses) {
      if (!pose.hides_when_burst) continue;
      // Seule la cloche d'une cuve du laboratoire se masque.
      CHECK(rule->key == kLabKey);
      REQUIRE(pose.piece < plan.pieces.size());
      CHECK(IsTank(plan.pieces[pose.piece]));
    }
    if (rule->key != kLabKey) CHECK(GroundBreakablePoses(FilesOf(plan), plan).empty());
  }
  CHECK(biomes >= 2);
}

TEST_CASE("Cuves rompues : la pose masquée ne se peint ni ne porte d'ombre",
          "[app][survivors][tanks]") {
  const moonclient::MapStage stage = TwoTanksStage();
  const ro::render::Camera camera;
  moonclient::ArenaDecorVeil veil;
  const std::vector<std::size_t> hidden{3};
  moonclient::ArenaVeilFrame frame{.stage = &stage,
                                   .mount = 1,
                                   .camera = &camera,
                                   .width = kViewportPixels,
                                   .height = kViewportPixels,
                                   .now_ms = kFrameMs,
                                   .hidden_ranks = hidden};
  veil.Advance(frame);
  const moonclient::LitDecorVeil decor = veil.decor();
  REQUIRE(decor.opacities != nullptr);
  // La cloche du rang 3 (deuxième pose du modèle 1) : nulle ; le reste plein.
  CHECK(OpacityOf(*decor.opacities, 1, 1) == 0);
  CHECK(OpacityOf(*decor.opacities, 1, 0) == ro::render::kVeilOpaque);
  CHECK(OpacityOf(*decor.opacities, 0, 0) == ro::render::kVeilOpaque);
  CHECK(OpacityOf(*decor.opacities, 0, 1) == ro::render::kVeilOpaque);
  for (const ro::render::VeiledPose& pose : decor.veiled) {
    CHECK(!(pose.model_index == 1 && pose.index == 1));
  }
  REQUIRE(decor.casting != nullptr);
  CHECK(OpacityOf(*decor.casting, 1, 1) == 0);
  CHECK(OpacityOf(*decor.casting, 1, 0) == ro::render::kVeilOpaque);
  CHECK(OpacityOf(*decor.casting, 0, 1) == ro::render::kVeilOpaque);

  // UNE CUVE QUI CÈDE À L'IMAGE SUIVANTE perd sa cloche dans cette image.
  const std::vector<std::size_t> both{1, 3};
  frame.hidden_ranks = both;
  frame.now_ms += kFrameMs;
  veil.Advance(frame);
  CHECK(OpacityOf(*veil.decor().opacities, 1, 0) == 0);
  CHECK(OpacityOf(*veil.decor().opacities, 1, 1) == 0);

  // UNE FENÊTRE NEUVE, sans cuve rompue : tout redevient plein, rien n'ôte
  // d'ombre.
  frame.mount = 2;
  frame.hidden_ranks = {};
  frame.now_ms += kFrameMs;
  veil.Advance(frame);
  CHECK(veil.decor().casting == nullptr);
  if (veil.decor().opacities != nullptr) {
    CHECK(OpacityOf(*veil.decor().opacities, 1, 1) == ro::render::kVeilOpaque);
  }
}

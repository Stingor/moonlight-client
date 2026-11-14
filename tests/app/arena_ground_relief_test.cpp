// LE RELIEF DU SOL GÉNÉRÉ : des collines visuelles, fonction pure du point du
// monde et de la graine, que deux fenêtres qui se recouvrent rendent pareilles ;
// une pente bornée par la caméra (aucune crête ne cache un monstre) et une
// amplitude bornée par la fenêtre (aucun bord ne se voit) ; des modèles posés
// sur le sol, les repères sur une terrasse ; une lumière qui suit les normales.

#include "survivors/arena_ground_relief.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/ground_light.h"
#include "ro/render/terrain_mesh.h"
#include "ro/render/terrain_relief.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"
#include "ro/vfs/vfs.h"

#include "arena_ground_synthetic.h"
#include "support/temp_directory.h"
#include "survivors/arena_camera.h"
#include "survivors/arena_ground_follow.h"
#include "survivors/arena_ground_plan.h"
#include "survivors/arena_ground_window.h"

using moonclient::GroundPlan;
using moonclient::GroundPlanLight;
using moonclient::GroundReliefPad;
using moonclient::GroundWindowAround;
using moonclient::GroundWindowOrigin;
using moonclient::kGroundCellsPerCube;
using moonclient::kGroundReliefCellUnits;
using moonclient::kGroundReliefMostUnits;
using moonclient::kGroundSlopeCap;
using moonclient::kGroundWindowCells;
using moonclient::kGroundWindowCubes;

namespace {

constexpr std::uint64_t kSeed = 20261084;
constexpr std::uint64_t kOtherSeed = 7;
// La lumière de `prt_fild08` : ambiante 0,3, diffuse pleine, soleil au
// sud-ouest à 45°.
const GroundPlanLight kFieldLight{.toward_sun = glm::normalize(glm::vec3(-0.5F, 0.707F, 0.5F)),
                                  .ambient = glm::vec3(0.3F),
                                  .diffuse = glm::vec3(1.0F)};
// Le format le plus large que l'arène voit entier, et sa visée la plus rase.
constexpr float kWidestAspect = 21.0F / 9.0F;
constexpr double kWidestSightRadians = 0.7027;
constexpr double kAngleSlack = 1.0e-3;
// Le centre d'une case, depuis son coin.
constexpr double kHalf = 0.5;
// Le bruit à gradients : la grille de sa cellule, l'écart de sa dérivée lue
// par différence, et de part et d'autre d'un bord de cellule.
constexpr int kNoiseGridSteps = 16;
constexpr double kTightBoundShare = 0.99;
constexpr double kNoiseProbe = 1.0e-5;
constexpr double kNoisePeriod = 24.0;
constexpr double kNoiseEdge = 1.0e-7;
constexpr double kNoiseEdgeSlack = 1.0e-6;
// Les formats d'image éprouvés, du plus étroit au plus large.
constexpr std::array<float, 6> kAspects = {4.0F / 3.0F,  16.0F / 10.0F, 16.0F / 9.0F,
                                           21.0F / 9.0F, 32.0F / 10.0F, 32.0F / 9.0F};
// L'écart des flottants sur une hauteur ou une pente lue de deux façons.
constexpr double kHeightSlack = 1.0e-3;
constexpr double kSlopeSlack = 1.0e-3;
// La pente lue sur la grille des coins de case, contre la normale : la corde
// d'une case et la tangente au centre diffèrent de la courbure, la plus forte
// au genou d'une terrasse ; en moyenne, de très peu.
constexpr double kChordSlack = 0.05;
constexpr double kMeanChordSlack = 0.002;
// Une pente au plafond, à l'opposé du soleil, sous ceci de la lumière.
constexpr float kDarkSideShade = 0.7F;
// Le grand échantillon : des fenêtres éparses sur le monde, tous les tant de
// cases sur chaque axe, pour plusieurs graines ; et le pas de la vérification
// de la seule fonction.
constexpr std::int32_t kSampleWindowsPerSide = 3;
constexpr std::int32_t kSampleWindowSpacingCells = 1000;
constexpr std::array<std::uint64_t, 4> kSampleSeeds = {kSeed, kOtherSeed, 1, 0xDEADBEEFULL};
constexpr double kPureStepCells = 2.6;
constexpr double kPureSideCells = 1500.0;
// Un modèle se pose au plus bas du sol à une demi-case autour de son pied.
constexpr double kFootReachCells = 0.5;
// La part la plus pentue que les collines doivent atteindre : elles se voient.
constexpr double kLeastSteepShare = 0.5;
// Les sous-pas d'un cube où le sol arrondi se lit.
constexpr int kRoundedProbes = 8;
// Les cellules de chemins lues de part et d'autre du départ pour trouver deux
// repères voisins, et l'écart de leurs collines qui ferait tirer l'un sur
// l'autre : plus que le genou de la bride à un morceau d'écart.
constexpr std::int64_t kSearchPathCells = 20;
constexpr double kNeighbourLeastGapUnits = 8.0;

GroundWindowOrigin StartWindow() {
  return GroundWindowAround(ro::sim::kTerrainStartCell, ro::sim::kTerrainStartCell);
}

GroundPlan Plan(std::uint64_t seed, GroundWindowOrigin origin) {
  const ro::sim::TerrainRule& rule = moonclient::test_support::ShippedPlains();
  const moonclient::BiomeLook* look = moonclient::FindBiomeLook(rule.key);
  REQUIRE(look != nullptr);
  return moonclient::PlanGroundWindow(rule, seed, *look, origin, kFieldLight);
}

std::size_t CellIndex(std::int32_t x, std::int32_t y) {
  return static_cast<std::size_t>(y) * kGroundWindowCells + static_cast<std::size_t>(x);
}

// La terrasse du plan sous le point (east, north) de la fenêtre, s'il y en a une.
std::optional<GroundReliefPad> PadUnder(const GroundPlan& plan, float east, float north) {
  const double world_east = plan.origin.x_cells + static_cast<double>(east);
  const double world_north = plan.origin.y_cells + static_cast<double>(north);
  for (const GroundReliefPad& pad : plan.relief_pads) {
    if (world_east >= pad.west && world_east <= pad.east && world_north >= pad.south &&
        world_north <= pad.north) {
      return pad;
    }
  }
  return std::nullopt;
}

// CHAQUE TERRASSE EST PLATE : chaque coin de case de la fenêtre sous elle vaut
// sa hauteur, au bit près. Rend le nombre de cases lues.
std::size_t CheckPadsFlat(const GroundPlan& plan) {
  std::size_t flat_cells = 0;
  for (const GroundReliefPad& pad : plan.relief_pads) {
    for (std::int32_t y = 0; y < kGroundWindowCells; ++y) {
      for (std::int32_t x = 0; x < kGroundWindowCells; ++x) {
        const double west = plan.origin.x_cells + x;
        const double south = plan.origin.y_cells + y;
        if (west < pad.west || south < pad.south || west + 1.0 > pad.east ||
            south + 1.0 > pad.north) {
          continue;
        }
        INFO("terrasse (" << pad.west << ", " << pad.south << ") à " << pad.height << ", case ("
                          << x << ", " << y << ")");
        for (const float corner : plan.cell_heights[CellIndex(x, y)]) {
          CHECK(corner == static_cast<float>(pad.height));
        }
        ++flat_cells;
      }
    }
  }
  return flat_cells;
}

}  // namespace

TEST_CASE("Relief : une fonction pure du point du monde et de la graine, bornée",
          "[app][survivors]") {
  double highest = 0.0;
  for (double east = 0.0; east < kPureSideCells; east += kPureStepCells) {
    for (double north = 0.0; north < kPureSideCells; north += kPureStepCells) {
      const double world_east = ro::sim::kTerrainStartCell + east;
      const double world_north = ro::sim::kTerrainStartCell - north;
      const float height = moonclient::GroundReliefAt(kSeed, world_east, world_north);
      CHECK(height == moonclient::GroundReliefAt(kSeed, world_east, world_north));
      CHECK(std::abs(static_cast<double>(height)) <= kGroundReliefMostUnits);
      highest = std::max(highest, std::abs(static_cast<double>(height)));
    }
  }
  // Les collines prennent de l'ampleur : plus de la moitié de leur borne.
  CHECK(highest > kGroundReliefMostUnits / 2.0);
  CHECK(moonclient::GroundReliefAt(kSeed, 100.5, 200.5) !=
        moonclient::GroundReliefAt(kOtherSeed, 100.5, 200.5));
}

TEST_CASE("Relief : deux fenêtres qui se recouvrent ont les mêmes hauteurs et la même lumière",
          "[app][survivors]") {
  const GroundWindowOrigin start = StartWindow();
  const GroundWindowOrigin beside{.x_cells = start.x_cells + ro::sim::kTerrainChunkCells,
                                  .y_cells = start.y_cells + 2 * ro::sim::kTerrainChunkCells};
  const GroundPlan first = Plan(kSeed, start);
  const GroundPlan second = Plan(kSeed, beside);
  REQUIRE(first.cell_heights.size() == static_cast<std::size_t>(kGroundWindowCells) *
                                           static_cast<std::size_t>(kGroundWindowCells));
  REQUIRE(first.cell_shades.size() == first.cell_heights.size());
  std::size_t compared = 0;
  for (std::int32_t y = 0; y < kGroundWindowCells; ++y) {
    for (std::int32_t x = 0; x < kGroundWindowCells; ++x) {
      const std::int32_t other_x = x + start.x_cells - beside.x_cells;
      const std::int32_t other_y = y + start.y_cells - beside.y_cells;
      if (other_x < 0 || other_y < 0 || other_x >= kGroundWindowCells ||
          other_y >= kGroundWindowCells) {
        continue;
      }
      INFO("case (" << x << ", " << y << ")");
      CHECK(first.cell_heights[CellIndex(x, y)] ==
            second.cell_heights[CellIndex(other_x, other_y)]);
      CHECK(first.cell_shades[CellIndex(x, y)] == second.cell_shades[CellIndex(other_x, other_y)]);
      ++compared;
    }
  }
  CHECK(compared > 0);
  // Une terrasse au moins dans la fenêtre du départ, et un sol qui s'y aplanit.
  REQUIRE(!first.relief_pads.empty());
}

TEST_CASE("Relief : le plafond de pente garde la visée la plus rase de chaque format",
          "[app][survivors]") {
  for (const float aspect : kAspects) {
    const double sight = moonclient::ArenaShallowestSightTangent(aspect);
    INFO("format " << aspect << ", visée la plus rase " << sight);
    CHECK(kGroundSlopeCap <= moonclient::kGroundSightMarginShare * sight);
  }
  // Le 21:9 : un coin du haut plonge de 40,3° (0,703 radian).
  CHECK(std::atan(moonclient::ArenaShallowestSightTangent(kWidestAspect)) ==
        Catch::Approx(kWidestSightRadians).margin(kAngleSlack));
  // Le plafond par axe, dans toute direction, tient sous le plafond.
  CHECK(std::sqrt(2.0) * moonclient::kGroundAxisSlopeCap <= kGroundSlopeCap);
}

TEST_CASE("Relief : aucune pente au-delà du plafond, terrasses comprises, sur un grand échantillon",
          "[app][survivors]") {
  // La pente de la fonction au centre de chaque case, et celle des triangles
  // du sol d'origine, sur les cubes de chaque fenêtre.
  double steepest = 0.0;
  double steepest_chord = 0.0;
  double steepest_axis = 0.0;
  std::size_t probes = 0;
  std::size_t beyond = 0;
  std::size_t pads = 0;
  constexpr double kCubeUnits = kGroundCellsPerCube * kGroundReliefCellUnits;
  for (const std::uint64_t seed : kSampleSeeds) {
    for (std::int32_t row = 0; row < kSampleWindowsPerSide; ++row) {
      for (std::int32_t column = 0; column < kSampleWindowsPerSide; ++column) {
        const GroundWindowOrigin origin =
            GroundWindowAround(ro::sim::kTerrainStartCell + column * kSampleWindowSpacingCells,
                               ro::sim::kTerrainStartCell - row * kSampleWindowSpacingCells);
        const GroundPlan plan = Plan(seed, origin);
        pads += plan.relief_pads.size();
        CheckPadsFlat(plan);
        for (std::int32_t y = 0; y < kGroundWindowCells; ++y) {
          for (std::int32_t x = 0; x < kGroundWindowCells; ++x) {
            const glm::dvec2 slope = moonclient::GroundReliefSlopeAt(
                seed, origin.x_cells + x + kHalf, origin.y_cells + y + kHalf, plan.relief_pads);
            const double steep = glm::length(slope);
            steepest = std::max(steepest, steep);
            steepest_axis = std::max({steepest_axis, std::abs(slope.x), std::abs(slope.y)});
            beyond += steep > kGroundSlopeCap + kSlopeSlack ? 1U : 0U;
            ++probes;
          }
        }
        // Les deux façons de couper un cube en triangles : chaque triangle a
        // deux côtés sur les axes, dont la pente dit la sienne.
        for (const moonclient::PlannedCube& cube : plan.cubes) {
          const auto h = [&cube](std::size_t corner) {
            return static_cast<double>(cube.corner_height[corner]);
          };
          const double south = (h(1) - h(0)) / kCubeUnits;
          const double north = (h(3) - h(2)) / kCubeUnits;
          const double west = (h(2) - h(0)) / kCubeUnits;
          const double east = (h(3) - h(1)) / kCubeUnits;
          for (const double across : {south, north}) {
            for (const double along : {west, east}) {
              const double chord = std::hypot(across, along);
              steepest_chord = std::max(steepest_chord, chord);
              beyond += chord > kGroundSlopeCap + kSlopeSlack ? 1U : 0U;
            }
          }
        }
      }
    }
  }
  INFO(probes << " points, " << pads << " terrasses ; pente la plus raide " << steepest
              << ", sur un axe " << steepest_axis << ", corde la plus raide " << steepest_chord);
  CHECK(beyond == 0);
  CHECK(steepest_axis <= moonclient::kGroundAxisSlopeCap + kSlopeSlack);
  CHECK(pads > 0);
  // Les collines se voient : la pente approche du plafond.
  CHECK(steepest >= kLeastSteepShare * kGroundSlopeCap);
}

TEST_CASE("Relief : une terrasse est plate, et le sol la rejoint sous le plafond",
          "[app][survivors]") {
  const GroundPlan plan = Plan(kSeed, StartWindow());
  REQUIRE(!plan.relief_pads.empty());
  CHECK(CheckPadsFlat(plan) > 0);
  for (const GroundReliefPad& pad : plan.relief_pads) {
    CHECK(std::abs(pad.height) <= moonclient::kReliefPadMostUnits);
  }
}

TEST_CASE("Relief : deux repères dans des morceaux voisins se dressent chacun à plat",
          "[app][survivors]") {
  // Les places de la graine autour du départ ; la paire de morceaux voisins
  // dont les collines s'écartent le plus entre leurs centres.
  const ro::sim::TerrainRule& rule = moonclient::test_support::ShippedPlains();
  constexpr std::uint64_t kNeighbourSeed = 1;
  const std::int64_t start_cell =
      ro::sim::TerrainPathCellOf(rule, ro::sim::TerrainChunkOf(ro::sim::kTerrainStartCell));
  std::vector<ro::sim::TerrainPlaza> plazas;
  for (std::int64_t cell_y = start_cell - kSearchPathCells; cell_y <= start_cell + kSearchPathCells;
       ++cell_y) {
    for (std::int64_t cell_x = start_cell - kSearchPathCells;
         cell_x <= start_cell + kSearchPathCells; ++cell_x) {
      const ro::sim::TerrainPlaza plaza =
          ro::sim::TerrainPathPlaza(rule, kNeighbourSeed, cell_x, cell_y);
      if (plaza.exists) plazas.push_back(plaza);
    }
  }
  const auto hills = [](const ro::sim::TerrainPlaza& plaza) {
    return moonclient::GroundHillsAt(kNeighbourSeed, static_cast<double>(plaza.centre.x),
                                     static_cast<double>(plaza.centre.y));
  };
  std::optional<std::pair<ro::sim::TerrainPlaza, ro::sim::TerrainPlaza>> pair;
  double widest_gap = 0.0;
  for (std::size_t one = 0; one < plazas.size(); ++one) {
    for (std::size_t other = one + 1; other < plazas.size(); ++other) {
      const std::int64_t apart = std::max(std::abs(plazas[one].centre.x - plazas[other].centre.x),
                                          std::abs(plazas[one].centre.y - plazas[other].centre.y));
      const double gap = std::abs(hills(plazas[one]) - hills(plazas[other]));
      if (apart == ro::sim::kTerrainChunkCells && gap > widest_gap) {
        widest_gap = gap;
        pair = std::make_pair(plazas[one], plazas[other]);
      }
    }
  }
  REQUIRE(pair.has_value());
  INFO("places en (" << pair->first.centre.x << ", " << pair->first.centre.y << ") et ("
                     << pair->second.centre.x << ", " << pair->second.centre.y
                     << "), collines écartées de " << widest_gap);
  // Assez écartées pour que l'une, à sa propre hauteur, tire l'autre.
  CHECK(widest_gap >= kNeighbourLeastGapUnits);
  const auto middle_x =
      static_cast<std::int32_t>((pair->first.centre.x + pair->second.centre.x) / 2);
  const auto middle_y =
      static_cast<std::int32_t>((pair->first.centre.y + pair->second.centre.y) / 2);
  const GroundPlan plan = Plan(kNeighbourSeed, GroundWindowAround(middle_x, middle_y));
  // La fusion reste bornée : chaque terrain livré a des carrés de repères
  // assez larges.
  for (const ro::sim::TerrainRule& terrain : ro::sim::ShippedSurvivorsRules().terrains) {
    if (terrain.landmark_period_chunks == 0) continue;
    INFO("terrain " << terrain.key);
    CHECK(terrain.landmark_period_chunks >= moonclient::kReliefLeastLandmarkPeriodChunks);
  }
  // Les deux terrasses sont fusionnées, à la même hauteur, chacune plate.
  const auto pad_at = [&plan](const ro::sim::TerrainPlaza& plaza) {
    return PadUnder(plan, static_cast<float>(plaza.centre.x - plan.origin.x_cells),
                    static_cast<float>(plaza.centre.y - plan.origin.y_cells));
  };
  const std::optional<GroundReliefPad> first = pad_at(pair->first);
  const std::optional<GroundReliefPad> second = pad_at(pair->second);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  CHECK(first->height == second->height);
  CHECK(CheckPadsFlat(plan) > 0);
}

TEST_CASE("Relief : les normales et la lumière suivent les hauteurs du plan", "[app][survivors]") {
  const GroundPlan plan = Plan(kSeed, StartWindow());
  const GroundWindowOrigin origin = plan.origin;
  std::size_t sloped = 0;
  double total_gap = 0.0;
  std::size_t cells = 0;
  for (std::int32_t y = 1; y + 1 < kGroundWindowCells; ++y) {
    for (std::int32_t x = 1; x + 1 < kGroundWindowCells; ++x) {
      // La pente lue sur les coins de la case, contre celle de la normale en
      // son centre : la hauteur du fichier descend, le sud est vers les z.
      const std::array<float, 4>& corners = plan.cell_heights[CellIndex(x, y)];
      const double towards_east =
          static_cast<double>((corners[1] + corners[3]) - (corners[0] + corners[2])) / 2.0 /
          kGroundReliefCellUnits;
      const double towards_north =
          static_cast<double>((corners[2] + corners[3]) - (corners[0] + corners[1])) / 2.0 /
          kGroundReliefCellUnits;
      const glm::vec3 normal = moonclient::GroundReliefNormalAt(
          kSeed, origin.x_cells + x + kHalf, origin.y_cells + y + kHalf, plan.relief_pads);
      INFO("case (" << x << ", " << y << ")");
      CHECK(normal.x / normal.y == Catch::Approx(towards_east).margin(kChordSlack));
      CHECK(-normal.z / normal.y == Catch::Approx(towards_north).margin(kChordSlack));
      total_gap += std::abs(static_cast<double>(normal.x / normal.y) - towards_east) +
                   std::abs(static_cast<double>(-normal.z / normal.y) - towards_north);
      ++cells;
      // La lumière d'un acteur se lit sur la normale de la case dessinée.
      // Tournée vers le soleil ou à plat, le plein jour ; à l'opposé, plus
      // sombre. Le sol : plus clair vers le soleil.
      const glm::vec3 drawn = moonclient::GroundCellNormal(corners);
      const float shade = plan.cell_shades[CellIndex(x, y)];
      CHECK(shade == moonclient::GroundSlopeActorShade(drawn, kFieldLight));
      const float ground = moonclient::GroundSlopeShade(drawn, kFieldLight);
      const glm::vec3 toward_sun = glm::normalize(kFieldLight.toward_sun);
      if (glm::dot(drawn, toward_sun) >= toward_sun.y) {
        CHECK(shade == 1.0F);
        CHECK(ground >= moonclient::kGroundFlatShade);
      } else {
        CHECK(shade < 1.0F);
        CHECK(ground < moonclient::kGroundFlatShade);
        ++sloped;
      }
    }
  }
  CHECK(sloped > 0);
  // En moyenne, la normale et la corde de la case s'accordent de près.
  CHECK(total_gap / static_cast<double>(2 * cells) <= kMeanChordSlack);
  const glm::vec3 up(0.0F, 1.0F, 0.0F);
  CHECK(moonclient::GroundSlopeShade(up, kFieldLight) ==
        Catch::Approx(moonclient::kGroundFlatShade));
  CHECK(moonclient::GroundSlopeActorShade(up, kFieldLight) == 1.0F);
  // Une pente au plafond, à l'opposé du soleil : nettement plus sombre ; vers
  // lui, plus claire que le plat.
  const glm::vec3 away = glm::normalize(glm::vec3(0.3F, 1.0F, -0.3F));
  const glm::vec3 facing = glm::normalize(glm::vec3(-0.3F, 1.0F, 0.3F));
  CHECK(moonclient::GroundSlopeShade(away, kFieldLight) < kDarkSideShade);
  CHECK(moonclient::GroundSlopeShade(facing, kFieldLight) > moonclient::kGroundFlatShade);
  CHECK(moonclient::GroundSlopeShade(away, kFieldLight) >= moonclient::kDarkestSlopeShade);
  // Sans diffuse, aucune pente ne change de lumière.
  CHECK(moonclient::GroundSlopeShade(away, GroundPlanLight{}) ==
        moonclient::GroundSlopeShade(up, GroundPlanLight{}));
}

TEST_CASE("Relief : le bruit à gradients reste sous ses bornes, quelles que soient ses directions",
          "[app][survivors]") {
  // Toutes les directions des quatre nœuds, sur une grille de la cellule.
  double highest = 0.0;
  double steepest = 0.0;
  std::array<std::uint32_t, 4> directions{};
  const std::uint32_t combinations =
      moonclient::kGradientDirections * moonclient::kGradientDirections *
      moonclient::kGradientDirections * moonclient::kGradientDirections;
  for (std::uint32_t combination = 0; combination < combinations; ++combination) {
    std::uint32_t rest = combination;
    for (std::uint32_t& direction : directions) {
      direction = rest % moonclient::kGradientDirections;
      rest /= moonclient::kGradientDirections;
    }
    for (int i = 0; i <= kNoiseGridSteps; ++i) {
      for (int j = 0; j <= kNoiseGridSteps; ++j) {
        const moonclient::GradientCell cell =
            moonclient::GradientNoiseIn(directions, static_cast<double>(i) / kNoiseGridSteps,
                                        static_cast<double>(j) / kNoiseGridSteps);
        highest = std::max(highest, std::abs(cell.value));
        steepest = std::max({steepest, std::abs(cell.along_east), std::abs(cell.along_north)});
      }
    }
  }
  INFO("valeur la plus haute " << highest << ", pente la plus raide " << steepest);
  CHECK(highest <= moonclient::kGradientNoiseMost);
  CHECK(steepest <= moonclient::kGradientNoiseAxisSteepest);
  // La borne n'est pas lâche : le bruit l'approche.
  CHECK(steepest >= kTightBoundShare * moonclient::kGradientNoiseAxisSteepest);

  // Les dérivées sont celles de la valeur, et la surface se raccorde d'une
  // cellule à l'autre.
  const std::array<std::uint32_t, 4> some = {1, 6, 3, 4};
  const moonclient::GradientCell at = moonclient::GradientNoiseIn(some, kHalf, kHalf);
  const double east_value = moonclient::GradientNoiseIn(some, kHalf + kNoiseProbe, kHalf).value;
  const double west_value = moonclient::GradientNoiseIn(some, kHalf - kNoiseProbe, kHalf).value;
  const double north_value = moonclient::GradientNoiseIn(some, kHalf, kHalf + kNoiseProbe).value;
  const double south_value = moonclient::GradientNoiseIn(some, kHalf, kHalf - kNoiseProbe).value;
  CHECK(at.along_east == Catch::Approx((east_value - west_value) / (2.0 * kNoiseProbe)));
  CHECK(at.along_north == Catch::Approx((north_value - south_value) / (2.0 * kNoiseProbe)));
  for (double north = 0.0; north < kNoisePeriod; north += kNoisePeriod / kNoiseGridSteps) {
    CHECK(
        moonclient::VisualGradientNoise(kSeed, 1, kNoisePeriod - kNoiseEdge, north, kNoisePeriod) ==
        Catch::Approx(moonclient::VisualGradientNoise(kSeed, 1, kNoisePeriod + kNoiseEdge, north,
                                                      kNoisePeriod))
            .margin(kNoiseEdgeSlack));
  }
}

TEST_CASE("Relief : l'emprise de la caméra, sur un sol en creux, ne voit pas le bord de la fenêtre",
          "[app][survivors]") {
  // Le héros au sommet d'une colline, le bord de l'image au fond d'un creux.
  const auto drop = static_cast<float>(moonclient::kGroundReliefSpanUnits);
  const ro::sim::SpawnFrame& frame = ro::sim::ShippedSurvivorsRules().spawn_frame;
  const GroundWindowOrigin start = StartWindow();
  const auto start_margin = static_cast<float>(std::min(
      ro::sim::kTerrainStartCell - start.x_cells, ro::sim::kTerrainStartCell - start.y_cells));
  const auto half_window = static_cast<float>(kGroundWindowCells / 2);
  for (const float aspect : kAspects) {
    INFO("format " << aspect);
    // Au plus grand recul, le héros au plus loin du centre avant un recentrage
    // forcé.
    ro::render::Camera camera;
    camera.distance = moonclient::ArenaFarthestDistance(aspect, frame);
    camera.pitch_degrees = moonclient::kArenaElevationDegrees;
    camera.fov_degrees = moonclient::ArenaFovDegrees(aspect);
    const auto far_seen = moonclient::GroundFootprintOf(camera, aspect, drop);
    REQUIRE(far_seen.has_value());
    // La marge avant le bord reste entière, et l'emprise sur le creux tient
    // dans ce que le recentrage forcé compte que l'image voit.
    const float forced = static_cast<float>(moonclient::kGroundForceCells);
    const auto margin = static_cast<float>(moonclient::kGroundEdgeMarginCells);
    const auto widest = static_cast<float>(moonclient::kGroundWidestSightCells);
    INFO("emprise sur le creux, ouest " << far_seen->west_cells << ", est "
                                        << far_seen->east_cells);
    for (const float seen : {far_seen->west_cells, far_seen->east_cells, far_seen->north_cells,
                             far_seen->south_cells}) {
      CHECK(seen <= widest);
      CHECK(forced + seen + margin <= half_window);
    }
    // Au départ de la descente bornée, le héros au départ de la fenêtre.
    camera.distance = moonclient::ArenaDescentCeiling(
        aspect, static_cast<float>(moonclient::kGroundDescentReachCells), drop);
    const auto descent_seen = moonclient::GroundFootprintOf(camera, aspect, drop);
    REQUIRE(descent_seen.has_value());
    CHECK(descent_seen->west_cells <= start_margin);
    CHECK(descent_seen->east_cells <= start_margin);
    CHECK(descent_seen->north_cells <= start_margin);
    CHECK(descent_seen->south_cells <= start_margin);
  }
}

TEST_CASE("Relief : chaque modèle est posé sur le sol sous lui, chaque repère sur sa terrasse",
          "[app][survivors]") {
  const ro::test_support::TempDirectory client;
  moonclient::test_support::FillSyntheticPrairie(client);
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"essai.grf"}});
  REQUIRE(vfs.has_value());
  auto sources =
      moonclient::OpenGroundSources(*vfs, moonclient::test_support::ShippedPlains(), kSeed,
                                    moonclient::test_support::SyntheticPrairieLook());
  REQUIRE(sources.has_value());
  const GroundPlan plan = moonclient::PlanGroundWindow(
      *sources->rule, sources->run_seed, *sources->look, StartWindow(), sources->plan_light);
  auto stage = moonclient::AssembleGroundWindow(*vfs, *sources, plan, "essai");
  REQUIRE(stage.has_value());
  const ro::formats::GndMap& gnd = stage->scene.gnd;
  const auto& models = stage->scene.rsw.models();
  REQUIRE(models.size() == plan.poses.size());
  const float depth = static_cast<float>(kGroundWindowCubes) * ro::render::kWorldUnitsPerCube;
  // L'altitude du sol au point (east, north) de la fenêtre, en cases.
  const auto ground_at = [&gnd, depth](float east, float north) {
    return static_cast<double>(ro::render::GroundHeightAt(
        gnd, east * ro::render::kWorldUnitsPerCell,
        depth + ro::render::kWorldUnitsPerCube - north * ro::render::kWorldUnitsPerCell));
  };
  // Le plus qu'un modèle s'enfonce sous son pied : la pente au plafond, sur la
  // portée de ses pieds en diagonale.
  const double deepest =
      kGroundSlopeCap * std::sqrt(2.0) * kFootReachCells * kGroundReliefCellUnits + kHeightSlack;
  std::size_t on_pads = 0;
  for (std::size_t rank = 0; rank < models.size(); ++rank) {
    const moonclient::PlannedPose& pose = plan.poses[rank];
    const double ground = ground_at(pose.east, pose.north);
    const double base = -models[rank].position[1];
    INFO("modèle " << rank << " en (" << pose.east << ", " << pose.north << ")");
    // Ni flottant au-dessus de son pied, ni enfoui au-delà de la pente.
    CHECK(base <= ground + kHeightSlack);
    CHECK(base >= ground - deepest);
    if (pose.piece == moonclient::kSeedPose) continue;
    const moonclient::PlannedPiece& piece = plan.pieces[pose.piece];
    if (piece.rule == nullptr || !piece.rule->landmark) continue;
    const std::optional<GroundReliefPad> pad = PadUnder(plan, pose.east, pose.north);
    REQUIRE(pad.has_value());
    CHECK(base == Catch::Approx(-pad->height).margin(kHeightSlack));
    ++on_pads;
  }
  CHECK(on_pads > 0);

  // LE SOL ARRONDI garde le plafond, entre les cubes comme en eux.
  double steepest = 0.0;
  for (std::int32_t j = 1; j + 1 < kGroundWindowCubes; ++j) {
    for (std::int32_t i = 1; i + 1 < kGroundWindowCubes; ++i) {
      const ro::render::CubeRelief relief = ro::render::CubeReliefOf(gnd, i, j);
      for (int a = 0; a <= kRoundedProbes; ++a) {
        for (int b = 0; b <= kRoundedProbes; ++b) {
          const std::array<float, 3> normal =
              ro::render::ReliefNormalIn(relief, static_cast<float>(a) / kRoundedProbes,
                                         static_cast<float>(b) / kRoundedProbes);
          steepest =
              std::max(steepest, static_cast<double>(std::hypot(normal[0], normal[2]) / normal[1]));
        }
      }
    }
  }
  INFO("sol arrondi : pente la plus raide " << steepest);
  CHECK(steepest <= kGroundSlopeCap + kSlopeSlack);

  // LA LUMIÈRE DES ACTEURS SUIT LA PENTE : celle de la carte, fois la part que
  // la pente garde sous leurs pieds.
  REQUIRE(stage->gat.has_value());
  const ro::render::GroundLight laid = ro::render::BuildGroundLight(gnd, *stage->gat);
  REQUIRE(stage->ground_light.factors.size() == plan.cell_shades.size());
  REQUIRE(laid.factors.size() == plan.cell_shades.size());
  std::size_t darker = 0;
  for (std::size_t cell = 0; cell < laid.factors.size(); ++cell) {
    for (std::size_t channel = 0; channel < laid.factors[cell].size(); ++channel) {
      CHECK(stage->ground_light.factors[cell][channel] ==
            Catch::Approx(laid.factors[cell][channel] * plan.cell_shades[cell]));
    }
    darker += plan.cell_shades[cell] < 1.0F ? 1U : 0U;
  }
  CHECK(darker > 0);
}

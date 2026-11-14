// LA PANNE DE COURANT DU LABORATOIRE : des lampes sur les crêtes de ses murs,
// posées par la seule case du monde — deux fenêtres qui se recouvrent posent
// les mêmes, le plan comme la scène —, qui s'éteignent par paliers sur
// l'horloge de la nuit et sonnent l'alarme au boss final. Les biomes dont le
// ciel tombe n'en ont aucune, et gardent leur ciel.

#include "survivors/arena_blackout.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_terrain.h"

#include "survivors/arena_ground_look.h"
#include "survivors/arena_ground_plan.h"
#include "survivors/arena_music.h"
#include "survivors/arena_night.h"
#include "survivors/arena_wall_lamps.h"

using moonclient::ArenaDarkness;
using moonclient::BiomeLook;
using moonclient::GroundPlan;
using moonclient::GroundPlanLight;
using moonclient::GroundWindowAround;
using moonclient::GroundWindowOrigin;
using moonclient::kGroundCellsPerCube;
using moonclient::kGroundWindowCells;
using moonclient::kSeedPose;
using moonclient::PlannedPose;
using moonclient::WallLamp;
using moonclient::WallLampField;
using moonclient::WallLampLook;

namespace {

constexpr std::uint64_t kSeed = 7;
constexpr std::uint64_t kOtherSeed = 20261111;
const GroundPlanLight kHighSun{.toward_sun = glm::vec3(0.0F, 1.0F, 0.0F)};
// Une fenêtre qui recouvre celle du départ d'un morceau à l'est et de deux au
// nord.
constexpr std::int32_t kOverlapEastCells = ro::sim::kTerrainChunkCells;
constexpr std::int32_t kOverlapNorthCells = 2 * ro::sim::kTerrainChunkCells;
// Une place vue de deux fenêtres s'arrondit à quelques millièmes près.
constexpr float kWorldPointSlack = 0.01F;
// Une salle porte des lampes : la fenêtre du départ en a au moins tant.
constexpr std::size_t kFewestWindowLamps = 20;
// La part éteinte se lit sur les lampes de neuf fenêtres : à tant près.
constexpr float kShareSlack = 0.06F;
constexpr std::int32_t kSampledWindowsSide = 3;
// L'alarme se mesure à tant d'instants d'une période.
constexpr std::uint64_t kAlarmSamples = 12;

struct Biome {
  const ro::sim::TerrainRule* rule = nullptr;
  const BiomeLook* look = nullptr;
};

Biome ShippedBiome(std::string_view key) {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  Biome biome;
  for (const ro::sim::ArenaRule& arena : rules.arenas) {
    if (arena.terrain_id == 0) continue;
    const ro::sim::TerrainRule* rule = ro::sim::FindTerrain(rules, arena.terrain_id);
    if (rule != nullptr && std::string_view(rule->key) == key) biome.rule = rule;
  }
  REQUIRE(biome.rule != nullptr);
  biome.look = moonclient::FindBiomeLook(key);
  REQUIRE(biome.look != nullptr);
  return biome;
}

Biome Lab() { return ShippedBiome("lighthalzen_biolab"); }

GroundWindowOrigin StartWindow() {
  return GroundWindowAround(ro::sim::kTerrainStartCell, ro::sim::kTerrainStartCell);
}

GroundWindowOrigin Shifted(GroundWindowOrigin origin, std::int32_t east, std::int32_t north) {
  return {.x_cells = origin.x_cells + east, .y_cells = origin.y_cells + north};
}

std::vector<WallLamp> WindowLamps(const Biome& biome, std::uint64_t seed,
                                  GroundWindowOrigin origin) {
  WallLampField field(*biome.rule, seed, *biome.look);
  std::vector<WallLamp> lamps;
  field.LampsOfWindow(origin, lamps);
  return lamps;
}

bool IsLampModel(const BiomeLook& look, std::string_view model) {
  for (const WallLampLook& lamp : moonclient::WallLampLooksOf(look)) {
    if (lamp.model == model) return true;
  }
  return false;
}

// Les poses de lampes d'un plan, en cases du monde.
using WorldPose = std::tuple<std::string_view, float, float, float>;
std::vector<WorldPose> LampPoses(const BiomeLook& look, const GroundPlan& plan) {
  std::vector<WorldPose> poses;
  for (const PlannedPose& pose : plan.poses) {
    if (pose.piece != kSeedPose || !IsLampModel(look, pose.model)) continue;
    poses.emplace_back(pose.model, pose.east + static_cast<float>(plan.origin.x_cells),
                       pose.north + static_cast<float>(plan.origin.y_cells), pose.yaw_degrees);
  }
  return poses;
}

bool InsideWindow(GroundWindowOrigin origin, float east, float north) {
  return east >= static_cast<float>(origin.x_cells) &&
         north >= static_cast<float>(origin.y_cells) &&
         east < static_cast<float>(origin.x_cells + kGroundWindowCells) &&
         north < static_cast<float>(origin.y_cells + kGroundWindowCells);
}

// Les lampes de neuf fenêtres voisines.
std::vector<WallLamp> SampledLamps(const Biome& biome) {
  WallLampField field(*biome.rule, kSeed, *biome.look);
  std::vector<WallLamp> all;
  std::vector<WallLamp> window;
  for (std::int32_t row = 0; row < kSampledWindowsSide; ++row) {
    for (std::int32_t column = 0; column < kSampledWindowsSide; ++column) {
      field.LampsOfWindow(
          Shifted(StartWindow(), column * kGroundWindowCells, row * kGroundWindowCells), window);
      all.insert(all.end(), window.begin(), window.end());
    }
  }
  return all;
}

float OutShareOf(const std::vector<WallLamp>& lamps, std::uint64_t presentation_ms) {
  std::size_t out = 0;
  for (const WallLamp& lamp : lamps) {
    if (!moonclient::WallLampLit(lamp, presentation_ms)) ++out;
  }
  return static_cast<float>(out) / static_cast<float>(lamps.size());
}

}  // namespace

TEST_CASE("Le labo est en panne, les champs voient tomber la nuit", "[survivors][blackout]") {
  CHECK(Lab().look->darkness == ArenaDarkness::kBlackout);
  CHECK_FALSE(moonclient::WallLampLooksOf(*Lab().look).empty());
  for (const std::string_view key : {"midgard_plains", "morroc_desert", "lutie_snowfield"}) {
    const Biome field = ShippedBiome(key);
    CHECK(field.look->darkness == ArenaDarkness::kNightfall);
    CHECK(moonclient::WallLampLooksOf(*field.look).empty());
    CHECK(WindowLamps(field, kSeed, StartWindow()).empty());
    // Leur ciel tombe comme avant, palier par palier.
    for (std::uint64_t minute = 0; minute <= moonclient::kArenaMidnightMs;
         minute += moonclient::kArenaNightStepMs) {
      CHECK(moonclient::ArenaSkyHourOf(field.look->darkness, minute) ==
            moonclient::ArenaSkyHour(minute));
    }
    // Leur plan ne pose aucune lampe.
    const GroundPlan plan =
        moonclient::PlanGroundWindow(*field.rule, kSeed, *field.look, StartWindow(), kHighSun);
    for (const PlannedPose& pose : plan.poses) {
      CHECK_FALSE(IsLampModel(*Lab().look, pose.model));
    }
  }
  // Le ciel du labo ne tombe jamais : il garde la lumière de sa carte.
  CHECK_FALSE(moonclient::ArenaSkyHourOf(ArenaDarkness::kBlackout, moonclient::kArenaMidnightMs)
                  .has_value());
}

TEST_CASE("Une lampe se pose au pied d'un mur, sur le sol qu'il borde", "[survivors][blackout]") {
  const Biome lab = Lab();
  const GroundPlan plan =
      moonclient::PlanGroundWindow(*lab.rule, kSeed, *lab.look, StartWindow(), kHighSun);
  const std::vector<WorldPose> poses = LampPoses(*lab.look, plan);
  REQUIRE(poses.size() >= kFewestWindowLamps);
  // Les lampes de la scène sont celles du plan, une pose chacune.
  const std::vector<WallLamp> lamps = WindowLamps(lab, kSeed, StartWindow());
  REQUIRE(lamps.size() == poses.size());
  const auto wall_at = [&plan](std::int32_t x, std::int32_t y) {
    return plan.wall[static_cast<std::size_t>(y - plan.origin.y_cells) * kGroundWindowCells +
                     static_cast<std::size_t>(x - plan.origin.x_cells)] != 0;
  };
  const auto inside = [&plan](std::int32_t x, std::int32_t y) {
    return x >= plan.origin.x_cells && y >= plan.origin.y_cells &&
           x < plan.origin.x_cells + kGroundWindowCells &&
           y < plan.origin.y_cells + kGroundWindowCells;
  };
  for (std::size_t rank = 0; rank < lamps.size(); ++rank) {
    const WallLamp& lamp = lamps[rank];
    const glm::vec2 cell = moonclient::WallLampCell(lamp);
    // La pose est celle de la lampe.
    CHECK(std::abs(std::get<1>(poses[rank]) - cell.x) < kWorldPointSlack);
    CHECK(std::abs(std::get<2>(poses[rank]) - cell.y) < kWorldPointSlack);
    // Tout le cube qui la porte est de mur, dans la fenêtre.
    const std::int32_t cube_west = lamp.cube_x * kGroundCellsPerCube;
    const std::int32_t cube_south = lamp.cube_y * kGroundCellsPerCube;
    for (std::int32_t dy = 0; dy < kGroundCellsPerCube; ++dy) {
      for (std::int32_t dx = 0; dx < kGroundCellsPerCube; ++dx) {
        REQUIRE(inside(cube_west + dx, cube_south + dy));
        CHECK(wall_at(cube_west + dx, cube_south + dy));
      }
    }
    // Elle-même se tient sur le sol, hors du mur.
    const auto x = static_cast<std::int32_t>(std::floor(cell.x));
    const auto y = static_cast<std::int32_t>(std::floor(cell.y));
    if (inside(x, y)) CHECK_FALSE(wall_at(x, y));
  }
}

TEST_CASE("Deux fenêtres qui se recouvrent posent les mêmes lampes", "[survivors][blackout]") {
  const Biome lab = Lab();
  const GroundWindowOrigin first = StartWindow();
  const GroundWindowOrigin second = Shifted(first, kOverlapEastCells, kOverlapNorthCells);
  // Par le plan.
  const std::vector<WorldPose> from_first = LampPoses(
      *lab.look, moonclient::PlanGroundWindow(*lab.rule, kSeed, *lab.look, first, kHighSun));
  const std::vector<WorldPose> from_second = LampPoses(
      *lab.look, moonclient::PlanGroundWindow(*lab.rule, kSeed, *lab.look, second, kHighSun));
  std::size_t shared = 0;
  for (const auto& [model, east, north, yaw] : from_first) {
    if (!InsideWindow(second, east, north)) continue;
    ++shared;
    bool found = false;
    for (const auto& [other_model, other_east, other_north, other_yaw] : from_second) {
      found = found || (other_model == model && std::abs(other_east - east) < kWorldPointSlack &&
                        std::abs(other_north - north) < kWorldPointSlack && other_yaw == yaw);
    }
    CHECK(found);
  }
  CHECK(shared > 0);
  // Par le champ : l'ordre des lectures n'y change rien.
  WallLampField forward(*lab.rule, kSeed, *lab.look);
  WallLampField backward(*lab.rule, kSeed, *lab.look);
  std::vector<WallLamp> ignored;
  backward.LampsOfWindow(second, ignored);
  std::vector<WallLamp> read_first;
  std::vector<WallLamp> read_after;
  forward.LampsOfWindow(first, read_first);
  backward.LampsOfWindow(first, read_after);
  CHECK(read_first == read_after);
  // Une autre graine pose autrement.
  CHECK(WindowLamps(lab, kOtherSeed, first) != read_first);
}

TEST_CASE("La part de lampes éteintes suit les paliers de la nuit", "[survivors][blackout]") {
  const std::vector<WallLamp> lamps = SampledLamps(Lab());
  REQUIRE(lamps.size() >= kSampledWindowsSide * kSampledWindowsSide * kFewestWindowLamps);
  // Toutes luisent avant 7:00.
  CHECK(OutShareOf(lamps, moonclient::kArenaNightFallsMs - 1) == 0.0F);
  std::set<std::size_t> out_before;
  for (std::uint64_t ms = moonclient::kArenaNightFallsMs; ms <= moonclient::kArenaMidnightMs;
       ms += moonclient::kArenaNightStepMs) {
    const float expected = moonclient::ArenaLampsOutShare(ms);
    CHECK(expected == *moonclient::ArenaNightShare(ms) * moonclient::kArenaBlackoutMostShare);
    CHECK(std::abs(OutShareOf(lamps, ms) - expected) < kShareSlack);
    // Une lampe éteinte le reste.
    std::set<std::size_t> out_now;
    for (std::size_t rank = 0; rank < lamps.size(); ++rank) {
      if (!moonclient::WallLampLit(lamps[rank], ms)) out_now.insert(rank);
    }
    for (const std::size_t rank : out_before) CHECK(out_now.contains(rank));
    out_before = std::move(out_now);
  }
  // Au boss final, il en reste.
  CHECK(OutShareOf(lamps, moonclient::kArenaMidnightMs) < 1.0F);
}

TEST_CASE("Au boss final, les lampes qui restent sonnent l'alarme", "[survivors][blackout]") {
  const std::vector<WallLamp> lamps = SampledLamps(Lab());
  const glm::vec3 floor(0.0F);
  constexpr float kLightHeight = 14.0F;
  const std::uint64_t before = moonclient::kArenaBossFromMs - 1;
  CHECK_FALSE(moonclient::ArenaAlarm(before));
  CHECK(moonclient::ArenaAlarm(moonclient::kArenaBossFromMs));
  const WallLamp* survivor = nullptr;
  for (const WallLamp& lamp : lamps) {
    if (moonclient::WallLampLit(lamp, moonclient::kArenaBossFromMs)) survivor = &lamp;
  }
  REQUIRE(survivor != nullptr);
  // Avant : le blanc, constant, à sa hauteur.
  const std::optional<ro::render::PointLight> white =
      moonclient::WallLampLight(*survivor, floor, kLightHeight, before);
  REQUIRE(white.has_value());
  CHECK(white->color == moonclient::kWallLampColor * moonclient::kWallLampStrength);
  CHECK(white->world_position.y == kLightHeight);
  CHECK_FALSE(white->casts_shadow);
  // Après : du rouge, qui bat.
  float brightest = 0.0F;
  float dimmest = 1.0F;
  for (std::uint64_t sample = 0; sample < kAlarmSamples; ++sample) {
    const std::uint64_t ms =
        moonclient::kArenaBossFromMs + sample * moonclient::kWallLampAlarmPeriodMs / kAlarmSamples;
    const std::optional<ro::render::PointLight> red =
        moonclient::WallLampLight(*survivor, floor, kLightHeight, ms);
    REQUIRE(red.has_value());
    CHECK(red->color.r > red->color.g);
    CHECK(red->color.r > red->color.b);
    brightest = std::max(brightest, red->color.r);
    dimmest = std::min(dimmest, red->color.r);
  }
  CHECK(brightest > 2.0F * dimmest);
  // Une lampe éteinte ne se rallume pas pour l'alarme.
  for (const WallLamp& lamp : lamps) {
    if (!moonclient::WallLampLit(lamp, moonclient::kArenaBossFromMs)) {
      CHECK_FALSE(moonclient::WallLampLight(lamp, floor, kLightHeight, moonclient::kArenaBossFromMs)
                      .has_value());
    }
  }
}

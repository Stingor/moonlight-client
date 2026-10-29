#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/core/pcg32.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_ground.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_state.h"

#include "survivors_space.h"
#include "survivors_systems.h"

namespace ro::sim::survivors {

// LES PLANTES : posées par blocs au départ sur une carte, ou nées au fil des
// tués, deux autour du départ puis une juste hors de l'écran à chaque compte
// de tués atteint ; une plante laissée loin du héros se perd. Toutes tirent
// le flux décor (kFieldStream), que rien d'autre ne tire.

namespace {

// Vrai si les règles posent des plantes par blocs : une espèce, un bloc et une
// densité.
[[nodiscard]] bool PlantedByBlocks(const SurvivorsRules& rules) noexcept {
  return !rules.plants.empty() && rules.tuning.plant_block_cells > 0 &&
         rules.tuning.plant_cells_per_plant != 0;
}

// Vrai si les règles font naître des plantes au fil des tués.
[[nodiscard]] bool SownByKills(const SurvivorsRules& rules) noexcept {
  return !rules.plants.empty() && rules.tuning.plant_kills_per_minute != 0;
}

[[nodiscard]] std::vector<std::uint32_t> PlantWeights(const SurvivorsRules& rules) {
  std::vector<std::uint32_t> weights;
  weights.reserve(rules.plants.size());
  for (const PlantRule& plant : rules.plants) weights.push_back(plant.weight);
  return weights;
}

// Une plante de l'espèce tirée par un PickWeighted du flux décor, posée en
// `at` ; rien pour un tirage hors des espèces.
void SowAt(SurvivorsField& field, std::span<const std::uint32_t> weights, FieldPoint at) {
  const SurvivorsRules& rules = *field.rules;
  const std::size_t pick = field.field_random.PickWeighted(weights);
  if (pick >= rules.plants.size()) return;
  field.plants.push_back(SurvivorsPlant{
      .serial = field.next_serial++, .foe_id = rules.plants[pick].foe_id, .position = at});
}

// LA PLANTE D'UN BLOC, parmi ses cases praticables `open` : un Chance du
// nombre de ces cases sur `plant_cells_per_plant`, même pour un bloc vide
// (le décor d'un bloc ne décale pas les suivants) ; si oui, un Below pour la
// case et un PickWeighted pour l'espèce.
void PlantBlock(SurvivorsField& field, std::span<const std::uint32_t> weights,
                std::span<const FieldPoint> open) {
  const SurvivorsRules& rules = *field.rules;
  const auto open_count = static_cast<std::uint32_t>(open.size());
  if (!field.field_random.Chance(open_count, rules.tuning.plant_cells_per_plant) || open.empty()) {
    return;
  }
  const FieldPoint at = open[field.field_random.Below(open_count)];
  SowAt(field, weights, at);
}

[[nodiscard]] bool PlantedOn(const SurvivorsField& field, FieldPoint at) {
  return std::any_of(field.plants.begin(), field.plants.end(),
                     [&](const SurvivorsPlant& plant) { return plant.position == at; });
}

}  // namespace

void PlantField(SurvivorsField& field) {
  if (field.rules == nullptr || !field.ground.bounded()) return;
  const SurvivorsRules& rules = *field.rules;
  if (!PlantedByBlocks(rules)) return;
  const std::int32_t block = rules.tuning.plant_block_cells;
  const std::vector<std::uint32_t> weights = PlantWeights(rules);

  const SurvivorsGround& ground = field.ground;
  std::vector<FieldPoint> open;
  for (std::int32_t block_y = 0; block_y < ground.height(); block_y += block) {
    for (std::int32_t block_x = 0; block_x < ground.width(); block_x += block) {
      open.clear();
      const std::int32_t last_y = std::min(block_y + block, ground.height());
      const std::int32_t last_x = std::min(block_x + block, ground.width());
      for (std::int32_t y = block_y; y < last_y; ++y) {
        for (std::int32_t x = block_x; x < last_x; ++x) {
          if (ground.Passable(x, y)) open.push_back(CellCenter(x, y));
        }
      }
      PlantBlock(field, weights, open);
    }
  }
}

void SowStartPlants(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  const SurvivorsRules& rules = *field.rules;
  if (!SownByKills(rules)) return;
  const SurvivorsTuning& tuning = rules.tuning;
  const std::vector<std::uint32_t> weights = PlantWeights(rules);
  const std::int32_t hero_x = CellOf(field.hero.position.x);
  const std::int32_t hero_y = CellOf(field.hero.position.y);
  const std::size_t count =
      std::min<std::size_t>(tuning.plant_start_count, kPlantStartHeadings.size());
  for (std::size_t index = 0; index < count; ++index) {
    const PlantStartHeading heading = kPlantStartHeadings[index];
    const std::optional<CellPosition> cell = NearestPassable(
        field.ground, hero_x + heading.east * tuning.plant_start_cells,
        hero_y + heading.north * tuning.plant_start_cells, tuning.spawn_search_ring);
    if (!cell) continue;
    const FieldPoint at = CellCenter(cell->x, cell->y);
    if (PlantedOn(field, at)) continue;
    SowAt(field, weights, at);
  }
}

void ForgetFarPlants(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  const std::int64_t forget = field.rules->tuning.plant_forget_units;
  if (forget <= 0) return;
  const FieldPoint hero = field.hero.position;
  std::erase_if(field.plants, [&](const SurvivorsPlant& plant) {
    const std::int64_t dx = std::int64_t{plant.position.x} - hero.x;
    const std::int64_t dy = std::int64_t{plant.position.y} - hero.y;
    return std::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy) > forget;
  });
}

void SowPlants(SurvivorsField& field) {
  if (field.rules == nullptr) return;
  const SurvivorsRules& rules = *field.rules;
  if (!SownByKills(rules)) return;
  const SurvivorsTuning& tuning = rules.tuning;
  const std::uint64_t minute = std::uint64_t{field.step} / kStepsPerMinute + 1;
  const std::uint64_t needed = std::uint64_t{tuning.plant_kills_per_minute} * minute;
  const std::uint64_t counted = std::uint64_t{field.tally.kills} - field.plant_kills_mark;
  if (counted < needed || field.plants.size() >= tuning.most_live_plants) return;
  if (!FramePerimeterDrawable(rules.spawn_frame)) return;
  for (std::uint8_t attempt = 0; attempt < tuning.spawn_tries_per_foe; ++attempt) {
    const std::optional<FieldPoint> at = TryFramePoint(field, field.field_random);
    if (!at) continue;
    const std::size_t before = field.plants.size();
    SowAt(field, PlantWeights(rules), *at);
    if (field.plants.size() != before) field.plant_kills_mark = field.tally.kills;
    return;
  }
}

}  // namespace ro::sim::survivors

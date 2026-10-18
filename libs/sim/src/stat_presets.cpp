#include "ro/sim/stat_presets.h"

#include <algorithm>

#include "ro/core/text.h"

namespace ro::sim {
namespace {

// Les trois paliers de la règle de coût, et ce qui multiplie la valeur dans
// chacun.
constexpr std::int64_t kFirstTierTop = 200;
constexpr std::int64_t kSecondTierTop = 300;
constexpr std::int64_t kSecondTierFactor = 2;
constexpr std::int64_t kThirdTierFactor = 4;
// `1 + (k × value + 9) / 10` : le dixième arrondi au-dessus, plus un.
constexpr std::int64_t kCostDivisor = 10;
constexpr std::int64_t kCostRoundUp = kCostDivisor - 1;

[[nodiscard]] bool IsBlank(char letter) {
  return letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r';
}

// Le coût de la marche qui part de `value`, le premier annoncé par le serveur
// quand il l'a dit.
[[nodiscard]] std::int64_t StepCost(std::int64_t value, std::int64_t current,
                                    std::int64_t announced) {
  if (value == current && announced > 0) {
    return announced;
  }
  return BaseStatPointCost(value);
}

// LA HAUTEUR QUE LE PLAFOND LAISSE ATTEINDRE depuis `current`.
[[nodiscard]] std::int64_t ReachableTop(std::int64_t current, std::int64_t announced,
                                        std::int64_t ceiling) {
  // ⚠ UN COÛT ANNONCÉ NUL : le serveur dit que cette valeur EST le plafond.
  if (announced == 0) {
    return current;
  }
  return std::max(current, ceiling);
}

// Ce que coûterait la montée de `from` à `to`.
[[nodiscard]] std::int64_t PathCost(std::int64_t from, std::int64_t to, std::int64_t current,
                                    std::int64_t announced) {
  std::int64_t total = 0;
  for (std::int64_t value = from; value < to; ++value) {
    total += StepCost(value, current, announced);
  }
  return total;
}

}  // namespace

std::int64_t BaseStatPointCost(std::int64_t value) {
  if (value <= kFirstTierTop) {
    return 1 + (value + kCostRoundUp) / kCostDivisor;
  }
  if (value <= kSecondTierTop) {
    return 1 + (kSecondTierFactor * value + kCostRoundUp) / kCostDivisor;
  }
  return 1 + (kThirdTierFactor * value + kCostRoundUp) / kCostDivisor;
}

std::string TidyStatPresetName(std::string_view typed) {
  while (!typed.empty() && IsBlank(typed.front())) {
    typed.remove_prefix(1);
  }
  while (!typed.empty() && IsBlank(typed.back())) {
    typed.remove_suffix(1);
  }
  constexpr std::size_t kRoom = kStatPresetNameBytes - 1;
  // La coupe tombe au début d'un caractère : un octet orphelin, ni l'écran ni
  // le fichier ne savent le lire.
  return std::string(core::Utf8Prefix(typed, kRoom));
}

StatPresetRefusal StatPresetBook::Save(std::string_view name, const BaseStats& stats) {
  std::string tidy = TidyStatPresetName(name);
  if (tidy.empty()) {
    return StatPresetRefusal::kEmptyName;
  }
  for (StatPreset& kept : presets_) {
    if (kept.name == tidy) {
      kept.stats = stats;
      return StatPresetRefusal::kNone;
    }
  }
  if (presets_.size() >= kStatPresetLimit) {
    return StatPresetRefusal::kFull;
  }
  presets_.push_back(StatPreset{.name = std::move(tidy), .stats = stats});
  return StatPresetRefusal::kNone;
}

StatPresetRefusal StatPresetBook::Rename(std::size_t index, std::string_view name) {
  if (index >= presets_.size()) {
    return StatPresetRefusal::kUnknown;
  }
  std::string tidy = TidyStatPresetName(name);
  if (tidy.empty()) {
    return StatPresetRefusal::kEmptyName;
  }
  for (std::size_t other = 0; other < presets_.size(); ++other) {
    if (other != index && presets_[other].name == tidy) {
      return StatPresetRefusal::kNameTaken;
    }
  }
  presets_[index].name = std::move(tidy);
  return StatPresetRefusal::kNone;
}

StatPresetRefusal StatPresetBook::Remove(std::size_t index) {
  if (index >= presets_.size()) {
    return StatPresetRefusal::kUnknown;
  }
  presets_.erase(presets_.begin() + static_cast<std::ptrdiff_t>(index));
  return StatPresetRefusal::kNone;
}

std::int64_t StatPlan::raises() const noexcept {
  std::int64_t total = 0;
  for (const StatPlanRow& row : rows) {
    total += row.after - row.current;
  }
  return total;
}

StatPlan PlanStatPreset(const BaseStats& target, const StatPlanInput& input) {
  StatPlan plan;
  plan.points_before = input.status_point;
  std::int64_t points = input.status_point;
  bool stopped = false;
  for (std::size_t rank = 0; rank < kBaseStatCount; ++rank) {
    StatPlanRow& row = plan.rows[rank];
    const std::int64_t current = input.current[rank];
    const std::int64_t announced = input.next_cost[rank];
    row.current = current;
    row.after = current;
    row.target = target[rank];
    if (current > row.target) {
      row.verdict = StatPlanVerdict::kAboveTarget;
      continue;
    }
    const std::int64_t top = ReachableTop(current, announced, input.ceiling);
    const std::int64_t goal = std::min(row.target, top);
    while (!stopped && row.after < goal) {
      const std::int64_t cost = StepCost(row.after, current, announced);
      if (cost > points) {
        stopped = true;
        break;
      }
      points -= cost;
      row.spent += cost;
      ++row.after;
    }
    row.missing_steps = row.target - row.after;
    row.missing_points = PathCost(row.after, goal, current, announced);
    if (row.after >= row.target) {
      row.verdict = StatPlanVerdict::kReached;
    } else if (row.after < goal) {
      row.verdict = StatPlanVerdict::kShort;
    } else {
      row.verdict = StatPlanVerdict::kCeiling;
    }
  }
  plan.points_after = points;
  return plan;
}

}  // namespace ro::sim

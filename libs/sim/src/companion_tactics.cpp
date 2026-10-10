// Voir l'en-tête.

#include "ro/sim/companion_tactics.h"

#include <algorithm>

namespace ro::sim {
namespace {

// LES CLASSES DES COFFRES AU TRÉSOR des sièges, en deux plages (données du jeu,
// telles que la base de monstres les numérote).
constexpr std::uint16_t kTreasureChestFirst = 1324;
constexpr std::uint16_t kTreasureChestLast = 1363;
constexpr std::uint16_t kSecondTreasureChestFirst = 1938;
constexpr std::uint16_t kSecondTreasureChestLast = 1946;

template <typename Enum>
[[nodiscard]] bool Within(Enum value, Enum last) noexcept {
  return static_cast<int>(value) <= static_cast<int>(last);
}

}  // namespace

bool ValidTactic(const CompanionTactic& tactic) noexcept {
  const bool key_ok = Within(tactic.key.kind, TacticKeyKind::kMinion) &&
                      (tactic.key.kind != TacticKeyKind::kMonster || tactic.key.job != 0);
  return key_ok && Within(tactic.rank, TacticRank::kIgnore) &&
         Within(tactic.trigger, TacticTrigger::kIfAttacksCompanion) &&
         Within(tactic.avoid, TacticAvoid::kAlways) &&
         Within(tactic.skill_use, TacticSkillUse::kAlways) &&
         Within(tactic.share, TacticShare::kAlways) && Within(tactic.chase, TacticChase::kNever) &&
         tactic.avoid_threshold_cells >= 1 &&
         tactic.avoid_threshold_cells <= kTacticAvoidMostCells && tactic.avoid_step_cells >= 1 &&
         tactic.avoid_step_cells <= kTacticAvoidMostCells && tactic.skill_use_count >= 0 &&
         (tactic.skill_use_count >= 1 || (tactic.skill_use != TacticSkillUse::kTimes &&
                                          tactic.skill_use != TacticSkillUse::kOnceAtLevel)) &&
         tactic.skill_use_count <= kTacticSkillUseMost &&
         tactic.sp_reserve_points >= kTacticSpReserveFromBase &&
         tactic.sp_reserve_points <= kTacticSpReserveMost && tactic.weight >= 0.0F &&
         tactic.weight <= kTacticWeightMost;
}

bool IsTreasureChest(std::uint16_t job) noexcept {
  return (job >= kTreasureChestFirst && job <= kTreasureChestLast) ||
         (job >= kSecondTreasureChestFirst && job <= kSecondTreasureChestLast);
}

const CompanionTactic* CompanionTactics::Find(TacticKey key) const noexcept {
  const auto found = std::ranges::find_if(
      lines_, [key](const CompanionTactic& tactic) { return tactic.key == key; });
  return found != lines_.end() ? &*found : nullptr;
}

bool CompanionTactics::Set(const CompanionTactic& tactic) {
  if (!ValidTactic(tactic)) {
    return false;
  }
  const auto found = std::ranges::find_if(
      lines_, [&tactic](const CompanionTactic& line) { return line.key == tactic.key; });
  if (found != lines_.end()) {
    *found = tactic;
    return true;
  }
  if (lines_.size() >= kCompanionTacticMost) {
    return false;
  }
  lines_.push_back(tactic);
  return true;
}

bool CompanionTactics::Remove(TacticKey key) {
  return std::erase_if(lines_, [key](const CompanionTactic& tactic) { return tactic.key == key; }) >
         0;
}

const CompanionTactic* TacticFor(const CompanionTactics& tactics, const Actor& monster, bool still,
                                 bool minion) noexcept {
  if (const CompanionTactic* own =
          tactics.Find(TacticKey{.kind = TacticKeyKind::kMonster, .job = monster.look.job});
      own != nullptr) {
    return own;
  }
  if (minion) {
    if (const CompanionTactic* line = tactics.Find(TacticKey{.kind = TacticKeyKind::kMinion});
        line != nullptr) {
      return line;
    }
  }
  if (IsTreasureChest(monster.look.job)) {
    if (const CompanionTactic* chest = tactics.Find(TacticKey{.kind = TacticKeyKind::kChest});
        chest != nullptr) {
      return chest;
    }
  }
  if (still) {
    if (const CompanionTactic* plant = tactics.Find(TacticKey{.kind = TacticKeyKind::kStill});
        plant != nullptr) {
      return plant;
    }
  }
  return tactics.Find(TacticKey{.kind = TacticKeyKind::kAnyMonster});
}

}  // namespace ro::sim

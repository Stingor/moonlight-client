// Voir l'en-tête.

#include "ro/sim/companion_players.h"

#include <algorithm>
#include <array>

#include "ro/sim/companion_rules.h"

namespace ro::sim {
namespace {

template <typename Enum>
[[nodiscard]] bool Within(Enum value, Enum last) noexcept {
  return static_cast<int>(value) <= static_cast<int>(last);
}

// LES CLASSES MONTÉES et leur classe à pied, telles que le serveur les numérote.
struct MountedJob {
  std::uint16_t mounted = 0;
  std::uint16_t on_foot = 0;
};
constexpr std::array<MountedJob, 6> kMountedJobs = {{
    {.mounted = 13, .on_foot = 7},       // Knight
    {.mounted = 21, .on_foot = 14},      // Crusader
    {.mounted = 4014, .on_foot = 4008},  // Lord Knight
    {.mounted = 4022, .on_foot = 4015},  // Paladin
    {.mounted = 4036, .on_foot = 4030},  // Baby Knight
    {.mounted = 4044, .on_foot = 4037},  // Baby Crusader
}};
// LES COSTUMES de la première plage : Wedding, puis Xmas à Summer 2.
constexpr std::uint16_t kWeddingJob = 22;
constexpr std::uint16_t kFirstSeasonalJob = 26;
constexpr std::uint16_t kLastSeasonalJob = 30;
// La forme d'union du Star Gladiator, et les classes du 3e métier montées.
constexpr std::uint16_t kStarGladiatorUnionJob = 4048;
constexpr std::uint16_t kFirstMountedThirdJob = 4080;
constexpr std::uint16_t kLastMountedThirdJob = 4087;

}  // namespace

std::uint16_t UnmountedJob(std::uint16_t job) noexcept {
  const auto found = std::ranges::find_if(
      kMountedJobs, [job](const MountedJob& mounted) { return mounted.mounted == job; });
  return found != kMountedJobs.end() ? found->on_foot : job;
}

bool IsConductJob(std::uint16_t job) noexcept {
  return UnmountedJob(job) == job && job != kWeddingJob &&
         !(job >= kFirstSeasonalJob && job <= kLastSeasonalJob) && job != kStarGladiatorUnionJob &&
         !(job >= kFirstMountedThirdJob && job <= kLastMountedThirdJob);
}

CompanionAiContext ContextOfMap(const MapMode& mode) noexcept {
  if (mode.kind == MapKind::kAgitZone && !mode.battlefield) {
    return CompanionAiContext::kWoE;
  }
  return IsVersusMap(mode) && !IsDuelMap(mode) ? CompanionAiContext::kPvP
                                               : CompanionAiContext::kPvE;
}

PlayerTargeting PlayerTargetingOf(const MapMode& mode) noexcept {
  if (!IsVersusMap(mode)) {
    return PlayerTargeting::kNone;
  }
  return IsDuelMap(mode) || mode.battlefield ? PlayerTargeting::kDefenceOnly
                                             : PlayerTargeting::kFull;
}

bool OwnerGuildSpared(const MapMode& mode) noexcept {
  return !mode.battlefield && (mode.kind == MapKind::kAgitZone || mode.attackable_beyond_guild);
}

CompanionAiContext CompanionContextWatch::Observe(std::uint32_t map_entries, const MapMode& mode,
                                                  std::uint32_t arrived_ms,
                                                  std::uint32_t now_ms) noexcept {
  const CompanionAiContext live = ContextOfMap(mode);
  if (!seen_entries_.has_value() || *seen_entries_ != map_entries) {
    seen_entries_ = map_entries;
    unsettled_since_ms_ = arrived_ms;
  }
  if (!unsettled_since_ms_.has_value()) {
    context_ = live;
    return context_;
  }
  const std::int64_t settled_for_ms =
      static_cast<std::int64_t>(now_ms) - static_cast<std::int64_t>(*unsettled_since_ms_);
  if (live != CompanionAiContext::kPvE) {
    context_ = live;
    unsettled_since_ms_.reset();
  } else if (settled_for_ms >= static_cast<std::int64_t>(kCompanionContextSettleMs)) {
    context_ = CompanionAiContext::kPvE;
    unsettled_since_ms_.reset();
  }
  return context_;
}

bool ValidConduct(const CompanionPlayerConduct& conduct) noexcept {
  const bool key_ok =
      Within(conduct.key.kind, PlayerConductKeyKind::kOwnerGuild) &&
      (conduct.key.kind != PlayerConductKeyKind::kPlayer || ValidFriendName(conduct.key.name));
  return key_ok && Within(conduct.rank, TacticRank::kIgnore) &&
         Within(conduct.trigger, TacticTrigger::kIfAttacksCompanion) &&
         Within(conduct.skill_use, TacticSkillUse::kAlways) &&
         Within(conduct.chase, TacticChase::kNever) && conduct.skill_use_count >= 0 &&
         (conduct.skill_use_count >= 1 || (conduct.skill_use != TacticSkillUse::kTimes &&
                                           conduct.skill_use != TacticSkillUse::kOnceAtLevel)) &&
         conduct.skill_use_count <= kTacticSkillUseMost &&
         conduct.sp_reserve_points >= kTacticSpReserveFromBase &&
         conduct.sp_reserve_points <= kTacticSpReserveMost;
}

const CompanionPlayerConduct* CompanionPlayerConducts::Find(
    const PlayerConductKey& key) const noexcept {
  const auto found = std::ranges::find_if(
      lines_, [&key](const CompanionPlayerConduct& conduct) { return conduct.key == key; });
  return found != lines_.end() ? &*found : nullptr;
}

bool CompanionPlayerConducts::Set(const CompanionPlayerConduct& conduct) {
  if (!ValidConduct(conduct)) {
    return false;
  }
  const auto found = std::ranges::find_if(
      lines_, [&conduct](const CompanionPlayerConduct& line) { return line.key == conduct.key; });
  if (found != lines_.end()) {
    *found = conduct;
    return true;
  }
  if (lines_.size() >= kCompanionConductMost) {
    return false;
  }
  lines_.push_back(conduct);
  return true;
}

bool CompanionPlayerConducts::Remove(const PlayerConductKey& key) {
  return std::erase_if(lines_, [&key](const CompanionPlayerConduct& conduct) {
           return conduct.key == key;
         }) > 0;
}

const CompanionPlayerConduct* ConductFor(const CompanionPlayerConducts& conducts,
                                         const Actor& player, bool owner_guildmate) noexcept {
  if (!player.name.empty()) {
    if (const CompanionPlayerConduct* own = conducts.Find(
            PlayerConductKey{.kind = PlayerConductKeyKind::kPlayer, .name = player.name});
        own != nullptr) {
      return own;
    }
  }
  if (owner_guildmate) {
    if (const CompanionPlayerConduct* guild =
            conducts.Find(PlayerConductKey{.kind = PlayerConductKeyKind::kOwnerGuild});
        guild != nullptr) {
      return guild;
    }
  }
  if (const CompanionPlayerConduct* job = conducts.Find(PlayerConductKey{
          .kind = PlayerConductKeyKind::kJob, .job = UnmountedJob(player.look.job)});
      job != nullptr) {
    return job;
  }
  return conducts.Find(PlayerConductKey{.kind = PlayerConductKeyKind::kAnyPlayer});
}

CompanionTactic TacticOfConduct(const CompanionPlayerConduct& conduct) noexcept {
  return CompanionTactic{.key = TacticKey{.kind = TacticKeyKind::kAnyMonster},
                         .rank = conduct.rank,
                         .trigger = conduct.trigger,
                         .avoid = TacticAvoid::kNever,
                         .skill_use = conduct.skill_use,
                         .skill_use_count = conduct.skill_use_count,
                         .opening_skill_id = conduct.opening_skill_id,
                         .harass = false,
                         .sp_reserve_points = conduct.sp_reserve_points,
                         .weight = kTacticDefaultWeight,
                         .share = TacticShare::kAlways,
                         .chase = conduct.chase};
}

bool ShieldedPlayer(const Actor& player, const OwnSide& side,
                    std::span<const std::string> friend_names, std::uint32_t owner_guild_id,
                    bool guild_spared) noexcept {
  if (side.IncludesOrParty(player.id)) {
    return true;
  }
  if (!player.name.empty() && std::ranges::find(friend_names, player.name) != friend_names.end()) {
    return true;
  }
  return guild_spared && OwnerGuildmate(player, owner_guild_id);
}

}  // namespace ro::sim

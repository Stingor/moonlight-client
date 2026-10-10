// Voir l'en-tête.

#include "ro/sim/mercenary.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace ro::sim {
namespace {

// Les classes de chaque guilde de mercenaires, bornes comprises.
constexpr std::uint16_t kArcherFirstJob = 6017;
constexpr std::uint16_t kArcherLastJob = 6026;
constexpr std::uint16_t kSpearmanFirstJob = 6027;
constexpr std::uint16_t kSpearmanLastJob = 6036;
constexpr std::uint16_t kSwordmanFirstJob = 6037;
constexpr std::uint16_t kSwordmanLastJob = 6046;

// L'ASPD : le délai d'attaque retranché de ce plafond, puis divisé.
constexpr int kAspdCeilingMs = 2000;
constexpr int kAspdDivisor = 10;

constexpr std::int64_t kMillisecondsPerSecond = 1000;
constexpr std::uint32_t kSecondsPerMinute = 60;
constexpr std::int64_t kMillisecondsPerMinute = kMillisecondsPerSecond * kSecondsPerMinute;
// L'échéance tient sur l'horloge du client, qui reboucle : un contrat plus long
// que la moitié de sa course ne s'y écrit pas sans mentir.
constexpr std::int64_t kLongestContractMs = std::numeric_limits<std::int32_t>::max();

}  // namespace

MercenaryGuild MercenaryGuildOfJob(std::uint16_t job) {
  if (job >= kArcherFirstJob && job <= kArcherLastJob) {
    return MercenaryGuild::kArcher;
  }
  if (job >= kSpearmanFirstJob && job <= kSpearmanLastJob) {
    return MercenaryGuild::kSpearman;
  }
  if (job >= kSwordmanFirstJob && job <= kSwordmanLastJob) {
    return MercenaryGuild::kSwordman;
  }
  return MercenaryGuild::kMonster;
}

int MercenaryAspd(int amotion_ms) { return (kAspdCeilingMs - amotion_ms) / kAspdDivisor; }

void MercenaryBook::Meet(MercenaryProfile profile, std::uint32_t now_ms, std::int64_t now_unix_s) {
  const std::int64_t left_ms = std::clamp(
      (static_cast<std::int64_t>(profile.expire_unix_s) - now_unix_s) * kMillisecondsPerSecond,
      std::int64_t{0}, kLongestContractMs);
  contract_deadline_ms_ = now_ms + static_cast<std::uint32_t>(left_ms);
  for (std::size_t rank = 0; rank < kMercenaryReminderMinutes.size(); ++rank) {
    reminder_armed_[rank] = left_ms > static_cast<std::int64_t>(kMercenaryReminderMinutes[rank]) *
                                          kMillisecondsPerMinute;
  }
  profile_ = std::move(profile);
  present_ = true;
}

std::uint32_t MercenaryBook::TakeContractReminder(std::uint32_t now_ms) noexcept {
  if (!present_) {
    return 0;
  }
  const std::uint32_t left_s = contract_left_s(now_ms);
  std::uint32_t crossed_minutes = 0;
  for (std::size_t rank = 0; rank < kMercenaryReminderMinutes.size(); ++rank) {
    const std::uint32_t minutes = kMercenaryReminderMinutes[rank];
    if (reminder_armed_[rank] && left_s <= minutes * kSecondsPerMinute) {
      reminder_armed_[rank] = false;
      crossed_minutes = minutes;
    }
  }
  return crossed_minutes;
}

bool MercenaryBook::Change(MercenaryStat stat, std::int32_t value) {
  if (!present_) {
    return false;
  }
  switch (stat) {
    case MercenaryStat::kHp:
      profile_.hp = value;
      break;
    case MercenaryStat::kMaxHp:
      profile_.max_hp = value;
      break;
    case MercenaryStat::kSp:
      profile_.sp = value;
      break;
    case MercenaryStat::kMaxSp:
      profile_.max_sp = value;
      break;
    case MercenaryStat::kAtk:
      profile_.atk = value;
      break;
    case MercenaryStat::kMatk:
      profile_.matk = value;
      break;
    case MercenaryStat::kHit:
      profile_.hit = value;
      break;
    case MercenaryStat::kCrit:
      profile_.crit = value;
      break;
    case MercenaryStat::kDef:
      profile_.def = value;
      break;
    case MercenaryStat::kMdef:
      profile_.mdef = value;
      break;
    case MercenaryStat::kFlee:
      profile_.flee = value;
      break;
    case MercenaryStat::kAmotion:
      profile_.amotion_ms = value;
      break;
    case MercenaryStat::kKills:
      profile_.kills = static_cast<std::uint32_t>(value);
      break;
    case MercenaryStat::kFaith:
      profile_.faith = value;
      break;
  }
  return true;
}

void MercenaryBook::SetSkills(std::vector<MercenarySkill> skills) { skills_ = std::move(skills); }

void MercenaryBook::Part(MercenaryFarewell why) {
  present_ = false;
  profile_ = MercenaryProfile();
  skills_.clear();
  contract_deadline_ms_ = 0;
  reminder_armed_ = {};
  if (why != MercenaryFarewell::kNone) {
    farewell_ = why;
  }
}

MercenaryFarewell MercenaryBook::TakeFarewell() noexcept {
  return std::exchange(farewell_, MercenaryFarewell::kNone);
}

const MercenarySkill* MercenaryBook::FindSkill(std::uint16_t skill_id) const {
  const auto found = std::ranges::find(skills_, skill_id, &MercenarySkill::skill_id);
  return found != skills_.end() ? &*found : nullptr;
}

std::uint32_t MercenaryBook::contract_left_s(std::uint32_t now_ms) const noexcept {
  if (!present_) {
    return 0;
  }
  // La différence se lit SIGNÉE : une échéance passée donne un négatif, que la
  // reboucle de l'horloge ne transforme pas en contrat de quarante jours.
  const auto left_ms = static_cast<std::int32_t>(contract_deadline_ms_ - now_ms);
  if (left_ms <= 0) {
    return 0;
  }
  // Arrondi vers le haut : il reste quelque chose tant que l'échéance n'est pas
  // atteinte.
  return static_cast<std::uint32_t>(
      (static_cast<std::int64_t>(left_ms) + kMillisecondsPerSecond - 1) / kMillisecondsPerSecond);
}

}  // namespace ro::sim

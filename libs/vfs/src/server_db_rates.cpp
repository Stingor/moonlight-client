// La base rangée, ses index, et le taux réel d'un butin. Voir l'en-tête.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/vfs/server_db.h"

namespace ro::vfs {
namespace {

// Un ratio de 100 laisse le taux tel quel.
constexpr std::int64_t kWholeRatio = 100;
constexpr double kWholeRatioReal = 100.0;
// `drop_rateincrease` ne touche qu'un taux sous la moitié.
constexpr std::uint32_t kRateIncreaseCeiling = kDropRateWhole / 2;
// La formule logarithmique : `taux × (5 − log10 taux) ^ (ln ratio / ln 5)`,
// arrondie au plus proche.
constexpr double kLogarithmBase = 5.0;
constexpr double kRounding = 0.5;
// Le plus grand nombre d'EXP du serveur, aux paquets de ce client : celui
// d'un entier signé de 64 bits.
constexpr std::uint64_t kMostExp = static_cast<std::uint64_t>(INT64_MAX);
constexpr double kMostExpReal = static_cast<double>(kMostExp);
// Le plus petit taux qui reste quand un taux ne peut pas tomber à zéro.
constexpr std::int64_t kLeastRate = 1;

// LES FAMILLES D'OBJET des réglages, par le `Type` de la base. ⚠ UN OBJET
// `DelayConsume` EST UN OBJET D'USAGE au moment où les taux se calculent : le
// serveur l'a déjà converti en fin de chargement.
enum class Family : std::uint8_t { kCommon, kHeal, kUse, kEquip, kCard };

[[nodiscard]] bool SameText(std::string_view left, std::string_view right) {
  return left.size() == right.size() &&
         std::ranges::equal(left, right, [](unsigned char a, unsigned char b) {
           return std::tolower(a) == std::tolower(b);
         });
}

[[nodiscard]] Family FamilyOf(std::string_view type) {
  if (SameText(type, "Healing")) {
    return Family::kHeal;
  }
  if (SameText(type, "Usable") || SameText(type, "Cash") || SameText(type, "DelayConsume")) {
    return Family::kUse;
  }
  if (SameText(type, "Weapon") || SameText(type, "Armor") || SameText(type, "PetArmor")) {
    return Family::kEquip;
  }
  if (SameText(type, "Card")) {
    return Family::kCard;
  }
  return Family::kCommon;
}

[[nodiscard]] const DropRateFamily& SettingsOf(const DropSettings& settings, Family family) {
  switch (family) {
    case Family::kHeal:
      return settings.heal;
    case Family::kUse:
      return settings.use;
    case Family::kEquip:
      return settings.equip;
    case Family::kCard:
      return settings.card;
    case Family::kCommon:
      break;
  }
  return settings.common;
}

// `mob_drop_adjust` : le taux ajusté par le ratio, borné.
[[nodiscard]] std::int64_t Adjusted(const DropSettings& settings, std::uint32_t rate,
                                    std::int64_t ratio, std::int64_t least, std::int64_t most) {
  double adjusted = static_cast<double>(rate);
  if (settings.logarithmic && ratio > 0 && ratio != kWholeRatio && rate > 0) {
    adjusted = adjusted * std::pow(kLogarithmBase - std::log10(adjusted),
                                   std::log(static_cast<double>(ratio) / kWholeRatioReal) /
                                       std::log(kLogarithmBase)) +
               kRounding;
  } else {
    adjusted = adjusted * static_cast<double>(ratio) / kWholeRatioReal;
  }
  // ⚠ PAS `std::clamp` : des réglages peuvent poser un minimum au-dessus du
  // maximum, et le serveur borne alors par le haut d'abord.
  const auto most_real = static_cast<double>(most);
  const auto least_real = static_cast<double>(least);
  if (adjusted >= most_real) {
    adjusted = most_real;
  } else if (adjusted <= least_real) {
    adjusted = least_real;
  }
  return static_cast<std::int64_t>(adjusted);
}

}  // namespace

std::uint32_t EffectiveDropRate(const DropSettings& settings, std::uint32_t written_rate,
                                std::string_view item_type, std::uint8_t rank, bool treasure,
                                bool mvp_reward, const std::int64_t* ratio_override) {
  if (written_rate == 0) {
    return 0;
  }
  std::uint32_t rate = written_rate;
  std::int64_t ratio = 0;
  std::int64_t least = 0;
  std::int64_t most = 0;
  if (mvp_reward) {
    ratio = settings.rate_mvp_reward;
    least = settings.least_mvp_reward;
    most = settings.most_mvp_reward;
  } else {
    if (settings.rate_increase && rate < kRateIncreaseCeiling) {
      ++rate;
    }
    if (treasure) {
      ratio = settings.rate_treasure;
      least = settings.least_treasure;
      most = settings.most_treasure;
    } else {
      const DropRateFamily& family = SettingsOf(settings, FamilyOf(item_type));
      if (rank == kServerMonsterMvp) {
        ratio = family.rate_mvp;
        least = family.least_mvp;
        most = family.most_mvp;
      } else if (rank == kServerMonsterBoss) {
        ratio = family.rate_boss;
        least = family.least_boss;
        most = family.most_boss;
      } else {
        ratio = family.rate;
        least = family.least;
        most = family.most;
      }
    }
  }
  if (ratio_override != nullptr) {
    ratio = *ratio_override;
  }
  // UN RATIO NUL RETIRE LA LIGNE, au chargement déjà.
  if (ratio == 0) {
    return 0;
  }
  std::int64_t effective = Adjusted(settings, rate, ratio, least, most);
  // AU TIRAGE, sans joueur : jamais plus que certain, jamais moins d'un
  // centième de pour cent quand le serveur ne laisse pas un taux tomber à zéro.
  effective = std::min<std::int64_t>(effective, kDropRateWhole);
  effective = std::max<std::int64_t>(effective, settings.rate_zero_allowed ? 0 : kLeastRate);
  return static_cast<std::uint32_t>(effective);
}

std::uint64_t EffectiveExp(std::uint64_t written, std::int64_t rate_percent) {
  const double real =
      static_cast<double>(written) * static_cast<double>(rate_percent) / kWholeRatioReal;
  if (real <= 0.0) {
    return 0;
  }
  return real >= kMostExpReal ? kMostExp : static_cast<std::uint64_t>(real);
}

ServerDb ServerDb::Known(std::vector<ServerItem> items, std::vector<ServerMonster> monsters,
                         DropSettings settings) {
  ServerDb db;
  db.known_ = true;
  db.items_ = std::move(items);
  db.monsters_ = std::move(monsters);
  db.settings_ = settings;
  std::ranges::sort(db.items_, {}, &ServerItem::item_id);
  std::ranges::sort(db.monsters_, {}, &ServerMonster::monster_id);
  for (std::size_t monster = 0; monster < db.monsters_.size(); ++monster) {
    const ServerMonster& shown = db.monsters_[monster];
    for (std::size_t drop = 0; drop < shown.drops.size(); ++drop) {
      db.droppers_[shown.drops[drop].item_id].push_back(
          DropRef{.monster = monster, .drop = drop, .mvp_reward = false});
    }
    for (std::size_t drop = 0; drop < shown.mvp_drops.size(); ++drop) {
      db.droppers_[shown.mvp_drops[drop].item_id].push_back(
          DropRef{.monster = monster, .drop = drop, .mvp_reward = true});
    }
  }
  return db;
}

const ServerItem* ServerDb::FindItem(std::uint32_t item_id) const {
  const auto found = std::ranges::lower_bound(items_, item_id, {}, &ServerItem::item_id);
  return found != items_.end() && found->item_id == item_id ? &*found : nullptr;
}

const ServerMonster* ServerDb::FindMonster(std::uint32_t monster_id) const {
  const auto found =
      std::ranges::lower_bound(monsters_, monster_id, {}, &ServerMonster::monster_id);
  return found != monsters_.end() && found->monster_id == monster_id ? &*found : nullptr;
}

void ServerDb::NoteStatuses(std::vector<ServerStatus> statuses) {
  statuses_ = std::move(statuses);
  std::ranges::sort(statuses_, {}, &ServerStatus::constant);
}

const ServerStatus* ServerDb::FindStatus(std::string_view constant) const {
  std::string wanted(constant);
  std::ranges::transform(wanted, wanted.begin(), [](unsigned char letter) {
    return static_cast<char>(std::toupper(letter));
  });
  const auto found = std::ranges::lower_bound(statuses_, wanted, {}, &ServerStatus::constant);
  return found != statuses_.end() && found->constant == wanted ? &*found : nullptr;
}

std::span<const ServerSpawn> ServerDb::SpawnsOf(std::uint32_t monster_id) const {
  const auto found = spawns_.find(monster_id);
  return found != spawns_.end() ? std::span<const ServerSpawn>(found->second)
                                : std::span<const ServerSpawn>();
}

void ServerDb::NoteSpawns(std::uint32_t monster_id, std::vector<ServerSpawn> spawns) {
  spawns_[monster_id] = std::move(spawns);
}

void ServerDb::NoteRatio(std::uint32_t item_id, std::int64_t ratio,
                         std::vector<std::uint32_t> monsters) {
  ratios_[item_id] = Ratio{.ratio = ratio, .monsters = std::move(monsters)};
}

std::uint32_t ServerDb::RateOf(const ServerMonster& monster, const ServerDrop& drop,
                               bool mvp_reward) const {
  const ServerItem* const item = FindItem(drop.item_id);
  const std::int64_t* ratio = nullptr;
  if (const auto found = ratios_.find(drop.item_id); found != ratios_.end()) {
    const std::vector<std::uint32_t>& aimed = found->second.monsters;
    if (aimed.empty() || std::ranges::find(aimed, monster.monster_id) != aimed.end()) {
      ratio = &found->second.ratio;
    }
  }
  return EffectiveDropRate(settings_, drop.written_rate,
                           item != nullptr ? std::string_view(item->type) : std::string_view(),
                           monster.rank, monster.treasure, mvp_reward, ratio);
}

std::vector<ItemDropper> ServerDb::DroppersOf(std::uint32_t item_id) const {
  std::vector<ItemDropper> droppers;
  const auto found = droppers_.find(item_id);
  if (found == droppers_.end()) {
    return droppers;
  }
  for (const DropRef& ref : found->second) {
    const ServerMonster& monster = monsters_[ref.monster];
    const ServerDrop& drop = ref.mvp_reward ? monster.mvp_drops[ref.drop] : monster.drops[ref.drop];
    const std::uint32_t rate = RateOf(monster, drop, ref.mvp_reward);
    if (rate == 0) {
      continue;
    }
    droppers.push_back(ItemDropper{
        .monster_id = monster.monster_id, .rate_hundredths = rate, .mvp_reward = ref.mvp_reward});
  }
  std::ranges::sort(droppers, [](const ItemDropper& left, const ItemDropper& right) {
    return left.rate_hundredths != right.rate_hundredths
               ? left.rate_hundredths > right.rate_hundredths
               : left.monster_id < right.monster_id;
  });
  return droppers;
}

}  // namespace ro::vfs

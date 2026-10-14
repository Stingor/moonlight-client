// Ce que la base de données compose. Voir l'en-tête.

#include "game/database_compose.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <string>
#include <utility>

#include "ro/core/case_fold.h"
#include "ro/core/i18n.h"
#include "ro/ui/inventory_window.h"

#include "game/market_route.h"

namespace moonclient {
namespace {

constexpr std::size_t kLineBytes = 96;
// Les centièmes de pour cent d'un pour cent.
constexpr std::uint32_t kHundredthsPerPercent = 100;
// Le poids se compte en dixièmes.
constexpr std::uint32_t kTenths = 10;
// LES FORMULES D'AVANT LA REFONTE que le serveur applique à un monstre au
// chargement : MATK de INT + (INT/7)² à INT + (INT/5)², DEF de VIT en plus, MDEF
// de INT + VIT/2 en plus.
constexpr int kMatkLeastDivisor = 7;
constexpr int kMatkMostDivisor = 5;
constexpr int kMdefVitDivisor = 2;
// Le temps de marche d'une case d'un monstre que la base ne règle pas.
constexpr std::uint16_t kDefaultWalkMs = 150;
// Le métier qui vaut tous les métiers.
constexpr std::string_view kAllJobs = "All";

[[nodiscard]] bool SameText(std::string_view left, std::string_view right) {
  return left.size() == right.size() &&
         std::ranges::equal(left, right, [](unsigned char a, unsigned char b) {
           return std::tolower(a) == std::tolower(b);
         });
}

[[nodiscard]] std::string Printed(const char* format, unsigned value) {
  std::array<char, kLineBytes> text{};
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
  std::snprintf(text.data(), text.size(), format, value);
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
  return text.data();
}

[[nodiscard]] std::string Zeny(std::uint32_t amount) {
  return ro::ui::GroupThousands(static_cast<std::int64_t>(amount)) + " z";
}

[[nodiscard]] std::string Joined(const std::vector<std::string>& parts) {
  std::string joined;
  for (const std::string& part : parts) {
    if (!joined.empty()) {
      joined += ", ";
    }
    joined += part;
  }
  return joined;
}

// Le libellé d'un emplacement : celui du groupe qui le nomme, ou son nom.
[[nodiscard]] std::string PlaceLabel(std::string_view place) {
  const char* const label = ro::ui::DatabaseItemPlaceLabel(place);
  return label != nullptr ? std::string(label) : std::string(place);
}

[[nodiscard]] std::string WeightText(std::uint32_t tenths) {
  std::array<char, kLineBytes> text{};
  if (tenths % kTenths == 0) {
    std::snprintf(text.data(), text.size(), "%u", static_cast<unsigned>(tenths / kTenths));
  } else {
    std::snprintf(text.data(), text.size(), "%u,%u", static_cast<unsigned>(tenths / kTenths),
                  static_cast<unsigned>(tenths % kTenths));
  }
  return text.data();
}

[[nodiscard]] std::string LevelText(const ro::vfs::ServerItem& item) {
  std::array<char, kLineBytes> text{};
  if (item.equip_level_max != 0 && item.equip_level_max >= item.equip_level_min) {
    std::snprintf(text.data(), text.size(), "%u – %u", static_cast<unsigned>(item.equip_level_min),
                  static_cast<unsigned>(item.equip_level_max));
  } else {
    std::snprintf(text.data(), text.size(), "%u", static_cast<unsigned>(item.equip_level_min));
  }
  return text.data();
}

[[nodiscard]] bool IsEquipment(const ro::vfs::ServerItem& item) { return !item.locations.empty(); }

}  // namespace

std::string ItemTypeLabel(std::string_view server_type) {
  const char* const label = ro::ui::DatabaseItemTypeLabel(server_type);
  return label != nullptr ? std::string(label) : std::string(server_type);
}

std::vector<ItemFactLine> ItemFactsOf(const ro::vfs::ServerItem& item) {
  std::vector<ItemFactLine> lines;
  const auto add = [&](const char* label, std::string value) {
    lines.push_back(ItemFactLine{.label = label, .value = std::move(value)});
  };
  std::string type = ItemTypeLabel(item.type);
  if (!item.sub_type.empty()) {
    type += " (" + item.sub_type + ")";
  }
  add(ro::core::Tr("Type"), std::move(type));
  add(ro::core::Tr("Poids"), WeightText(item.weight_tenths));
  add(ro::core::Tr("Prix d'achat"), Zeny(item.buy_zeny));
  add(ro::core::Tr("Prix de revente"), Zeny(item.sell_zeny));
  if (item.attack > 0) {
    add(ro::core::Tr("Attaque"), Printed("%u", item.attack));
  }
  if (item.magic_attack > 0) {
    add(ro::core::Tr("Attaque magique"), Printed("%u", item.magic_attack));
  }
  if (item.defense > 0) {
    add(ro::core::Tr("Défense"), Printed("%u", item.defense));
  }
  if (item.range_cells > 0) {
    add(ro::core::Tr("Portée"), Printed(ro::core::Tr("%u case(s)"), item.range_cells));
  }
  if (item.slots > 0) {
    add(ro::core::Tr("Emplacements de carte"), Printed("%u", item.slots));
  }
  if (IsEquipment(item)) {
    std::vector<std::string> places;
    for (const std::string& place : item.locations) {
      if (std::string label = PlaceLabel(place); std::ranges::find(places, label) == places.end()) {
        places.push_back(std::move(label));
      }
    }
    add(ro::core::Tr("Se porte"), Joined(places));
  }
  if (item.weapon_level > 0) {
    add(ro::core::Tr("Niveau d'arme"), Printed("%u", item.weapon_level));
  }
  if (item.armor_level > 0) {
    add(ro::core::Tr("Niveau d'armure"), Printed("%u", item.armor_level));
  }
  if (item.equip_level_min > 0) {
    add(ro::core::Tr("Niveau requis"), LevelText(item));
  }
  if (!item.jobs.empty()) {
    const bool all = std::ranges::any_of(
        item.jobs, [](const std::string& job) { return SameText(job, kAllJobs); });
    add(ro::core::Tr("Jobs"), all ? std::string(ro::core::Tr("Tous")) : Joined(item.jobs));
  }
  if (SameText(item.gender, "Female")) {
    add(ro::core::Tr("Sexe"), ro::core::Tr("Femmes seulement"));
  } else if (SameText(item.gender, "Male")) {
    add(ro::core::Tr("Sexe"), ro::core::Tr("Hommes seulement"));
  }
  if (IsEquipment(item)) {
    add(ro::core::Tr("Raffinable"), item.refineable ? ro::core::Tr("Oui") : ro::core::Tr("Non"));
  }
  return lines;
}

std::string DropRateText(std::uint32_t rate_hundredths) {
  std::array<char, kLineBytes> text{};
  std::snprintf(text.data(), text.size(), "%u,%02u %%",
                static_cast<unsigned>(rate_hundredths / kHundredthsPerPercent),
                static_cast<unsigned>(rate_hundredths % kHundredthsPerPercent));
  return text.data();
}

ro::vfs::ItemSearch ItemSearchOf(const ro::ui::DatabaseFilters& filters, std::string_view text) {
  ro::vfs::ItemSearch search{
      .text = text, .level_min = filters.item_level_min, .level_max = filters.item_level_max};
  const auto chosen = [](int rank, std::size_t count) -> std::optional<std::size_t> {
    if (rank <= ro::ui::kDatabaseAnyChoice || static_cast<std::size_t>(rank) > count) {
      return std::nullopt;
    }
    return static_cast<std::size_t>(rank - 1);
  };
  if (const auto type = chosen(filters.item_type, ro::ui::kDatabaseItemTypes.size()); type) {
    search.type = ro::ui::kDatabaseItemTypes[*type].server_type;
  }
  if (const auto place = chosen(filters.item_place, ro::ui::kDatabaseItemPlaces.size()); place) {
    search.locations = ro::ui::kDatabaseItemPlaces[*place].server_places;
  }
  return search;
}

ro::vfs::MonsterSearch MonsterSearchOf(const ro::ui::DatabaseFilters& filters,
                                       std::string_view text) {
  // Le rang d'une liste moins un : le nombre du serveur, ou « tous ».
  const auto number = [](int rank) {
    return rank <= ro::ui::kDatabaseAnyChoice ? ro::vfs::kAnyMonsterTrait : rank - 1;
  };
  return ro::vfs::MonsterSearch{.text = text,
                                .race = number(filters.race),
                                .element = number(filters.element),
                                .size = number(filters.size),
                                .rank = number(filters.rank),
                                .level_min = filters.level_min,
                                .level_max = filters.level_max};
}

ro::sim::MonsterSheet MonsterSheetOf(const ro::vfs::ServerMonster& monster,
                                     const ro::vfs::ServerDb& db, const ItemNamer& item_name) {
  ro::sim::MonsterSheet sheet;
  sheet.sprite_class = monster.monster_id;
  sheet.level = monster.level;
  sheet.hp = monster.hp;
  sheet.sp = monster.sp;
  // L'EXP RÉELLE, celle que le serveur retient — et celle de sa fiche —, et
  // l'écrite pour l'infobulle.
  const ro::vfs::DropSettings& settings = db.settings();
  const auto real = [](std::uint32_t written, std::int64_t rate) {
    return static_cast<std::uint32_t>(
        std::min<std::uint64_t>(ro::vfs::EffectiveExp(written, rate), UINT32_MAX));
  };
  sheet.base_exp = real(monster.base_exp, settings.base_exp_rate);
  sheet.job_exp = real(monster.job_exp, settings.job_exp_rate);
  sheet.mvp_exp = real(monster.mvp_exp, settings.mvp_exp_rate);
  sheet.written_exp = ro::sim::MonsterWrittenExp{.base_exp = monster.base_exp,
                                                 .job_exp = monster.job_exp,
                                                 .mvp_exp = monster.mvp_exp,
                                                 .base_rate_percent = settings.base_exp_rate,
                                                 .job_rate_percent = settings.job_exp_rate,
                                                 .mvp_rate_percent = settings.mvp_exp_rate};
  const auto narrow = [](std::uint32_t value) {
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(value, UINT16_MAX));
  };
  sheet.atk_min = narrow(monster.attack);
  sheet.atk_max = narrow(monster.attack2);
  const int intelligence = monster.int_stat;
  sheet.matk_min = narrow(static_cast<std::uint32_t>(
      intelligence + (intelligence / kMatkLeastDivisor) * (intelligence / kMatkLeastDivisor)));
  sheet.matk_max = narrow(static_cast<std::uint32_t>(
      intelligence + (intelligence / kMatkMostDivisor) * (intelligence / kMatkMostDivisor)));
  sheet.def = narrow(monster.defense);
  sheet.def_bonus = monster.vit;
  sheet.mdef = narrow(monster.magic_defense);
  sheet.mdef_bonus =
      narrow(static_cast<std::uint32_t>(intelligence + monster.vit / kMdefVitDivisor));
  sheet.str = monster.str;
  sheet.agi = monster.agi;
  sheet.vit = monster.vit;
  sheet.int_stat = monster.int_stat;
  sheet.dex = monster.dex;
  sheet.luk = monster.luk;
  sheet.attack_range_cells = monster.attack_range_cells;
  sheet.size = monster.size;
  sheet.race = monster.race;
  sheet.element = monster.element;
  sheet.element_level = monster.element_level;
  sheet.boss_rank = monster.rank;
  sheet.mode = monster.mode;
  sheet.walk_ms_per_cell =
      monster.walk_ms_per_cell != 0 ? monster.walk_ms_per_cell : kDefaultWalkMs;
  sheet.name = monster.name;
  sheet.aegis_name = monster.aegis_name;
  // LES BUTINS ORDINAIRES, PUIS LES RÉCOMPENSES : l'ordre de la fiche du
  // serveur. Une ligne que le serveur retire ne se montre pas.
  for (const bool mvp_reward : {false, true}) {
    for (const ro::vfs::ServerDrop& drop : mvp_reward ? monster.mvp_drops : monster.drops) {
      const std::uint32_t rate = db.RateOf(monster, drop, mvp_reward);
      if (rate == 0) {
        continue;
      }
      sheet.drops.push_back(ro::sim::MonsterDrop{.item_id = drop.item_id,
                                                 .rate_hundredths = rate,
                                                 .mvp_reward = mvp_reward,
                                                 .name = item_name(drop.item_id)});
    }
  }
  for (const ro::vfs::ServerSpawn& spawn : db.SpawnsOf(monster.monster_id)) {
    sheet.spawns.push_back(ro::sim::MonsterSpawn{
        .count = static_cast<std::uint16_t>(std::min<std::uint32_t>(spawn.count, UINT16_MAX)),
        .map_name = spawn.map_name});
  }
  // L'HOMONYMIE, comptée dans la base : un fait, pas une déduction.
  std::uint8_t namesakes = 0;
  for (const ro::vfs::ServerMonster& other : db.monsters()) {
    if (other.name == monster.name) {
      namesakes = static_cast<std::uint8_t>(std::min<int>(namesakes + 1, UINT8_MAX));
      if (sheet.namesake_origin == 0) {
        sheet.namesake_origin = other.monster_id;
      }
    }
  }
  sheet.namesake_count = std::max<std::uint8_t>(namesakes, 1);
  return sheet;
}

std::optional<std::string> ReadDatabaseCommand(std::string_view line) {
  if (line.size() < kDatabaseCommand.size() ||
      !ro::core::EqualsFolded(line.substr(0, kDatabaseCommand.size()), kDatabaseCommand)) {
    return std::nullopt;
  }
  std::string_view rest = line.substr(kDatabaseCommand.size());
  if (!rest.empty() && rest.front() != ' ') {
    return std::nullopt;
  }
  while (!rest.empty() && rest.front() == ' ') {
    rest.remove_prefix(1);
  }
  while (!rest.empty() && rest.back() == ' ') {
    rest.remove_suffix(1);
  }
  return std::string(rest);
}

std::vector<SellerLine> SellerLinesOf(std::span<const ro::sim::MarketOffer> offers,
                                      const ItemNamer& item_name) {
  std::vector<SellerLine> lines;
  lines.reserve(offers.size());
  for (const ro::sim::MarketOffer& offer : offers) {
    std::array<char, kLineBytes> place{};
    std::snprintf(place.data(), place.size(), "%s %d,%d", offer.map_name.c_str(), offer.cell_x,
                  offer.cell_y);
    // Le nom d'un PNJ s'arrête au retour chariot, comme au Marché.
    const std::string_view shop(offer.shop_name);
    lines.push_back(SellerLine{
        .npc = std::string(shop.substr(0, shop.find('\r'))),
        .place = place.data(),
        .price = MarketPriceText(
            offer, offer.currency_item != 0 ? item_name(offer.currency_item) : std::string())});
  }
  return lines;
}

}  // namespace moonclient

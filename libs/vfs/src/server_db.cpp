// La lecture des trois tables de la base de données du serveur. Voir l'en-tête.

#include "ro/vfs/server_db.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/formats/rathena_db_text.h"

namespace ro::vfs {
namespace {

constexpr std::string_view kItemDbType = "ITEM_DB";
constexpr std::string_view kMobDbType = "MOB_DB";
constexpr int kContextVersion = 1;
constexpr std::string_view kTrueText = "true";
constexpr std::string_view kTreasureGroup = "Treasure";
constexpr std::string_view kWeaponType = "Weapon";
constexpr std::string_view kCardType = "Card";
// Les objets dont le serveur impose le genre, quel qu'en soit l'écrit.
constexpr std::uint32_t kWeddingRingMale = 2634;
constexpr std::uint32_t kWeddingRingFemale = 2635;
constexpr std::string_view kMaleOnlySubType = "Musical";
constexpr std::string_view kFemaleOnlySubType = "Whip";
constexpr std::string_view kMaleGender = "Male";
constexpr std::string_view kFemaleGender = "Female";
// `override_mob_names` à cette valeur montre `JapaneseName`.
constexpr std::int64_t kJapaneseNameOverride = 2;
// Les champs d'une ligne `monsters` du contexte, dans l'ordre que le script écrit.
constexpr std::size_t kRaceField = 0;
constexpr std::size_t kElementField = 1;
constexpr std::size_t kElementLevelField = 2;
constexpr std::size_t kSizeField = 3;
constexpr std::size_t kRankField = 4;
constexpr std::size_t kModeField = 5;
constexpr std::size_t kFactFieldCount = 6;
// Une ligne `item_ratios` : le ratio, puis les monstres visés.
constexpr std::size_t kRatioField = 0;
constexpr std::size_t kRatioMonstersField = 1;
constexpr std::size_t kRatioFieldCount = 2;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "base de données : " + std::move(detail)};
}

// ⚠ UNE CLÉ ABSENTE REND UN NŒUD INVALIDE, dont yaml-cpp refuse même la
// question du type : on demande d'abord s'il existe.
[[nodiscard]] bool IsScalarNode(const YAML::Node& node) { return node && node.IsScalar(); }
[[nodiscard]] bool IsMapNode(const YAML::Node& node) { return node && node.IsMap(); }
[[nodiscard]] bool IsSequenceNode(const YAML::Node& node) { return node && node.IsSequence(); }

[[nodiscard]] std::string Lowered(std::string_view text) {
  std::string lowered(text);
  std::ranges::transform(lowered, lowered.begin(), [](unsigned char letter) {
    return static_cast<char>(std::tolower(letter));
  });
  return lowered;
}

[[nodiscard]] bool SameText(std::string_view left, std::string_view right) {
  return left.size() == right.size() &&
         std::ranges::equal(left, right, [](unsigned char a, unsigned char b) {
           return std::tolower(a) == std::tolower(b);
         });
}

// Un entier écrit comme le serveur le lit ; `fallback` s'il ne se lit pas, et
// borné à la largeur de `Number`.
template <typename Number>
[[nodiscard]] Number NumberIn(std::string_view text, Number fallback) {
  std::int64_t value = 0;
  const char* const first = text.data() + (text.starts_with('+') ? 1 : 0);
  const auto [end, failure] = std::from_chars(first, text.data() + text.size(), value);
  if (failure != std::errc() || end != text.data() + text.size()) {
    return fallback;
  }
  value = std::clamp<std::int64_t>(value, std::numeric_limits<Number>::min(),
                                   std::numeric_limits<Number>::max());
  return static_cast<Number>(value);
}

template <typename Number>
[[nodiscard]] Number NumberOf(const YAML::Node& node, Number fallback) {
  return IsScalarNode(node) ? NumberIn<Number>(node.Scalar(), fallback) : fallback;
}

// ── LES CHAMPS D'UNE ENTRÉE, sous la forme stricte ──────────────────────────
using Field = formats::RathenaField;
using Pairs = std::vector<formats::RathenaPair>;

template <typename Number>
[[nodiscard]] Number FieldNumber(const Field& field, Number fallback) {
  return field.shape == formats::RathenaShape::kText ? NumberIn<Number>(field.text, fallback)
                                                     : fallback;
}

[[nodiscard]] std::string FieldText(const Field& field) {
  return field.shape == formats::RathenaShape::kText ? field.text : std::string();
}

[[nodiscard]] bool FieldTrue(const Field& field) {
  return field.shape == formats::RathenaShape::kText && SameText(field.text, kTrueText);
}

// Les clés levées d'une table de booléens, dans l'ordre écrit.
[[nodiscard]] std::vector<std::string> FieldRaised(const Field& field) {
  std::vector<std::string> raised;
  for (const formats::RathenaPair& pair : field.map) {
    if (SameText(pair.value, kTrueText)) {
      raised.emplace_back(pair.key);
    }
  }
  return raised;
}

// Les clés abaissées d'une table de booléens, dans l'ordre écrit.
[[nodiscard]] std::vector<std::string> FieldLowered(const Field& field) {
  std::vector<std::string> lowered;
  for (const formats::RathenaPair& pair : field.map) {
    if (!SameText(pair.value, kTrueText)) {
      lowered.emplace_back(pair.key);
    }
  }
  return lowered;
}

// La valeur d'un couple d'une table d'une ligne ; vide sans lui.
[[nodiscard]] std::string_view PairValue(const Pairs& pairs, std::string_view key) {
  for (const formats::RathenaPair& pair : pairs) {
    if (pair.key == key) {
      return pair.value;
    }
  }
  return {};
}

ServerItem ItemOf(const formats::RathenaEntry& entry) {
  ServerItem item;
  for (const Field& value : entry) {
    const std::string_view key = value.key;
    if (key == "Id") {
      item.item_id = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "AegisName") {
      item.aegis_name = FieldText(value);
    } else if (key == "Name") {
      item.name = FieldText(value);
    } else if (key == "Type") {
      item.type = FieldText(value);
    } else if (key == "SubType") {
      item.sub_type = FieldText(value);
    } else if (key == "Buy") {
      item.buy_zeny = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "Sell") {
      item.sell_zeny = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "Weight") {
      item.weight_tenths = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "Attack") {
      item.attack = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "MagicAttack") {
      item.magic_attack = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "Defense") {
      item.defense = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "Range") {
      item.range_cells = FieldNumber<std::uint16_t>(value, 0);
    } else if (key == "Slots") {
      item.slots = FieldNumber<std::uint16_t>(value, 0);
    } else if (key == "Jobs") {
      item.jobs = FieldRaised(value);
      item.jobs_lowered = FieldLowered(value);
    } else if (key == "Classes") {
      item.classes = FieldRaised(value);
    } else if (key == "Locations") {
      item.locations = FieldRaised(value);
    } else if (key == "Gender") {
      item.gender = FieldText(value);
    } else if (key == "WeaponLevel") {
      item.weapon_level = FieldNumber<std::uint16_t>(value, 0);
    } else if (key == "ArmorLevel") {
      item.armor_level = FieldNumber<std::uint16_t>(value, 0);
    } else if (key == "EquipLevelMin") {
      item.equip_level_min = FieldNumber<std::uint16_t>(value, kServerItemDefaultEquipLevelMin);
    } else if (key == "EquipLevelMax") {
      item.equip_level_max = FieldNumber<std::uint16_t>(value, 0);
    } else if (key == "Refineable") {
      item.refineable = FieldTrue(value);
    } else if (key == "View") {
      item.view = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "Delay") {
      item.reuse_delay_ms = NumberIn<std::uint32_t>(PairValue(value.map, "Duration"), 0);
    } else if (key == "Script") {
      item.script = FieldText(value);
    } else if (key == "EquipScript") {
      item.equip_script = FieldText(value);
    } else if (key == "UnEquipScript") {
      item.unequip_script = FieldText(value);
    }
  }
  // Un texte vide vaut le champ absent : le défaut du serveur.
  if (item.type.empty()) {
    item.type = kServerItemDefaultType;
  }
  if (item.gender.empty()) {
    item.gender = kServerItemDefaultGender;
  }
  const bool weapon = SameText(item.type, kWeaponType);
  if (item.sub_type.empty() && weapon) {
    item.sub_type = kServerWeaponDefaultSubType;
  } else if (item.sub_type.empty() && SameText(item.type, kCardType)) {
    item.sub_type = kServerCardDefaultSubType;
  }
  // Le genre que le serveur impose, écrit ou non.
  if (item.item_id == kWeddingRingMale || (weapon && SameText(item.sub_type, kMaleOnlySubType))) {
    item.gender = kMaleGender;
  } else if (item.item_id == kWeddingRingFemale ||
             (weapon && SameText(item.sub_type, kFemaleOnlySubType))) {
    item.gender = kFemaleGender;
  }
  return item;
}

using AegisIndex = std::unordered_map<std::string, std::uint32_t>;

std::vector<ServerDrop> DropsOf(const Field& field, const AegisIndex& items) {
  std::vector<ServerDrop> drops;
  for (const Pairs& line : field.list) {
    const auto found = items.find(Lowered(PairValue(line, "Item")));
    if (found == items.end()) {
      continue;
    }
    drops.push_back(
        ServerDrop{.item_id = found->second,
                   .written_rate = NumberIn<std::uint32_t>(PairValue(line, "Rate"), 0),
                   .steal_protected = SameText(PairValue(line, "StealProtected"), kTrueText)});
  }
  return drops;
}

ServerMonster MonsterOf(const formats::RathenaEntry& entry, const AegisIndex& items,
                        bool japanese_name) {
  ServerMonster monster;
  std::string japanese;
  for (const Field& value : entry) {
    const std::string_view key = value.key;
    if (key == "Id") {
      monster.monster_id = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "AegisName") {
      monster.aegis_name = FieldText(value);
    } else if (key == "Name") {
      monster.name = FieldText(value);
    } else if (key == "JapaneseName") {
      japanese = FieldText(value);
    } else if (key == "Level") {
      monster.level = FieldNumber<std::uint16_t>(value, kServerMonsterDefaultLevel);
    } else if (key == "Hp") {
      monster.hp = FieldNumber<std::uint32_t>(value, kServerMonsterDefaultHp);
    } else if (key == "Sp") {
      monster.sp = FieldNumber<std::uint32_t>(value, kServerMonsterDefaultSp);
    } else if (key == "BaseExp") {
      monster.base_exp = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "JobExp") {
      monster.job_exp = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "MvpExp") {
      monster.mvp_exp = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "Attack") {
      monster.attack = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "Attack2") {
      monster.attack2 = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "Defense") {
      monster.defense = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "MagicDefense") {
      monster.magic_defense = FieldNumber<std::uint32_t>(value, 0);
    } else if (key == "Str") {
      monster.str = FieldNumber<std::uint16_t>(value, kServerMonsterDefaultStat);
    } else if (key == "Agi") {
      monster.agi = FieldNumber<std::uint16_t>(value, kServerMonsterDefaultStat);
    } else if (key == "Vit") {
      monster.vit = FieldNumber<std::uint16_t>(value, kServerMonsterDefaultStat);
    } else if (key == "Int") {
      monster.int_stat = FieldNumber<std::uint16_t>(value, kServerMonsterDefaultStat);
    } else if (key == "Dex") {
      monster.dex = FieldNumber<std::uint16_t>(value, kServerMonsterDefaultStat);
    } else if (key == "Luk") {
      monster.luk = FieldNumber<std::uint16_t>(value, kServerMonsterDefaultStat);
    } else if (key == "AttackRange") {
      monster.attack_range_cells = FieldNumber<std::uint16_t>(value, 0);
    } else if (key == "WalkSpeed") {
      monster.walk_ms_per_cell = FieldNumber<std::uint16_t>(value, kServerMonsterDefaultWalkMs);
    } else if (key == "RaceGroups") {
      monster.treasure = std::ranges::any_of(FieldRaised(value), [](const std::string& group) {
        return SameText(group, kTreasureGroup);
      });
    } else if (key == "Drops") {
      monster.drops = DropsOf(value, items);
    } else if (key == "MvpDrops") {
      monster.mvp_drops = DropsOf(value, items);
    }
  }
  if (japanese_name && !japanese.empty()) {
    monster.name = std::move(japanese);
  }
  return monster;
}

[[nodiscard]] std::int64_t SettingOf(const YAML::Node& battle, const char* key,
                                     std::int64_t fallback) {
  return NumberOf<std::int64_t>(battle[key], fallback);
}

// Une famille d'objets : son taux pour chaque rang, et ses bornes, communes
// sauf pour l'équipement.
DropRateFamily FamilyOf(const YAML::Node& battle, const std::string& name) {
  DropRateFamily family;
  const std::string rate = "item_rate_" + name;
  const std::string drop = "item_drop_" + name;
  family.rate = SettingOf(battle, rate.c_str(), family.rate);
  family.rate_boss = SettingOf(battle, (rate + "_boss").c_str(), family.rate_boss);
  family.rate_mvp = SettingOf(battle, (rate + "_mvp").c_str(), family.rate_mvp);
  family.least = SettingOf(battle, (drop + "_min").c_str(), family.least);
  family.most = SettingOf(battle, (drop + "_max").c_str(), family.most);
  family.least_boss = SettingOf(battle, (drop + "_boss_min").c_str(), family.least);
  family.most_boss = SettingOf(battle, (drop + "_boss_max").c_str(), family.most);
  family.least_mvp = SettingOf(battle, (drop + "_mvp_min").c_str(), family.least);
  family.most_mvp = SettingOf(battle, (drop + "_mvp_max").c_str(), family.most);
  return family;
}

DropSettings SettingsOf(const YAML::Node& battle) {
  DropSettings settings;
  settings.common = FamilyOf(battle, "common");
  settings.heal = FamilyOf(battle, "heal");
  settings.use = FamilyOf(battle, "use");
  settings.equip = FamilyOf(battle, "equip");
  settings.card = FamilyOf(battle, "card");
  settings.rate_mvp_reward = SettingOf(battle, "item_rate_mvp", settings.rate_mvp_reward);
  settings.least_mvp_reward = SettingOf(battle, "item_drop_mvp_min", settings.least_mvp_reward);
  settings.most_mvp_reward = SettingOf(battle, "item_drop_mvp_max", settings.most_mvp_reward);
  settings.rate_treasure = SettingOf(battle, "item_rate_treasure", settings.rate_treasure);
  settings.least_treasure = SettingOf(battle, "item_drop_treasure_min", settings.least_treasure);
  settings.most_treasure = SettingOf(battle, "item_drop_treasure_max", settings.most_treasure);
  settings.logarithmic = SettingOf(battle, "item_logarithmic_drops", 0) != 0;
  settings.rate_zero_allowed = SettingOf(battle, "drop_rate0item", 0) != 0;
  settings.rate_increase = SettingOf(battle, "drop_rateincrease", 0) != 0;
  settings.base_exp_rate = SettingOf(battle, "base_exp_rate", settings.base_exp_rate);
  settings.job_exp_rate = SettingOf(battle, "job_exp_rate", settings.job_exp_rate);
  settings.mvp_exp_rate = SettingOf(battle, "mvp_exp_rate", settings.mvp_exp_rate);
  return settings;
}

// Les nombres que le serveur déduit d'un monstre, posés sur lui.
void ApplyFacts(const YAML::Node& row, ServerMonster& monster) {
  if (!IsSequenceNode(row) || row.size() != kFactFieldCount) {
    return;
  }
  monster.race = NumberOf<std::uint8_t>(row[kRaceField], monster.race);
  monster.element = NumberOf<std::uint8_t>(row[kElementField], monster.element);
  monster.element_level = NumberOf<std::uint8_t>(row[kElementLevelField], monster.element_level);
  monster.size = NumberOf<std::uint8_t>(row[kSizeField], monster.size);
  monster.rank = NumberOf<std::uint8_t>(row[kRankField], monster.rank);
  monster.mode = NumberOf<std::uint32_t>(row[kModeField], monster.mode);
}

std::vector<ServerSpawn> SpawnsOf(const YAML::Node& places) {
  std::vector<ServerSpawn> spawns;
  if (!IsMapNode(places)) {
    return spawns;
  }
  for (const auto& place : places) {
    spawns.push_back(ServerSpawn{.map_name = place.first.Scalar(),
                                 .count = NumberOf<std::uint32_t>(place.second, 0)});
  }
  std::ranges::sort(spawns, [](const ServerSpawn& left, const ServerSpawn& right) {
    return left.count != right.count ? left.count > right.count : left.map_name < right.map_name;
  });
  return spawns;
}

}  // namespace

core::Result<ServerDb> ParseServerDb(std::string_view item_db_text, std::string_view mob_db_text,
                                     std::string_view context_text) {
  if (item_db_text.empty() || mob_db_text.empty() || context_text.empty()) {
    return ServerDb{};
  }
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node context = YAML::Load(std::string(context_text));
    if (!IsMapNode(context) || NumberOf<int>(context["version"], 0) != kContextVersion) {
      return std::unexpected(Refused("contexte d'une version non gérée"));
    }
    const YAML::Node battle = context["battle"];
    const DropSettings settings = SettingsOf(battle);
    const bool japanese_name = SettingOf(battle, "override_mob_names", 0) == kJapaneseNameOverride;

    // 🔴 LES DEUX GROSSES TABLES SE PARCOURENT SANS ANALYSEUR YAML : leur script
    // les écrit sous une forme stricte, qu'un curseur lit ligne à ligne.
    std::vector<ServerItem> items;
    AegisIndex aegis;
    if (auto read = formats::ReadRathenaDb(item_db_text, kItemDbType, kServerItemDbPath,
                                           [&](const formats::RathenaEntry& entry) {
                                             ServerItem item = ItemOf(entry);
                                             if (item.item_id == 0) {
                                               return;
                                             }
                                             aegis.emplace(Lowered(item.aegis_name), item.item_id);
                                             items.push_back(std::move(item));
                                           });
        !read) {
      return std::unexpected(read.error());
    }
    // ⚠ INDEXÉS UNE FOIS : chercher une clé entière dans une table yaml-cpp
    // convertit chacune de ses clés, et deux mille monstres en feraient quatre
    // millions.
    std::unordered_map<std::uint32_t, YAML::Node> facts;
    if (const YAML::Node rows = context["monsters"]; IsMapNode(rows)) {
      for (const auto& pair : rows) {
        facts.emplace(NumberOf<std::uint32_t>(pair.first, 0), pair.second);
      }
    }
    std::vector<ServerMonster> monsters;
    if (auto read = formats::ReadRathenaDb(
            mob_db_text, kMobDbType, kServerMobDbPath,
            [&](const formats::RathenaEntry& entry) {
              ServerMonster monster = MonsterOf(entry, aegis, japanese_name);
              if (monster.monster_id == 0) {
                return;
              }
              if (const auto found = facts.find(monster.monster_id); found != facts.end()) {
                ApplyFacts(found->second, monster);
              }
              monsters.push_back(std::move(monster));
            });
        !read) {
      return std::unexpected(read.error());
    }

    ServerDb db = ServerDb::Known(std::move(items), std::move(monsters), settings);
    if (const YAML::Node spawns = context["spawns"]; IsMapNode(spawns)) {
      for (const auto& pair : spawns) {
        db.NoteSpawns(NumberOf<std::uint32_t>(pair.first, 0), SpawnsOf(pair.second));
      }
    }
    if (const YAML::Node ratios = context["item_ratios"]; IsMapNode(ratios)) {
      for (const auto& pair : ratios) {
        const YAML::Node& row = pair.second;
        if (!IsSequenceNode(row) || row.size() != kRatioFieldCount) {
          return std::unexpected(Refused("une exception de ratio n'a pas ses deux champs"));
        }
        std::vector<std::uint32_t> targets;
        for (const YAML::Node& target : row[kRatioMonstersField]) {
          targets.push_back(NumberOf<std::uint32_t>(target, 0));
        }
        db.NoteRatio(NumberOf<std::uint32_t>(pair.first, 0),
                     NumberOf<std::int64_t>(row[kRatioField], 0), std::move(targets));
      }
    }
    return db;
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
}

core::Result<ServerDbTexts> ReadServerDbTexts(Vfs& vfs) {
  ServerDbTexts texts;
  for (auto [path, into] :
       {std::pair{kServerItemDbPath, &texts.item_db}, std::pair{kServerMobDbPath, &texts.mob_db},
        std::pair{kServerDbContextPath, &texts.context}}) {
    auto bytes = vfs.Read(std::string(path));
    if (!bytes) {
      if (bytes.error().code == core::ErrorCode::kNotFound) {
        return ServerDbTexts{};
      }
      return std::unexpected(
          core::Error{bytes.error().code, std::string(path) + " : " + bytes.error().detail});
    }
    into->assign(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  }
  return texts;
}

}  // namespace ro::vfs

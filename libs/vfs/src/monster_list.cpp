#include "ro/vfs/monster_list.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "yaml_required.h"

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;
// Les champs d'une ligne, dans l'ordre que le script écrit.
constexpr std::size_t kAegisField = 0;
constexpr std::size_t kNameField = 1;
constexpr std::size_t kLevelField = 2;
constexpr std::size_t kRaceField = 3;
constexpr std::size_t kElementField = 4;
constexpr std::size_t kElementLevelField = 5;
constexpr std::size_t kSizeField = 6;
constexpr std::size_t kRankField = 7;
constexpr std::size_t kFieldCount = 8;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "liste des monstres : " + std::move(detail)};
}

// Un petit nombre d'une ligne, refusé par exception comme yaml-cpp refuse une
// valeur qui n'entre pas dans son type. ⚠ PAS `as<std::uint8_t>` : yaml-cpp y
// lirait un caractère, pas un nombre.
std::uint8_t SmallOf(const YAML::Node& row, std::size_t field) {
  const auto wide = row[field].as<std::uint16_t>();
  if (wide > std::numeric_limits<std::uint8_t>::max()) {
    throw YAML::BadConversion(row[field].Mark());
  }
  return static_cast<std::uint8_t>(wide);
}

}  // namespace

MonsterList MonsterList::Known(std::vector<MonsterListEntry> entries) {
  MonsterList list;
  list.known_ = true;
  list.entries_ = std::move(entries);
  std::ranges::sort(list.entries_, {}, &MonsterListEntry::monster_id);
  return list;
}

const MonsterListEntry* MonsterList::Find(std::uint16_t monster_id) const {
  const auto found =
      std::ranges::lower_bound(entries_, monster_id, {}, &MonsterListEntry::monster_id);
  return found != entries_.end() && found->monster_id == monster_id ? &*found : nullptr;
}

core::Result<MonsterList> ParseMonsterList(std::string_view yaml_text) {
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const auto root = VersionedRoot(yaml_text, kSupportedVersion);
    if (!root) {
      return std::unexpected(Refused(root.error()));
    }
    const YAML::Node& whole = *root;
    const YAML::Node monsters = whole["monsters"];
    if (!monsters || !monsters.IsMap()) {
      return std::unexpected(Refused("« monsters » n'est pas une table"));
    }
    std::vector<MonsterListEntry> entries;
    entries.reserve(monsters.size());
    for (const auto& pair : monsters) {
      const YAML::Node& row = pair.second;
      if (!row.IsSequence() || row.size() != kFieldCount) {
        return std::unexpected(Refused("une ligne n'a pas ses huit champs"));
      }
      MonsterListEntry entry{.monster_id = pair.first.as<std::uint16_t>(),
                             .aegis_name = row[kAegisField].as<std::string>(),
                             .name = row[kNameField].as<std::string>(),
                             .level = row[kLevelField].as<std::uint16_t>(),
                             .race = SmallOf(row, kRaceField),
                             .element = SmallOf(row, kElementField),
                             .element_level = SmallOf(row, kElementLevelField),
                             .size = SmallOf(row, kSizeField),
                             .rank = SmallOf(row, kRankField)};
      if (entry.monster_id == 0) {
        return std::unexpected(Refused("un identifiant nul"));
      }
      if (entry.rank > kMonsterMvpRank) {
        return std::unexpected(Refused("un rang inconnu"));
      }
      entries.push_back(std::move(entry));
    }
    return MonsterList::Known(std::move(entries));
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
}

core::Result<MonsterList> ReadMonsterList(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kMonsterListPath));
  if (!bytes) {
    if (bytes.error().code == core::ErrorCode::kNotFound) {
      return MonsterList{};
    }
    return std::unexpected(core::Error{
        bytes.error().code, std::string(kMonsterListPath) + " : " + bytes.error().detail});
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return ParseMonsterList(text);
}

}  // namespace ro::vfs

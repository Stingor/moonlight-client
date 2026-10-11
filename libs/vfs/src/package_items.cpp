#include "ro/vfs/package_items.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;
// Une entrée s'écrit [identifiant, poids, quantité].
constexpr std::size_t kEntryIdField = 0;
constexpr std::size_t kEntryWeightField = 1;
constexpr std::size_t kEntryAmountField = 2;
constexpr std::size_t kEntryFieldCount = 3;
// Le premier tirage d'un conteneur porte le rang 1.
constexpr int kFirstDrawRank = 1;

// Les échelles du calcul d'affichage : le client porte la fraction sur sept
// chiffres, en arrondit un, et la rend en pour-cent au dix-millième.
constexpr double kWeightedScale = 10000000.0;
constexpr std::int64_t kRoundingHalf = 5;
constexpr std::int64_t kRoundingStep = 10;
constexpr double kPercentScale = 10000.0;
constexpr double kWholePercent = 100.0;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "table des conteneurs : " + std::move(detail)};
}

// Un champ obligatoire d'une table YAML, absent : une exception, comme yaml-cpp
// signale un champ qui n'entre pas dans son type.
[[nodiscard]] YAML::Node Required(const YAML::Node& map, const char* key) {
  YAML::Node value = map[key];
  if (!value) {
    throw YAML::KeyNotFound(map.Mark(), std::string(key));
  }
  return value;
}

[[noreturn]] void Reject(const YAML::Node& node, const char* why) {
  throw YAML::RepresentationException(node.Mark(), why);
}

[[nodiscard]] PackageGroup ParseDraw(const YAML::Node& node, int rank) {
  if (!node.IsMap()) {
    Reject(node, "un tirage n'est pas une table");
  }
  PackageGroup draw{.id = rank,
                    .guaranteed = Required(node, "guaranteed").as<bool>(),
                    .rolls = Required(node, "rolls").as<int>(),
                    .total = Required(node, "total").as<int>(),
                    .entries = {}};
  const bool monsters = Required(node, "monsters").as<bool>();
  if (draw.rolls < 1 || draw.total < 0) {
    Reject(node, "un tirage sans répétition, ou de total négatif");
  }
  const YAML::Node entries = Required(node, "entries");
  if (!entries.IsSequence()) {
    Reject(entries, "« entries » n'est pas une liste");
  }
  for (const YAML::Node& entry : entries) {
    if (!entry.IsSequence() || entry.size() != kEntryFieldCount) {
      Reject(entry, "une entrée n'est pas un triplet [identifiant, poids, quantité]");
    }
    PackageEntry read{.id = entry[kEntryIdField].as<std::uint32_t>(),
                      .is_monster = monsters,
                      .weight = entry[kEntryWeightField].as<int>(),
                      .amount = entry[kEntryAmountField].as<int>()};
    if (read.id == 0 || read.weight < 0 || read.amount < 0) {
      Reject(entry, "une entrée sans identifiant, ou de poids ou de quantité négatifs");
    }
    draw.entries.push_back(read);
  }
  return draw;
}

}  // namespace

const PackageTable* PackageItems::Find(std::uint32_t package_id) const {
  const auto found = by_package.find(package_id);
  return found == by_package.end() ? nullptr : &found->second;
}

double PackagePercent(bool guaranteed, int weight, int total) {
  if (guaranteed || total <= 0) {
    return kWholePercent;
  }
  const auto scaled = static_cast<std::int64_t>(kWeightedScale * static_cast<double>(weight) /
                                                static_cast<double>(total));
  return static_cast<double>((scaled + kRoundingHalf) / kRoundingStep) / kPercentScale;
}

core::Result<PackageItems> ParsePackageItems(std::string_view yaml_text) {
  PackageItems items;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node whole = YAML::Load(std::string(yaml_text));
    if (!whole.IsMap()) {
      return std::unexpected(Refused("le texte n'est pas une table YAML"));
    }
    const YAML::Node version = whole["version"];
    if (!version || version.as<int>() != kSupportedVersion) {
      return std::unexpected(Refused("version non gérée"));
    }
    const YAML::Node packages = whole["packages"];
    if (!packages || !packages.IsMap()) {
      return std::unexpected(Refused("« packages » n'est pas une table"));
    }
    for (const auto& pair : packages) {
      const auto package_id = pair.first.as<std::uint32_t>();
      if (package_id == 0 || !pair.second.IsSequence()) {
        return std::unexpected(
            Refused("un conteneur nul, ou dont les tirages ne sont pas une liste"));
      }
      PackageTable table;
      int rank = kFirstDrawRank;
      for (const YAML::Node& draw : pair.second) {
        table.groups.push_back(ParseDraw(draw, rank++));
      }
      items.by_package[package_id] = std::move(table);
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return items;
}

core::Result<PackageItems> ReadPackageItems(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kPackageItemsPath));
  if (!bytes) {
    if (bytes.error().code == core::ErrorCode::kNotFound) {
      return PackageItems{};
    }
    return std::unexpected(core::Error{
        bytes.error().code, std::string(kPackageItemsPath) + " : " + bytes.error().detail});
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return ParsePackageItems(text);
}

}  // namespace ro::vfs

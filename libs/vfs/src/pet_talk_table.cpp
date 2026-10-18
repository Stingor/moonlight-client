// Voir l'en-tête.

#include "ro/vfs/pet_talk_table.h"

#include <algorithm>
#include <expected>
#include <optional>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"

#include "yaml_required.h"

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     "répliques des familiers : " + std::move(detail)};
}

// Le rang d'un nom dans sa liste, ou rien quand la liste ne le porte pas.
template <std::size_t kCount>
[[nodiscard]] std::optional<std::size_t> RankOf(const std::array<std::string_view, kCount>& names,
                                                std::string_view name) {
  const auto found = std::ranges::find(names, name);
  if (found == names.end()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(found - names.begin());
}

// Les répliques d'une espèce. Une exception de yaml-cpp, ou un refus.
[[nodiscard]] core::Result<PetTalkLines> ParseSpecies(const YAML::Node& node) {
  if (!node.IsMap()) {
    return std::unexpected(Refused("une espèce n'est pas une table"));
  }
  PetTalkLines lines;
  for (const auto& hunger : node) {
    const std::optional<std::size_t> hunger_rank =
        RankOf(kPetTalkHungerNodes, hunger.first.as<std::string>());
    if (!hunger_rank.has_value() || !hunger.second.IsMap()) {
      return std::unexpected(Refused("rang de faim inconnu : " + hunger.first.as<std::string>()));
    }
    for (const auto& occasion : hunger.second) {
      const std::optional<std::size_t> occasion_rank =
          RankOf(kPetTalkOccasionNodes, occasion.first.as<std::string>());
      if (!occasion_rank.has_value() || !occasion.second.IsSequence()) {
        return std::unexpected(Refused("occasion inconnue : " + occasion.first.as<std::string>()));
      }
      std::vector<std::string>& said = lines.at(*hunger_rank).at(*occasion_rank);
      for (const YAML::Node& line : occasion.second) {
        said.push_back(line.as<std::string>());
      }
    }
  }
  return lines;
}

}  // namespace

std::span<const std::string> PetTalkTable::LinesOf(std::uint32_t pet_class, int hunger_digit,
                                                   int occasion_digit) const {
  if (occasion_digit < 0 ||
      static_cast<std::size_t>(occasion_digit) >= kPetTalkOccasionNodes.size()) {
    return {};
  }
  // Un chiffre de faim hors des rangs nommés parle comme le premier.
  const bool named_hunger =
      hunger_digit > 0 && static_cast<std::size_t>(hunger_digit) < kPetTalkHungerNodes.size();
  const std::size_t hunger_rank = named_hunger ? static_cast<std::size_t>(hunger_digit) : 0;
  const auto species = species_by_class.find(pet_class);
  const std::string& name = species != species_by_class.end() ? species->second : default_species;
  const auto found = lines_by_species.find(name);
  if (found == lines_by_species.end()) {
    return {};
  }
  return found->second.at(hunger_rank).at(static_cast<std::size_t>(occasion_digit));
}

core::Result<PetTalkTable> ParsePetTalkTable(std::string_view yaml_text) {
  PetTalkTable table;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const auto root = VersionedRoot(yaml_text, kSupportedVersion);
    if (!root) {
      return std::unexpected(Refused(root.error()));
    }
    const YAML::Node& whole = *root;
    const YAML::Node fallback = whole["default_species"];
    if (!fallback || !fallback.IsScalar() || fallback.as<std::string>().empty()) {
      return std::unexpected(Refused("pas d'espèce par défaut"));
    }
    table.default_species = fallback.as<std::string>();
    if (const YAML::Node classes = whole["classes"]; classes) {
      if (!classes.IsMap()) {
        return std::unexpected(Refused("« classes » n'est pas une table"));
      }
      for (const auto& pair : classes) {
        table.species_by_class[pair.first.as<std::uint32_t>()] = pair.second.as<std::string>();
      }
    }
    const YAML::Node species = whole["species"];
    if (!species || !species.IsMap()) {
      return std::unexpected(Refused("« species » n'est pas une table"));
    }
    for (const auto& pair : species) {
      core::Result<PetTalkLines> lines = ParseSpecies(pair.second);
      if (!lines) {
        return std::unexpected(lines.error());
      }
      table.lines_by_species[pair.first.as<std::string>()] = std::move(*lines);
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return table;
}

PetTalkTable ReadPetTalkTable(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kPetTalkPath));
  if (!bytes) {
    core::LogWarning("{} absente — les familiers se tairont", kPetTalkPath);
    return {};
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  core::Result<PetTalkTable> table = ParsePetTalkTable(text);
  if (!table) {
    core::LogWarning("{} — les familiers se tairont", core::Describe(table.error()));
    return {};
  }
  return std::move(*table);
}

}  // namespace ro::vfs

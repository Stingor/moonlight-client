// Voir l'en-tête.

#include "ro/vfs/skill_layout.h"

#include <algorithm>
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
constexpr std::int64_t kMostSixteenBits = std::numeric_limits<std::uint16_t>::max();
constexpr std::int64_t kMostJob = std::numeric_limits<std::uint32_t>::max();

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     "disposition des compétences : " + std::move(detail)};
}

[[nodiscard]] std::uint32_t Job(const YAML::Node& node) {
  return static_cast<std::uint32_t>(Whole(node, 0, kMostJob));
}

// Une section : une table, ou rien quand le script n'avait rien à y écrire.
[[nodiscard]] YAML::Node Section(const YAML::Node& whole, const char* key) {
  YAML::Node section = whole[key];
  if (section && !section.IsNull() && !section.IsMap()) {
    throw YAML::RepresentationException(section.Mark(),
                                        std::string("« ") + key + " » n'est pas une table");
  }
  return section;
}

[[nodiscard]] std::vector<SkillLayoutLink> ReadLinks(const YAML::Node& section) {
  std::vector<SkillLayoutLink> links;
  if (!section || section.IsNull()) {
    return links;
  }
  for (const auto& pair : section) {
    links.push_back(SkillLayoutLink{.job = Job(pair.first), .parent = Job(pair.second)});
  }
  std::ranges::sort(links, {}, &SkillLayoutLink::job);
  return links;
}

[[nodiscard]] SkillLayoutGrid ReadGrid(const YAML::Node& job, const YAML::Node& cells) {
  if (!cells.IsMap()) {
    throw YAML::RepresentationException(cells.Mark(), "une grille n'est pas une table");
  }
  SkillLayoutGrid grid{.job = Job(job)};
  grid.slots.reserve(cells.size());
  for (const auto& cell : cells) {
    grid.slots.push_back(SkillLayoutSlot{
        .position = static_cast<std::uint16_t>(Whole(cell.first, 0, kMostSixteenBits)),
        .skill_id = static_cast<std::uint16_t>(Whole(cell.second, 1, kMostSixteenBits))});
  }
  std::ranges::sort(grid.slots, {}, &SkillLayoutSlot::position);
  return grid;
}

}  // namespace

core::Result<SkillLayout> ParseSkillLayout(std::string_view yaml_text) {
  SkillLayout layout;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const auto root = VersionedRoot(yaml_text, kSupportedVersion);
    if (!root) {
      return std::unexpected(Refused(root.error()));
    }
    const YAML::Node& whole = *root;
    if (const YAML::Node grids = Section(whole, "grids"); grids && !grids.IsNull()) {
      for (const auto& pair : grids) {
        layout.grids.push_back(ReadGrid(pair.first, pair.second));
      }
    }
    std::ranges::sort(layout.grids, {}, &SkillLayoutGrid::job);
    layout.inherit = ReadLinks(Section(whole, "inherit"));
    layout.origins = ReadLinks(Section(whole, "origins"));
    if (const YAML::Node names = Section(whole, "tab_names"); names && !names.IsNull()) {
      for (const auto& pair : names) {
        layout.tab_names.push_back(SkillLayoutTabNames{
            .job = Job(pair.first), .names = pair.second.as<std::vector<std::string>>()});
      }
    }
    std::ranges::sort(layout.tab_names, {}, &SkillLayoutTabNames::job);
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return layout;
}

core::Result<SkillLayout> LoadSkillLayout(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kSkillLayoutPath));
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  return ParseSkillLayout(
      std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

}  // namespace ro::vfs

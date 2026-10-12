// Voir l'en-tête.

#include "ro/vfs/job_names.h"

#include <expected>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

namespace ro::vfs {
namespace {

constexpr int kSupportedVersion = 1;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "noms de métiers : " + std::move(detail)};
}

}  // namespace

std::string_view JobNames::Find(std::uint32_t job_id) const {
  const auto found = by_id.find(job_id);
  return found != by_id.end() ? std::string_view(found->second) : std::string_view();
}

core::Result<JobNames> ParseJobNames(std::string_view yaml_text) {
  JobNames names;
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
    const YAML::Node jobs = whole["jobs"];
    if (!jobs || !jobs.IsMap()) {
      return std::unexpected(Refused("« jobs » n'est pas une table"));
    }
    for (const auto& pair : jobs) {
      if (!pair.second.IsScalar()) {
        return std::unexpected(
            Refused("le nom du métier " + pair.first.as<std::string>() + " n'est pas un texte"));
      }
      names.by_id[pair.first.as<std::uint32_t>()] = pair.second.as<std::string>();
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return names;
}

core::Result<JobNames> ReadJobNames(Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kJobNamesPath));
  if (!bytes) {
    if (bytes.error().code == core::ErrorCode::kNotFound) {
      return JobNames{};
    }
    return std::unexpected(
        core::Error{bytes.error().code, std::string(kJobNamesPath) + " : " + bytes.error().detail});
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return ParseJobNames(text);
}

}  // namespace ro::vfs

// Les titres. Voir l'en-tête : le fil n'en porte qu'un numéro, le libellé est
// entièrement du côté du client.

#include "ro/render/title_table.h"

#include <algorithm>
#include <expected>
#include <utility>

#include <yaml-cpp/yaml.h>

namespace ro::render {
namespace {

constexpr int kSupportedVersion = 1;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     std::string(kTitleTablePath) + " : " + std::move(detail)};
}

}  // namespace

core::Result<TitleTable> TitleTable::Load(vfs::Vfs& vfs) {
  auto bytes = vfs.Read(kTitleTablePath);
  if (!bytes) {
    return std::unexpected(bytes.error());
  }
  return Parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
}

core::Result<TitleTable> TitleTable::Parse(std::string_view yaml_text) {
  TitleTable loaded;
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
    const YAML::Node titles = whole["titles"];
    if (!titles || !titles.IsMap()) {
      return std::unexpected(Refused("« titles » n'est pas une table"));
    }
    loaded.entries_.reserve(titles.size());
    for (const auto& pair : titles) {
      if (!pair.second.IsScalar()) {
        return std::unexpected(Refused("un libellé n'est pas un texte"));
      }
      loaded.entries_.push_back(
          Entry{pair.first.as<std::int32_t>(), pair.second.as<std::string>()});
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  std::ranges::sort(loaded.entries_, {}, &Entry::title_id);
  return loaded;
}

std::string_view TitleTable::Find(std::int32_t title_id) const {
  const auto found =
      std::ranges::lower_bound(entries_, title_id, {}, [](const Entry& e) { return e.title_id; });
  if (found == entries_.end() || found->title_id != title_id) {
    return {};
  }
  return found->label;
}

}  // namespace ro::render

// Le livre des réputations. Voir l'en-tête.

#include "world/reputation_book.h"

#include <algorithm>
#include <exception>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"

namespace moonclient {
namespace {

constexpr int kSupportedVersion = 1;

constexpr const char* kVersionField = "version";
constexpr const char* kFactionsSection = "factions";
constexpr const char* kGroupsSection = "groups";
constexpr const char* kIdField = "id";
constexpr const char* kNameField = "name";
constexpr const char* kLeastField = "least";
constexpr const char* kMostField = "most";
constexpr const char* kShownField = "shown";
constexpr const char* kGroupFactionsField = "factions";

constexpr std::string_view kShownNever = "never";
constexpr std::string_view kShownWithPoints = "with_points";

[[nodiscard]] ro::core::Error Refused(std::string detail) {
  return ro::core::Error{ro::core::ErrorCode::kCorruptData, "reputation : " + std::move(detail)};
}

// Une visibilité inconnue se montre : cacher une faction que le serveur tient
// serait pire que la montrer.
[[nodiscard]] ReputationShown ShownOf(const std::string& text) {
  if (text == kShownNever) {
    return ReputationShown::kNever;
  }
  if (text == kShownWithPoints) {
    return ReputationShown::kWithPoints;
  }
  return ReputationShown::kAlways;
}

}  // namespace

ro::core::Result<ReputationBook> ReputationBook::Parse(std::string_view yaml_text) {
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node root = YAML::Load(std::string(yaml_text));
    if (!root.IsMap()) {
      return std::unexpected(Refused("racine qui n'est pas une table"));
    }
    if (root[kVersionField].as<int>(0) != kSupportedVersion) {
      return std::unexpected(Refused("version inconnue"));
    }
    ReputationBook book;
    book.loaded_ = true;
    if (const YAML::Node section = root[kFactionsSection]; section && section.IsSequence()) {
      for (const YAML::Node& entry : section) {
        book.factions_.push_back(ReputationFaction{
            .faction_id = entry[kIdField].as<std::uint64_t>(0),
            .name = entry[kNameField].as<std::string>(""),
            .least = std::min<std::int64_t>(entry[kLeastField].as<std::int64_t>(0), 0),
            .most = std::max<std::int64_t>(entry[kMostField].as<std::int64_t>(0), 0),
            .shown = ShownOf(entry[kShownField].as<std::string>(""))});
      }
    }
    std::ranges::sort(book.factions_, {}, &ReputationFaction::faction_id);
    if (const YAML::Node section = root[kGroupsSection]; section && section.IsSequence()) {
      for (const YAML::Node& entry : section) {
        ReputationGroup group{.group_id = entry[kIdField].as<std::uint64_t>(0),
                              .name = entry[kNameField].as<std::string>("")};
        if (const YAML::Node ids = entry[kGroupFactionsField]; ids && ids.IsSequence()) {
          for (const YAML::Node& faction : ids) {
            group.faction_ids.push_back(faction.as<std::uint64_t>(0));
          }
        }
        book.groups_.push_back(std::move(group));
      }
    }
    return book;
  } catch (const std::exception& trouble) {
    return std::unexpected(Refused(std::string("YAML illisible — ") + trouble.what()));
  }
}

const ReputationFaction* ReputationBook::FindFaction(std::uint64_t faction_id) const noexcept {
  const auto found =
      std::ranges::lower_bound(factions_, faction_id, {}, &ReputationFaction::faction_id);
  return found == factions_.end() || found->faction_id != faction_id ? nullptr : &*found;
}

ReputationBook ReadReputationBook(ro::vfs::Vfs& vfs) {
  auto bytes = vfs.Read(std::string(kReputationBookPath));
  if (!bytes) {
    ro::core::LogWarning("reputation : {} — la fenêtre de réputation le dira",
                         ro::core::Describe(bytes.error()));
    return {};
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  ro::core::Result<ReputationBook> book = ReputationBook::Parse(text);
  if (!book) {
    ro::core::LogWarning("reputation : {} — la fenêtre de réputation le dira",
                         ro::core::Describe(book.error()));
    return {};
  }
  return std::move(*book);
}

}  // namespace moonclient

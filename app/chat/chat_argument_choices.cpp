// Les choix d'argument de la liste des commandes. Voir l'en-tête.

#include "chat/chat_argument_choices.h"

#include <array>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/search_key.h"

#include "chat/server_jobchange.h"

namespace moonclient {
namespace {

struct DeclaredSource {
  std::string_view command_name;
  ChatArgumentSource source;
};

// Les commandes du serveur qui déclarent une source. ⚠ PAR LE NOM
// D'ENREGISTREMENT : les alias (« @job ») y mènent par la liste elle-même.
constexpr auto kServerSources = std::to_array<DeclaredSource>({
    {.command_name = "jobchange", .source = ChatArgumentSource::kServerJobChange},
});

}  // namespace

ChatArgumentSource ServerCommandArgumentSource(std::string_view command_name) {
  for (const DeclaredSource& declared : kServerSources) {
    if (declared.command_name == command_name) {
      return declared.source;
    }
  }
  return ChatArgumentSource::kNone;
}

void ChatArgumentChoices::Rebuild(const ro::vfs::JobNames& job_names) {
  texts_.clear();
  server_jobs_.clear();
  const auto keep = [this](std::string text) -> std::string_view {
    return texts_.emplace_back(std::move(text));
  };
  for (const ServerJobChangeTarget& target : ServerJobChangeTargets()) {
    const std::string_view named = job_names.Find(target.job_id);
    const std::string_view label = named.empty() ? target.server_name : keep(std::string(named));
    const std::string_view detail = ro::core::AsciiEqualsFolded(label, target.server_name)
                                        ? std::string_view()
                                        : target.server_name;
    std::string searched(label);
    if (!detail.empty()) {
      searched += ' ';
      searched += detail;
    }
    server_jobs_.push_back(ro::ui::ChatArgumentChoice{
        .value = keep(std::to_string(target.job_id)),
        .label = label,
        .detail = detail,
        .search_key = keep(ro::core::SearchKey(searched)),
    });
  }
}

std::span<const ro::ui::ChatArgumentChoice> ChatArgumentChoices::Of(
    ChatArgumentSource source) const noexcept {
  switch (source) {
    case ChatArgumentSource::kServerJobChange:
      return server_jobs_;
    case ChatArgumentSource::kNone:
      break;
  }
  return {};
}

}  // namespace moonclient

// Les trois commandes de classement. Voir l'en-tête.

#include "chat/rank_command.h"

#include <array>
#include <utility>

#include "ro/core/case_fold.h"

namespace moonclient {
namespace {

constexpr std::array<std::pair<std::string_view, ro::net::RankKind>, 3> kRankCommands = {{
    {"/blacksmith", ro::net::RankKind::kBlacksmith},
    {"/alchemist", ro::net::RankKind::kAlchemist},
    {"/taekwon", ro::net::RankKind::kTaekwon},
}};

}  // namespace

std::optional<ro::net::RankKind> ReadRankCommand(std::string_view line) {
  for (const auto& [command, kind] : kRankCommands) {
    if (ro::core::EqualsFolded(line, command)) {
      return kind;
    }
  }
  return std::nullopt;
}

}  // namespace moonclient

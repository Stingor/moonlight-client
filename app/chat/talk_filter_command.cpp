// Les deux commandes qui taisent la parole. Voir l'en-tête.

#include "chat/talk_filter_command.h"

#include <array>
#include <utility>

#include "ro/core/case_fold.h"
#include "ro/core/i18n.h"
#include "ro/ui/options_window.h"

namespace moonclient {
namespace {

// LES QUATRE NOMS, l'alias court portant la même bascule que son nom long.
constexpr std::array<std::pair<std::string_view, TalkFilterRow>, 4> kTalkFilterCommands = {{
    {"/notalkmsg", TalkFilterRow::kTalkHidden},
    {"/nm", TalkFilterRow::kTalkHidden},
    {"/notalkmsg2", TalkFilterRow::kTalkAndGuildHidden},
    {"/nm2", TalkFilterRow::kTalkAndGuildHidden},
}};

// L'ÉCHO DU JEU, selon la bascule et ce qu'elle vient de devenir. ⚠ « ON » ET
// « OFF » DISENT L'AFFICHAGE, pas la bascule : taire la parole l'éteint.
[[nodiscard]] std::string EchoOf(TalkFilterRow row, bool hidden) {
  if (row == TalkFilterRow::kTalkAndGuildHidden) {
    return hidden ? ro::core::Tr("La parole est masquée, celle de la guilde comprise. [/nm2 OFF]")
                  : ro::core::Tr("La parole est affichée. [/nm2 ON]");
  }
  return hidden ? ro::core::Tr(
                      "La parole ne s'affichera plus dans la fenêtre de conversation. "
                      "[Affichage OFF]")
                : ro::core::Tr(
                      "La parole s'affichera dans la fenêtre de conversation. "
                      "[Affichage ON]");
}

}  // namespace

std::optional<TalkFilterRow> ReadTalkFilterCommand(std::string_view line) {
  for (const auto& [command, row] : kTalkFilterCommands) {
    if (ro::core::EqualsFolded(line, command)) {
      return row;
    }
  }
  return std::nullopt;
}

std::string ToggleTalkFilter(TalkFilterRow row, GameOptions& options, const OptionTargets& to,
                             std::uint32_t now_ms) {
  const bool hidden = !options.pages().talk_filter.IsOn(row);
  options.Change(ro::ui::OptionsWindowAction{.changed = TalkFilterOptions::IdOf(row), .on = hidden},
                 to, now_ms);
  return EchoOf(row, hidden);
}

}  // namespace moonclient

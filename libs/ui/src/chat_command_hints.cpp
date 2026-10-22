// Le filtrage et le clavier de la liste des commandes. Voir l'en-tête.

#include "ro/ui/chat_command_hints.h"

#include <algorithm>

#include "ro/core/ascii.h"
#include "ro/core/search_key.h"

namespace ro::ui {
namespace {

constexpr std::string_view kSigils = "/@";
constexpr std::string_view kBlanks = " \t";
constexpr char kAliasSeparator = ' ';
constexpr int kRowsShown = static_cast<int>(kChatCommandRowsShown);

// La première graphie de `aliases` qui commence par `typed`, ou une vue vide.
[[nodiscard]] std::string_view AliasStartingWith(std::string_view aliases, std::string_view typed) {
  while (!aliases.empty()) {
    const std::size_t end = aliases.find(kAliasSeparator);
    const std::string_view alias = aliases.substr(0, end);
    if (!alias.empty() && core::AsciiStartsWithFolded(alias, typed)) {
      return alias;
    }
    if (end == std::string_view::npos) {
      break;
    }
    aliases.remove_prefix(end + 1);
  }
  return {};
}

// `word` est-il l'une des graphies de `aliases`, casse latine indifférente ?
[[nodiscard]] bool IsAlias(std::string_view aliases, std::string_view word) {
  while (!aliases.empty()) {
    const std::size_t end = aliases.find(kAliasSeparator);
    if (core::AsciiEqualsFolded(aliases.substr(0, end), word)) {
      return true;
    }
    if (end == std::string_view::npos) {
      break;
    }
    aliases.remove_prefix(end + 1);
  }
  return false;
}

// La rangée choisie, ramenée dans la fenêtre visible.
void KeepChosenShown(ChatCommandListState& state, int count) {
  if (state.chosen < state.first_shown) {
    state.first_shown = state.chosen;
  } else if (state.chosen >= state.first_shown + kRowsShown) {
    state.first_shown = state.chosen - kRowsShown + 1;
  }
  state.first_shown = std::clamp(state.first_shown, 0, std::max(0, count - kRowsShown));
}

// Complète avec la rangée `row` et ferme la liste sur la ligne obtenue.
//
// ⚠ SAUF UNE COMMANDE QUI DÉCLARE LES CHOIX DE SON ARGUMENT : la ligne
// complétée « @jobchange » les propose aussitôt, sans attendre une frappe.
[[nodiscard]] std::string Complete(ChatCommandListState& state, const ChatCommandListView& view,
                                   std::span<const ChatCommandHint> hints, std::size_t row) {
  std::string line;
  if (view.argument_of.has_value()) {
    const ChatCommandHint& hint = hints[*view.argument_of];
    line = view.argument_head;
    line += hint.argument_choices[view.choice_rows[row]].value;
    state.dismissed = line;
  } else {
    const ChatCommandMatch& match = view.matches[row];
    const ChatCommandHint& hint = hints[match.hint];
    line = CompletedChatCommand(hint, match);
    state.dismissed = hint.argument_choices.empty() ? line : std::string();
  }
  state.navigated = false;
  return line;
}

}  // namespace

std::size_t ChatCommandRowCount(const ChatCommandListView& view) noexcept {
  return view.argument_of.has_value() ? view.choice_rows.size() : view.matches.size();
}

std::optional<std::string_view> ChatCommandQuery(std::string_view line) {
  if (line.empty() || kSigils.find(line.front()) == std::string_view::npos ||
      line.find_first_of(kBlanks) != std::string_view::npos) {
    return std::nullopt;
  }
  return line;
}

std::vector<ChatCommandMatch> MatchChatCommands(std::span<const ChatCommandHint> hints,
                                                std::string_view query) {
  std::vector<ChatCommandMatch> matches;
  if (query.empty()) {
    return matches;
  }
  const char sigil = query.front();
  const std::string_view typed = query.substr(1);
  std::vector<bool> taken(hints.size(), false);
  for (std::size_t rank = 0; rank < hints.size(); ++rank) {
    const ChatCommandHint& hint = hints[rank];
    if (hint.sigil == sigil && core::AsciiStartsWithFolded(hint.name, typed)) {
      matches.push_back(ChatCommandMatch{.hint = rank, .spelled = hint.name});
      taken[rank] = true;
    }
  }
  // ⚠ LES ALIAS SEULEMENT QUAND ON A TAPÉ QUELQUE CHOSE : sur un sigil seul,
  // chaque commande paraîtrait une seconde fois sous son premier alias.
  if (typed.empty()) {
    return matches;
  }
  for (std::size_t rank = 0; rank < hints.size(); ++rank) {
    const ChatCommandHint& hint = hints[rank];
    if (taken[rank] || hint.sigil != sigil) {
      continue;
    }
    if (const std::string_view alias = AliasStartingWith(hint.aliases, typed); !alias.empty()) {
      matches.push_back(ChatCommandMatch{.hint = rank, .spelled = alias, .by_alias = true});
    }
  }
  return matches;
}

std::optional<ChatArgumentQuery> ChatArgumentQueryOf(std::span<const ChatCommandHint> hints,
                                                     std::string_view line) {
  if (line.empty() || kSigils.find(line.front()) == std::string_view::npos) {
    return std::nullopt;
  }
  const std::size_t blank = line.find_first_of(kBlanks);
  if (blank == std::string_view::npos) {
    return std::nullopt;
  }
  const std::string_view word = line.substr(1, blank - 1);
  std::size_t typed_start = line.find_first_not_of(kBlanks, blank);
  if (typed_start == std::string_view::npos) {
    typed_start = line.size();
  }
  for (std::size_t rank = 0; rank < hints.size(); ++rank) {
    const ChatCommandHint& hint = hints[rank];
    if (hint.sigil != line.front() || hint.argument_choices.empty()) {
      continue;
    }
    if (core::AsciiEqualsFolded(hint.name, word) || IsAlias(hint.aliases, word)) {
      return ChatArgumentQuery{
          .hint = rank, .head = line.substr(0, typed_start), .typed = line.substr(typed_start)};
    }
  }
  return std::nullopt;
}

std::vector<std::size_t> MatchChatArguments(std::span<const ChatArgumentChoice> choices,
                                            std::string_view typed) {
  std::vector<std::size_t> matches;
  const std::string typed_key = core::SearchKey(typed);
  for (std::size_t rank = 0; rank < choices.size(); ++rank) {
    const ChatArgumentChoice& choice = choices[rank];
    if (core::AsciiStartsWithFolded(choice.value, typed) ||
        core::SearchKeyContains(choice.search_key, typed_key)) {
      matches.push_back(rank);
    }
  }
  return matches;
}

ChatCommandListView FollowChatCommandLine(ChatCommandListState& state,
                                          std::span<const ChatCommandHint> hints,
                                          std::string_view line) {
  ChatCommandListView view;
  if (!state.dismissed.empty() && line != state.dismissed) {
    state.dismissed.clear();
  }
  const std::optional<std::string_view> query = ChatCommandQuery(line);
  const std::optional<ChatArgumentQuery> argument =
      query.has_value() ? std::nullopt : ChatArgumentQueryOf(hints, line);
  if ((!query.has_value() && !argument.has_value()) || !state.dismissed.empty()) {
    state.query.clear();
    state.navigated = false;
    return view;
  }
  if (query.has_value()) {
    view.matches = MatchChatCommands(hints, *query);
  } else {
    view.argument_of = argument->hint;
    view.argument_head = std::string(argument->head);
    view.choice_rows = MatchChatArguments(hints[argument->hint].argument_choices, argument->typed);
  }
  if (line != state.query) {
    state.query = std::string(line);
    state.chosen = 0;
    state.first_shown = 0;
    state.navigated = false;
  }
  const int count = static_cast<int>(ChatCommandRowCount(view));
  state.chosen = std::clamp(state.chosen, 0, std::max(0, count - 1));
  KeepChosenShown(state, count);
  view.open = count > 0;
  return view;
}

std::string CompletedChatCommand(const ChatCommandHint& hint, const ChatCommandMatch& match) {
  std::string line(1, hint.sigil);
  line += match.spelled;
  if (hint.takes_argument) {
    line += ' ';
  }
  return line;
}

ChatCommandKeyAnswer PressChatCommandKey(ChatCommandListState& state,
                                         const ChatCommandListView& view,
                                         std::span<const ChatCommandHint> hints,
                                         std::string_view line, ChatCommandKey key) {
  ChatCommandKeyAnswer answer;
  if (!view.open || ChatCommandRowCount(view) == 0) {
    return answer;
  }
  const int count = static_cast<int>(ChatCommandRowCount(view));
  switch (key) {
    case ChatCommandKey::kUp:
    case ChatCommandKey::kDown:
      // ⚠ LE PREMIER APPUI CHOISIT LA PREMIÈRE RANGÉE, déjà en surbrillance :
      // ↓ part de là, ↑ fait le tour par la fin.
      state.chosen = (state.chosen + (key == ChatCommandKey::kUp ? count - 1 : 1)) % count;
      state.navigated = true;
      KeepChosenShown(state, count);
      answer.consumed = true;
      break;
    case ChatCommandKey::kEnter:
      if (!state.navigated) {
        break;
      }
      [[fallthrough]];
    case ChatCommandKey::kTab:
      answer.line = Complete(state, view, hints, static_cast<std::size_t>(state.chosen));
      answer.consumed = true;
      break;
    case ChatCommandKey::kEscape:
      state.dismissed = std::string(line);
      state.navigated = false;
      answer.consumed = true;
      break;
  }
  return answer;
}

std::string PickChatCommand(ChatCommandListState& state, const ChatCommandListView& view,
                            std::span<const ChatCommandHint> hints, std::size_t row) {
  if (row >= ChatCommandRowCount(view)) {
    return {};
  }
  return Complete(state, view, hints, row);
}

void DismissChatCommandList(ChatCommandListState& state, std::string_view line) {
  state.dismissed = std::string(line);
  state.navigated = false;
}

std::string_view FirstHelpLine(std::string_view help) { return help.substr(0, help.find('\n')); }

}  // namespace ro::ui

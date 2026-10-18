// Le filtrage et le clavier de la liste des commandes. Voir l'en-tête.

#include "ro/ui/chat_command_hints.h"

#include <algorithm>

#include "ro/core/ascii.h"

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
[[nodiscard]] std::string Complete(ChatCommandListState& state, const ChatCommandListView& view,
                                   std::span<const ChatCommandHint> hints, std::size_t row) {
  const ChatCommandMatch& match = view.matches[row];
  std::string line = CompletedChatCommand(hints[match.hint], match);
  state.dismissed = line;
  state.navigated = false;
  return line;
}

}  // namespace

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

ChatCommandListView FollowChatCommandLine(ChatCommandListState& state,
                                          std::span<const ChatCommandHint> hints,
                                          std::string_view line) {
  ChatCommandListView view;
  if (!state.dismissed.empty() && line != state.dismissed) {
    state.dismissed.clear();
  }
  const std::optional<std::string_view> query = ChatCommandQuery(line);
  if (!query.has_value() || !state.dismissed.empty()) {
    state.query.clear();
    state.navigated = false;
    return view;
  }
  view.matches = MatchChatCommands(hints, *query);
  if (*query != state.query) {
    state.query = std::string(*query);
    state.chosen = 0;
    state.first_shown = 0;
    state.navigated = false;
  }
  const int count = static_cast<int>(view.matches.size());
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
  if (!view.open || view.matches.empty()) {
    return answer;
  }
  const int count = static_cast<int>(view.matches.size());
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
  if (row >= view.matches.size()) {
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

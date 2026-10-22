// La liste des derniers correspondants : son filtre et son clavier. Voir
// l'en-tête.

#include "ro/ui/chat_whisper_list.h"

#include "ro/core/search_key.h"

namespace ro::ui {
namespace {

// Un octet de suite UTF-8 : 10xx xxxx. Il ne commence jamais un caractère.
constexpr unsigned char kContinuationMask = 0xC0;
constexpr unsigned char kContinuationBits = 0x80;

[[nodiscard]] bool StartsCharacter(std::string_view text, std::size_t at) {
  return at >= text.size() ||
         (static_cast<unsigned char>(text[at]) & kContinuationMask) != kContinuationBits;
}

// `needle` se trouve dans `haystack`, À UNE FRONTIÈRE DE CARACTÈRE aux deux
// bouts. ⚠ UNE RECHERCHE D'OCTETS NUE trouverait un octet de suite isolé au
// milieu d'un caractère coréen, et dirait qu'il y est.
[[nodiscard]] bool ContainsWholeCharacters(std::string_view haystack, std::string_view needle) {
  for (std::size_t at = haystack.find(needle); at != std::string_view::npos;
       at = haystack.find(needle, at + 1)) {
    if (StartsCharacter(haystack, at) && StartsCharacter(haystack, at + needle.size())) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::vector<std::size_t> MatchRecentWhispers(std::span<const ChatRecentWhisper> names,
                                             std::string_view typed) {
  const std::string wanted = ro::core::SearchKey(typed);
  std::vector<std::size_t> rows;
  for (std::size_t rank = 0; rank < names.size(); ++rank) {
    if (ContainsWholeCharacters(ro::core::SearchKey(names[rank].name), wanted)) {
      rows.push_back(rank);
    }
  }
  return rows;
}

void OpenWhisperList(ChatWhisperListState& state, std::string_view typed) {
  if (state.quiet_next) {
    state.quiet_next = false;
    return;
  }
  state.open = true;
  state.chosen = kNoWhisperChosen;
  state.opened_on = typed;
  state.filtered_on.clear();
  state.escaped_on.reset();
}

void CloseWhisperList(ChatWhisperListState& state) {
  state.open = false;
  state.chosen = kNoWhisperChosen;
  state.escaped_on.reset();
}

ChatWhisperListView FollowWhisperList(ChatWhisperListState& state,
                                      std::span<const ChatRecentWhisper> names,
                                      std::string_view typed) {
  ChatWhisperListView view;
  // Une frappe après Échap rouvre : c'est un nouveau nom qu'on cherche.
  if (state.escaped_on.has_value() && *state.escaped_on != typed) {
    state.escaped_on.reset();
    state.open = true;
  }
  const std::string_view filter =
      typed == state.opened_on ? std::string_view() : std::string_view(typed);
  if (filter != state.filtered_on) {
    state.filtered_on = filter;
    state.chosen = kNoWhisperChosen;
  }
  view.rows = MatchRecentWhispers(names, filter);
  if (state.chosen >= static_cast<int>(view.rows.size())) {
    state.chosen = kNoWhisperChosen;
  }
  view.open = state.open && !view.rows.empty();
  return view;
}

ChatWhisperKeyAnswer PressWhisperListKey(ChatWhisperListState& state,
                                         const ChatWhisperListView& view, std::string_view typed,
                                         ChatWhisperKey key) {
  ChatWhisperKeyAnswer answer;
  if (!view.open) {
    return answer;
  }
  const int count = static_cast<int>(view.rows.size());
  switch (key) {
    case ChatWhisperKey::kDown:
      // ⚠ EN BOUCLE, dans les deux sens : dix noms au plus, et le dernier est
      // à une touche du premier.
      state.chosen = state.chosen == kNoWhisperChosen ? 0 : (state.chosen + 1) % count;
      answer.consumed = true;
      break;
    case ChatWhisperKey::kUp:
      state.chosen = state.chosen <= 0 ? count - 1 : state.chosen - 1;
      answer.consumed = true;
      break;
    case ChatWhisperKey::kEnter:
    case ChatWhisperKey::kTab:
      // ⚠ SANS NOM SURLIGNÉ, LA TOUCHE GARDE SON RÔLE : Entrée envoie, Tab passe
      // à la phrase.
      if (state.chosen == kNoWhisperChosen) {
        break;
      }
      answer.picked = view.rows[static_cast<std::size_t>(state.chosen)];
      answer.consumed = true;
      CloseWhisperList(state);
      break;
    case ChatWhisperKey::kEscape:
      CloseWhisperList(state);
      state.escaped_on = std::string(typed);
      answer.consumed = true;
      break;
  }
  return answer;
}

}  // namespace ro::ui

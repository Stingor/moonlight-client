// UNE LIGNE QUI NOUS NOMME. Voir l'en-tête.

#include "ro/sim/chat_mention.h"

#include <algorithm>

#include "ro/core/case_fold.h"

namespace ro::sim {
namespace {

// Les mots-clés se tapent séparés par un point-virgule.
constexpr char kWordSeparator = ';';
// En deçà, un mot-clé s'allumerait sur la moitié des lignes du fil.
constexpr std::size_t kShortestMentionWord = 2;
// Ce qui sépare l'auteur de ce qu'il dit, dans une ligne de parole.
constexpr std::string_view kSpeakerSeparator = " : ";
// Le premier octet qui n'est pas de l'ASCII.
constexpr unsigned char kFirstNonAscii = 0x80U;

[[nodiscard]] bool IsBlank(char letter) {
  return letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r';
}

[[nodiscard]] std::string_view Trimmed(std::string_view text) {
  while (!text.empty() && IsBlank(text.front())) {
    text.remove_prefix(1);
  }
  while (!text.empty() && IsBlank(text.back())) {
    text.remove_suffix(1);
  }
  return text;
}

// UN OCTET QUI CONTINUE UN MOT : lettre ou chiffre ASCII, tiret bas, et tout
// octet non ASCII — un « é » colle au mot qu'il termine, et « Anaé » n'est pas
// « Ana ».
[[nodiscard]] bool ContinuesWord(char letter) {
  const auto byte = static_cast<unsigned char>(letter);
  return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
         (byte >= '0' && byte <= '9') || byte == '_' || byte >= kFirstNonAscii;
}

// LE MOT ENTIER, dans un texte et un mot déjà repliés : la casse ne compte
// plus, et les bornes du mot se lisent sur le texte replié.
[[nodiscard]] bool FoldedTextMentions(std::string_view folded_text, std::string_view folded_word) {
  if (folded_word.empty()) {
    return false;
  }
  for (std::size_t at = folded_text.find(folded_word); at != std::string_view::npos;
       at = folded_text.find(folded_word, at + 1)) {
    const std::size_t past = at + folded_word.size();
    const bool starts_word = at == 0 || !ContinuesWord(folded_text[at - 1]);
    const bool ends_word = past == folded_text.size() || !ContinuesWord(folded_text[past]);
    if (starts_word && ends_word) {
      return true;
    }
  }
  return false;
}

// LA LIGNE D'UN CANAL SANS SON ÉTIQUETTE : « [#main] Ana : … » commence par le
// nom du canal, entre crochets ou derrière un « # », suivi d'une espace. C'est
// derrière lui que se lit « Nom : ».
[[nodiscard]] std::string_view WithoutChannelTag(std::string_view text) {
  if (text.empty() || (text.front() != '[' && text.front() != '#')) {
    return text;
  }
  const std::size_t space = text.find(' ');
  if (space == std::string_view::npos) {
    return text;
  }
  if (text.front() == '[' && text[space - 1] != ']') {
    return text;
  }
  return text.substr(space + 1);
}

// Ce qui suit l'éventuelle étiquette de canal : là où l'auteur se nomme.
[[nodiscard]] std::string_view HeadedPart(const ChatLine& line) {
  const std::string_view text = line.text;
  return line.kind == ChatKind::kChannel ? WithoutChannelTag(text) : text;
}

// CE QUE LA LIGNE DIT, sans son auteur.
//
// ⚠ L'AUTEUR NE MENTIONNE PERSONNE : sans cette coupe, un mot-clé qui est un
// pseudo s'allumerait sur chaque ligne de ce joueur, et un « Ana Bis » qui
// parle citerait « Ana » à chaque phrase.
[[nodiscard]] std::string_view SaidPart(const ChatLine& line) {
  const std::string_view text = line.text;
  if (line.type == ChatType::kDiscord) {
    const std::string_view author = DiscordRelayAuthorName(text);
    if (!author.empty()) {
      // L'auteur est une vue sur `text` : ce qui suit son crochet fermant.
      const auto past_author =
          static_cast<std::size_t>(author.data() - text.data()) + author.size() + 1;
      return text.substr(past_author);
    }
    return text;
  }
  switch (line.kind) {
    case ChatKind::kPublic:
    case ChatKind::kParty:
    case ChatKind::kGuild:
    case ChatKind::kChannel:
    case ChatKind::kClan:
    case ChatKind::kBattlefield: {
      const std::string_view headed = HeadedPart(line);
      if (const std::size_t said = headed.find(kSpeakerSeparator); said != std::string_view::npos) {
        return headed.substr(said + kSpeakerSeparator.size());
      }
      return headed;
    }
    default:
      return text;
  }
}

// Les genres où quelqu'un PARLE. Le reste — serveur, sac, combat, expérience,
// appel, écho de sa propre voix — ne mentionne jamais personne.
[[nodiscard]] bool IsSpoken(const ChatLine& line) {
  switch (line.kind) {
    case ChatKind::kPublic:
    case ChatKind::kParty:
    case ChatKind::kGuild:
    case ChatKind::kChannel:
    case ChatKind::kClan:
    case ChatKind::kBattlefield:
    case ChatKind::kAnnouncement:
      return true;
    case ChatKind::kWhisper:
      return line.whisper.side == WhisperSide::kReceived;
    case ChatKind::kSelf:
    case ChatKind::kServer:
    case ChatKind::kItem:
    case ChatKind::kBattle:
    case ChatKind::kPartyBattle:
    case ChatKind::kExp:
    case ChatKind::kCall:
      return false;
  }
  return false;
}

}  // namespace

std::vector<std::string> SplitMentionWords(std::string_view typed) {
  std::vector<std::string> words;
  while (!typed.empty()) {
    const std::size_t separator = typed.find(kWordSeparator);
    const std::string_view word = Trimmed(typed.substr(0, separator));
    typed = separator == std::string_view::npos ? std::string_view{} : typed.substr(separator + 1);
    if (word.size() < kShortestMentionWord) {
      continue;
    }
    const bool known = std::ranges::any_of(
        words, [word](const std::string& kept) { return core::EqualsFolded(kept, word); });
    if (!known) {
      words.emplace_back(word);
    }
  }
  return words;
}

bool TextMentions(std::string_view text, std::string_view word) {
  return FoldedTextMentions(core::CaseFolded(text), core::CaseFolded(word));
}

bool IsOwnChatLine(const ChatLine& line, std::string_view own_name, std::uint32_t own_id) {
  if (line.kind == ChatKind::kSelf) {
    return true;
  }
  if (line.whisper.side == WhisperSide::kSent || line.whisper.side == WhisperSide::kUndelivered) {
    return true;
  }
  // ⚠ ZÉRO N'EST PERSONNE : c'est l'auteur de tout ce qui ne vient de personne.
  if (own_id != 0 && line.speaker_id == own_id) {
    return true;
  }
  if (own_name.empty()) {
    return false;
  }
  const std::string_view headed = HeadedPart(line);
  if (headed.size() > own_name.size() && headed.starts_with(own_name) &&
      headed.substr(own_name.size()).starts_with(kSpeakerSeparator)) {
    return true;
  }
  return line.type == ChatType::kDiscord && DiscordRelayAuthorName(line.text) == own_name;
}

std::string_view DiscordRelayAuthorName(std::string_view text) {
  if (text.empty() || text.front() != '[') {
    return {};
  }
  const std::size_t channel_end = text.find(']');
  if (channel_end == std::string_view::npos || channel_end + 1 >= text.size() ||
      text[channel_end + 1] != '[') {
    return {};
  }
  // Ce qui suit « ][ ».
  const std::size_t name_at = channel_end + 2;
  const std::size_t name_end = text.find(']', name_at);
  if (name_end == std::string_view::npos) {
    return {};
  }
  return text.substr(name_at, name_end - name_at);
}

bool MentionsMe(const ChatLine& line, const MentionRules& rules) {
  if (!IsSpoken(line) || IsOwnChatLine(line, rules.own_name, rules.own_id)) {
    return false;
  }
  // Le texte ne se replie qu'une fois, quel que soit le nombre de mots-clés.
  const std::string said = core::CaseFolded(SaidPart(line));
  if (!rules.own_name.empty() && FoldedTextMentions(said, core::CaseFolded(rules.own_name))) {
    return true;
  }
  return std::ranges::any_of(rules.words, [&said](const std::string& word) {
    return FoldedTextMentions(said, core::CaseFolded(word));
  });
}

}  // namespace ro::sim

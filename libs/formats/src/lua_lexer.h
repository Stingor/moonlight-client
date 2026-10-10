#pragma once

// LE DÉCOUPEUR : des octets vers des lexèmes, et rien de plus.
//
// ⚠ SÉPARÉ DU PARSEUR PARCE QUE LE FICHIER PASSAIT LE PLAFOND, et la coupe se
// lit : d'un côté ce qui transforme des octets en lexèmes, de l'autre ce qui
// transforme des lexèmes en tables. Les deux moitiés ne partagent qu'un
// gabarit de recherche.
//
// ⚠ ET IL N'EST PAS DANS `include/` : rien hors de cette bibliothèque n'a de
// raison de découper du Lua. Voir `lua_table_internal.h`, qui vit ici pour la
// même raison.

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "ro/core/error.h"

namespace ro::formats::lua_internal {

using core::Error;
using core::ErrorCode;

// Vrai si le mot figure dans la liste. ⚠ LES DEUX MOITIÉS S'EN SERVENT : le
// découpeur pour les mots réservés, le parseur pour les opérateurs.
template <std::size_t N>
bool Contains(const std::array<std::string_view, N>& words, std::string_view word) {
  for (const std::string_view candidate : words) {
    if (candidate == word) {
      return true;
    }
  }
  return false;
}

// Les mots réservés de Lua 5.1, tels que llex.c les liste. Un nom qui en fait
// partie n'est jamais un identifiant.
constexpr std::array<std::string_view, 21> kKeywords = {
    "and",   "break", "do",  "else", "elseif", "end",    "false", "for",  "function", "if",   "in",
    "local", "nil",   "not", "or",   "repeat", "return", "then",  "true", "until",    "while"};

// Les symboles à plusieurs caractères, du plus long au plus court : `...` doit
// se reconnaître avant `..`, et `..` avant `.`.
constexpr std::array<std::string_view, 6> kLongSymbols = {"...", "..", "==", "~=", "<=", ">="};
constexpr std::string_view kShortSymbols = "+-*/%^#<>=(){}[];:,.";

// Un « long crochet » `[[` ou `[==[` ouvre une chaîne ou un commentaire, que
// ferme le crochet portant le même nombre de signes égal. Au-delà de ce niveau,
// ce n'est plus un fichier de données.
constexpr int kMaxLongBracketLevel = 8;

// Une séquence `\ddd` ne code qu'un octet.
constexpr int kMaxDecimalEscape = 255;
constexpr int kMaxDecimalEscapeDigits = 3;

constexpr std::array<std::uint8_t, 3> kUtf8ByteOrderMark = {0xEF, 0xBB, 0xBF};

constexpr bool IsNameStart(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
constexpr bool IsDigit(char c) { return c >= '0' && c <= '9'; }
constexpr bool IsNameChar(char c) { return IsNameStart(c) || IsDigit(c); }
constexpr bool IsSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
constexpr bool IsNewline(char c) { return c == '\n' || c == '\r'; }

struct Token {
  enum class Kind : std::uint8_t { kEnd, kName, kKeyword, kNumber, kString, kSymbol };

  Kind kind = Kind::kEnd;
  // Le lexème : un nom, un mot réservé ou un symbole. Vide pour une chaîne.
  std::string_view text;
  double number = 0.0;
  // Les octets d'une chaîne, échappements résolus, encodage intact.
  std::string bytes;
  int line = 1;
};

// Découpe le source en lexèmes. Les blancs et les commentaires n'en sont pas.
class Lexer {
 public:
  explicit Lexer(std::string_view source) : source_(source) {}

  [[nodiscard]] core::Result<Token> Next() {
    if (auto skipped = SkipSpaceAndComments(); !skipped) {
      return std::unexpected(skipped.error());
    }
    Token token;
    token.line = line_;
    if (AtEnd()) {
      return token;
    }
    const char c = Peek();
    if (IsNameStart(c)) {
      const std::size_t start = at_;
      while (IsNameChar(Peek())) {
        Consume();
      }
      token.text = source_.substr(start, at_ - start);
      token.kind = Contains(kKeywords, token.text) ? Token::Kind::kKeyword : Token::Kind::kName;
      return token;
    }
    if (IsDigit(c) || (c == '.' && IsDigit(Peek(1)))) {
      return ReadNumber(token);
    }
    if (c == '"' || c == '\'') {
      return ReadQuotedString(token, c);
    }
    if (const int level = LongBracketLevel(); level >= 0) {
      auto bytes = ReadLongBracket(level);
      if (!bytes) {
        return std::unexpected(bytes.error());
      }
      token.kind = Token::Kind::kString;
      token.bytes = std::move(*bytes);
      return token;
    }
    for (const std::string_view symbol : kLongSymbols) {
      if (source_.substr(at_).starts_with(symbol)) {
        token.kind = Token::Kind::kSymbol;
        token.text = symbol;
        for (std::size_t index = 0; index < symbol.size(); ++index) {
          Consume();
        }
        return token;
      }
    }
    if (kShortSymbols.find(c) != std::string_view::npos) {
      token.kind = Token::Kind::kSymbol;
      token.text = source_.substr(at_, 1);
      Consume();
      return token;
    }
    return Fail("caractère inattendu");
  }

 private:
  [[nodiscard]] char Peek(std::size_t ahead = 0) const noexcept {
    return at_ + ahead < source_.size() ? source_[at_ + ahead] : '\0';
  }
  [[nodiscard]] bool AtEnd() const noexcept { return at_ >= source_.size(); }
  void Consume() noexcept {
    if (at_ < source_.size()) {
      if (source_[at_] == '\n') {
        ++line_;
      }
      ++at_;
    }
  }
  // Un saut de ligne, sous ses trois formes, compté une seule fois.
  void ConsumeNewline() noexcept {
    const char first = Peek();
    Consume();
    if (IsNewline(Peek()) && Peek() != first) {
      Consume();
    }
  }

  [[nodiscard]] std::unexpected<Error> Fail(std::string what) const {
    return std::unexpected(
        Error{ErrorCode::kCorruptData, "ligne " + std::to_string(line_) + " : " + what});
  }

  [[nodiscard]] core::Result<void> SkipSpaceAndComments() {
    if (at_ == 0 && source_.size() >= kUtf8ByteOrderMark.size() &&
        static_cast<std::uint8_t>(source_[0]) == kUtf8ByteOrderMark[0] &&
        static_cast<std::uint8_t>(source_[1]) == kUtf8ByteOrderMark[1] &&
        static_cast<std::uint8_t>(source_[2]) == kUtf8ByteOrderMark[2]) {
      at_ = kUtf8ByteOrderMark.size();
    }
    while (!AtEnd()) {
      if (IsSpace(Peek())) {
        Consume();
        continue;
      }
      if (Peek() != '-' || Peek(1) != '-') {
        return {};
      }
      Consume();
      Consume();
      if (const int level = LongBracketLevel(); level >= 0) {
        if (auto comment = ReadLongBracket(level); !comment) {
          return std::unexpected(comment.error());
        }
        continue;
      }
      while (!AtEnd() && !IsNewline(Peek())) {
        Consume();
      }
    }
    return {};
  }

  // Le niveau d'un long crochet ouvrant à la position courante — le nombre de
  // signes égal entre ses deux crochets — ou -1 si ce n'en est pas un.
  [[nodiscard]] int LongBracketLevel() const noexcept {
    if (Peek() != '[') {
      return -1;
    }
    int level = 0;
    while (Peek(static_cast<std::size_t>(level) + 1) == '=' && level <= kMaxLongBracketLevel) {
      ++level;
    }
    return Peek(static_cast<std::size_t>(level) + 1) == '[' ? level : -1;
  }

  [[nodiscard]] core::Result<std::string> ReadLongBracket(int level) {
    if (level > kMaxLongBracketLevel) {
      return Fail("long crochet d'un niveau que l'on ne lit pas");
    }
    for (int index = 0; index < level + 2; ++index) {
      Consume();
    }
    // Lua ignore un saut de ligne collé au crochet ouvrant.
    if (IsNewline(Peek())) {
      ConsumeNewline();
    }
    std::string bytes;
    while (!AtEnd()) {
      if (Peek() == ']') {
        int closing = 0;
        while (Peek(static_cast<std::size_t>(closing) + 1) == '=') {
          ++closing;
        }
        if (closing == level && Peek(static_cast<std::size_t>(closing) + 1) == ']') {
          for (int index = 0; index < level + 2; ++index) {
            Consume();
          }
          return bytes;
        }
      }
      bytes.push_back(Peek());
      Consume();
    }
    return Fail("long crochet sans fin");
  }

  [[nodiscard]] core::Result<Token> ReadQuotedString(Token token, char quote) {
    Consume();
    token.kind = Token::Kind::kString;
    while (true) {
      if (AtEnd() || IsNewline(Peek())) {
        return Fail("chaîne sans fin");
      }
      const char c = Peek();
      if (c == quote) {
        Consume();
        return token;
      }
      if (c != '\\') {
        token.bytes.push_back(c);
        Consume();
        continue;
      }
      Consume();
      if (AtEnd()) {
        return Fail("chaîne sans fin");
      }
      const char escaped = Peek();
      switch (escaped) {
        case 'a':
          token.bytes.push_back('\a');
          break;
        case 'b':
          token.bytes.push_back('\b');
          break;
        case 'f':
          token.bytes.push_back('\f');
          break;
        case 'n':
          token.bytes.push_back('\n');
          break;
        case 'r':
          token.bytes.push_back('\r');
          break;
        case 't':
          token.bytes.push_back('\t');
          break;
        case 'v':
          token.bytes.push_back('\v');
          break;
        case '\n':
        case '\r':
          token.bytes.push_back('\n');
          ConsumeNewline();
          continue;
        default:
          if (IsDigit(escaped)) {
            int value = 0;
            int digits = 0;
            while (IsDigit(Peek()) && digits < kMaxDecimalEscapeDigits) {
              value = value * 10 + (Peek() - '0');
              Consume();
              ++digits;
            }
            if (value > kMaxDecimalEscape) {
              return Fail("séquence \\ddd au-delà d'un octet");
            }
            token.bytes.push_back(static_cast<char>(value));
            continue;
          }
          // Tout autre caractère se garde tel quel : c'est ainsi que `\\`, `\"`
          // et `\'` se lisent, et Lua fait de même pour ce qu'il ne connaît pas.
          token.bytes.push_back(escaped);
          break;
      }
      Consume();
    }
  }

  // Lua lit d'abord des chiffres et des points, puis un exposant, puis tout ce
  // qui ressemble à un nom ; c'est ensuite la conversion qui tranche. On suit
  // le même découpage, pour refuser les mêmes textes que lui.
  [[nodiscard]] core::Result<Token> ReadNumber(Token token) {
    const std::size_t start = at_;
    while (IsDigit(Peek()) || Peek() == '.') {
      Consume();
    }
    if (Peek() == 'e' || Peek() == 'E') {
      Consume();
      if (Peek() == '+' || Peek() == '-') {
        Consume();
      }
    }
    while (IsNameChar(Peek())) {
      Consume();
    }
    const std::string_view text = source_.substr(start, at_ - start);
    token.kind = Token::Kind::kNumber;

    const bool hexadecimal =
        text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X');
    if (hexadecimal) {
      std::uint64_t value = 0;
      const auto [end, error] =
          std::from_chars(text.data() + 2, text.data() + text.size(), value, 16);
      if (error != std::errc() || end != text.data() + text.size()) {
        return Fail("nombre illisible : " + std::string(text));
      }
      token.number = static_cast<double>(value);
      return token;
    }
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), token.number);
    if (error != std::errc() || end != text.data() + text.size()) {
      return Fail("nombre illisible : " + std::string(text));
    }
    return token;
  }

  std::string_view source_;
  std::size_t at_ = 0;
  int line_ = 1;
};

}  // namespace ro::formats::lua_internal

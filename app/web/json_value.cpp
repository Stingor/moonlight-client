// LE LECTEUR JSON DU LOGIN WEB : une descente récursive, bornée.

#include "web/json_value.h"

#include <array>
#include <charconv>
#include <cmath>
#include <utility>

namespace moonclient::web {
namespace {

using ro::core::ErrorCode;
using ro::core::Fail;

// LES BORNES DU CODAGE UTF-16 QUE `\uXXXX` EMPLOIE.
//
// Un caractère hors du plan de base s'écrit en DEUX échappements : un
// substitut haut puis un substitut bas, qui se recombinent. Les lire
// séparément donnerait deux caractères de remplacement là où le site en a
// envoyé un.
constexpr std::uint32_t kHighSurrogateFirst = 0xD800;
constexpr std::uint32_t kHighSurrogateLast = 0xDBFF;
constexpr std::uint32_t kLowSurrogateFirst = 0xDC00;
constexpr std::uint32_t kLowSurrogateLast = 0xDFFF;
constexpr std::uint32_t kSupplementaryFirst = 0x10000;
constexpr std::uint32_t kSurrogateShift = 10;

// Les seuils de l'encodage UTF-8, et le caractère de remplacement pour ce qui
// n'a pas de sens — un substitut orphelin, par exemple.
constexpr std::uint32_t kOneByteLast = 0x7F;
constexpr std::uint32_t kTwoByteLast = 0x7FF;
constexpr std::uint32_t kThreeByteLast = 0xFFFF;
constexpr std::uint32_t kReplacement = 0xFFFD;

void AppendUtf8(std::uint32_t code_point, std::string& into) {
  if (code_point >= kHighSurrogateFirst && code_point <= kLowSurrogateLast) {
    // ⚠ UN SUBSTITUT SEUL N'EST PAS UN CARACTÈRE : le remplacer vaut mieux que
    // d'écrire une séquence UTF-8 que personne ne peut relire.
    code_point = kReplacement;
  }
  if (code_point <= kOneByteLast) {
    into.push_back(static_cast<char>(code_point));
    return;
  }
  if (code_point <= kTwoByteLast) {
    into.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
    into.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    return;
  }
  if (code_point <= kThreeByteLast) {
    into.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
    into.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
    into.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
    return;
  }
  into.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
  into.push_back(static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
  into.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
  into.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
}

// LE LECTEUR. Il tient sa position et sa profondeur, et rend une erreur qui dit
// OÙ — un message sans position est inutilisable sur une réponse de mille
// octets.
class Reader {
 public:
  explicit Reader(std::string_view text) : text_(text) {}

  [[nodiscard]] ro::core::Result<JsonValue> ReadDocument() {
    SkipBlanks();
    auto value = ReadValue(0);
    if (!value.has_value()) {
      return value;
    }
    SkipBlanks();
    if (at_ != text_.size()) {
      return Broken("des octets suivent la valeur");
    }
    return value;
  }

 private:
  [[nodiscard]] ro::core::Result<JsonValue> Broken(std::string_view why) const {
    return Fail(ErrorCode::kCorruptData,
                "JSON : " + std::string(why) + " (position " + std::to_string(at_) + ")");
  }

  void SkipBlanks() {
    while (at_ < text_.size() &&
           (text_[at_] == ' ' || text_[at_] == '\t' || text_[at_] == '\n' || text_[at_] == '\r')) {
      ++at_;
    }
  }

  [[nodiscard]] bool Eat(char expected) {
    if (at_ < text_.size() && text_[at_] == expected) {
      ++at_;
      return true;
    }
    return false;
  }

  [[nodiscard]] bool EatWord(std::string_view word) {
    if (text_.substr(at_, word.size()) != word) {
      return false;
    }
    at_ += word.size();
    return true;
  }

  [[nodiscard]] ro::core::Result<JsonValue> ReadValue(int depth) {
    if (depth >= kJsonMaxDepth) {
      return Broken("imbrication trop profonde");
    }
    if (at_ >= text_.size()) {
      return Broken("fin prématurée");
    }
    switch (text_[at_]) {
      case '{':
        return ReadObject(depth);
      case '[':
        return ReadArray(depth);
      case '"': {
        auto text = ReadString();
        if (!text.has_value()) {
          return std::unexpected(text.error());
        }
        return JsonValue(std::move(*text));
      }
      case 't':
        return EatWord("true") ? ro::core::Result<JsonValue>(JsonValue(true))
                               : Broken("mot inconnu");
      case 'f':
        return EatWord("false") ? ro::core::Result<JsonValue>(JsonValue(false))
                                : Broken("mot inconnu");
      case 'n':
        return EatWord("null") ? ro::core::Result<JsonValue>(JsonValue()) : Broken("mot inconnu");
      default:
        return ReadNumber();
    }
  }

  [[nodiscard]] ro::core::Result<JsonValue> ReadObject(int depth) {
    ++at_;  // '{'
    std::map<std::string, JsonValue> fields;
    SkipBlanks();
    if (Eat('}')) {
      return JsonValue::Object(std::move(fields));
    }
    while (true) {
      SkipBlanks();
      if (at_ >= text_.size() || text_[at_] != '"') {
        return Broken("une clé d'objet doit être une chaîne");
      }
      auto key = ReadString();
      if (!key.has_value()) {
        return std::unexpected(key.error());
      }
      SkipBlanks();
      if (!Eat(':')) {
        return Broken("deux-points attendus après une clé");
      }
      SkipBlanks();
      auto value = ReadValue(depth + 1);
      if (!value.has_value()) {
        return value;
      }
      // ⚠ LA DERNIÈRE OCCURRENCE L'EMPORTE, comme partout : une clé répétée
      // n'est pas interdite par le format, et refuser serait plus surprenant
      // que de faire ce que font les autres.
      fields.insert_or_assign(std::move(*key), std::move(*value));
      SkipBlanks();
      if (Eat(',')) {
        continue;
      }
      if (Eat('}')) {
        return JsonValue::Object(std::move(fields));
      }
      return Broken("virgule ou accolade fermante attendue");
    }
  }

  [[nodiscard]] ro::core::Result<JsonValue> ReadArray(int depth) {
    ++at_;  // '['
    std::vector<JsonValue> items;
    SkipBlanks();
    if (Eat(']')) {
      return JsonValue::Array(std::move(items));
    }
    while (true) {
      SkipBlanks();
      auto value = ReadValue(depth + 1);
      if (!value.has_value()) {
        return value;
      }
      items.push_back(std::move(*value));
      SkipBlanks();
      if (Eat(',')) {
        continue;
      }
      if (Eat(']')) {
        return JsonValue::Array(std::move(items));
      }
      return Broken("virgule ou crochet fermant attendu");
    }
  }

  [[nodiscard]] std::optional<std::uint32_t> ReadHexQuad() {
    if (at_ + 4 > text_.size()) {
      return std::nullopt;
    }
    std::uint32_t value = 0;
    for (int digit = 0; digit < 4; ++digit) {
      const char letter = text_[at_ + static_cast<std::size_t>(digit)];
      std::uint32_t nibble = 0;
      if (letter >= '0' && letter <= '9') {
        nibble = static_cast<std::uint32_t>(letter - '0');
      } else if (letter >= 'a' && letter <= 'f') {
        nibble = static_cast<std::uint32_t>(letter - 'a') + 10;
      } else if (letter >= 'A' && letter <= 'F') {
        nibble = static_cast<std::uint32_t>(letter - 'A') + 10;
      } else {
        return std::nullopt;
      }
      value = (value << 4U) | nibble;
    }
    at_ += 4;
    return value;
  }

  [[nodiscard]] ro::core::Result<std::string> ReadString() {
    ++at_;  // '"'
    std::string out;
    while (at_ < text_.size()) {
      const char letter = text_[at_];
      if (letter == '"') {
        ++at_;
        return out;
      }
      if (letter != '\\') {
        // ⚠ LES OCTETS BRUTS PASSENT TELS QUELS : ce qui n'est pas échappé est
        // déjà de l'UTF-8, et le réencoder serait le casser.
        out.push_back(letter);
        ++at_;
        continue;
      }
      ++at_;
      if (at_ >= text_.size()) {
        return std::unexpected(Broken("échappement tronqué").error());
      }
      const char escape = text_[at_++];
      switch (escape) {
        case '"':
        case '\\':
        case '/':
          out.push_back(escape);
          break;
        case 'b':
          out.push_back('\b');
          break;
        case 'f':
          out.push_back('\f');
          break;
        case 'n':
          out.push_back('\n');
          break;
        case 'r':
          out.push_back('\r');
          break;
        case 't':
          out.push_back('\t');
          break;
        case 'u': {
          const std::optional<std::uint32_t> first = ReadHexQuad();
          if (!first.has_value()) {
            return std::unexpected(Broken("échappement \\u mal formé").error());
          }
          std::uint32_t code_point = *first;
          // 🔴 LES DEUX MOITIÉS D'UN CARACTÈRE HORS DU PLAN DE BASE. Le site
          // échappe tout ce qui n'est pas ASCII ; un pseudonyme qui porte un
          // emoji arrive en DEUX `\u`, et les lire séparément rendrait deux
          // losanges.
          if (code_point >= kHighSurrogateFirst && code_point <= kHighSurrogateLast &&
              text_.substr(at_, 2) == "\\u") {
            const std::size_t before = at_;
            at_ += 2;
            const std::optional<std::uint32_t> second = ReadHexQuad();
            if (second.has_value() && *second >= kLowSurrogateFirst &&
                *second <= kLowSurrogateLast) {
              code_point =
                  kSupplementaryFirst + (((code_point - kHighSurrogateFirst) << kSurrogateShift) |
                                         (*second - kLowSurrogateFirst));
            } else {
              // Pas une paire : on rend la seconde moitié à la suite, qui la
              // traitera pour ce qu'elle est.
              at_ = before;
            }
          }
          AppendUtf8(code_point, out);
          break;
        }
        default:
          return std::unexpected(Broken("échappement inconnu").error());
      }
    }
    return std::unexpected(Broken("chaîne non terminée").error());
  }

  [[nodiscard]] ro::core::Result<JsonValue> ReadNumber() {
    const std::size_t start = at_;
    if (at_ < text_.size() && (text_[at_] == '-' || text_[at_] == '+')) {
      ++at_;
    }
    while (at_ < text_.size() &&
           ((text_[at_] >= '0' && text_[at_] <= '9') || text_[at_] == '.' || text_[at_] == 'e' ||
            text_[at_] == 'E' || text_[at_] == '-' || text_[at_] == '+')) {
      ++at_;
    }
    if (at_ == start) {
      return Broken("valeur inattendue");
    }
    double number = 0.0;
    const char* first = text_.data() + start;
    const char* last = text_.data() + at_;
    const std::from_chars_result read = std::from_chars(first, last, number);
    if (read.ec != std::errc() || read.ptr != last) {
      return Broken("nombre mal formé");
    }
    return JsonValue(number);
  }

  std::string_view text_;
  std::size_t at_ = 0;
};

}  // namespace

JsonValue JsonValue::Array(std::vector<JsonValue> items) {
  JsonValue value;
  value.kind_ = Kind::kArray;
  value.items_ = std::move(items);
  return value;
}

JsonValue JsonValue::Object(std::map<std::string, JsonValue> fields) {
  JsonValue value;
  value.kind_ = Kind::kObject;
  value.fields_ = std::move(fields);
  return value;
}

const JsonValue* JsonValue::Field(std::string_view name) const {
  if (kind_ != Kind::kObject) {
    return nullptr;
  }
  const auto found = fields_.find(std::string(name));
  return found == fields_.end() ? nullptr : &found->second;
}

std::string_view JsonValue::Text() const noexcept {
  return kind_ == Kind::kString ? std::string_view(text_) : std::string_view();
}

bool JsonValue::Truth(bool fallback) const noexcept {
  return kind_ == Kind::kBool ? truth_ : fallback;
}

std::optional<std::int64_t> JsonValue::Whole() const noexcept {
  if (kind_ != Kind::kNumber) {
    return std::nullopt;
  }
  // ⚠ ENTIER, ET DANS LES BORNES : un nombre à virgule ou hors de ce qu'un
  // entier de soixante-quatre bits porte n'est pas un identifiant de compte.
  double integral = 0.0;
  if (std::modf(number_, &integral) != 0.0) {
    return std::nullopt;
  }
  if (integral < -9.2e18 || integral > 9.2e18) {
    return std::nullopt;
  }
  return static_cast<std::int64_t>(integral);
}

std::string_view JsonValue::TextAt(std::string_view name) const {
  const JsonValue* field = Field(name);
  return field == nullptr ? std::string_view() : field->Text();
}

bool JsonValue::TruthAt(std::string_view name, bool fallback) const {
  const JsonValue* field = Field(name);
  return field == nullptr ? fallback : field->Truth(fallback);
}

std::int64_t JsonValue::WholeAt(std::string_view name, std::int64_t fallback) const {
  const JsonValue* field = Field(name);
  if (field == nullptr) {
    return fallback;
  }
  return field->Whole().value_or(fallback);
}

ro::core::Result<JsonValue> ParseJson(std::string_view text) {
  Reader reader(text);
  return reader.ReadDocument();
}

}  // namespace moonclient::web

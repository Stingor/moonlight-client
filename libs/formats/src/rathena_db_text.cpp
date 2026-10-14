// Le curseur des bases rAthena écrites sous forme stricte. Voir l'en-tête.

#include "ro/formats/rathena_db_text.h"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <utility>

namespace ro::formats {
namespace {

constexpr std::string_view kHeaderLine = "Header:";
constexpr std::string_view kBodyLine = "Body:";
constexpr std::string_view kEmptyBodyLine = "Body: []";
constexpr std::string_view kTypeKey = "Type";
// Les retraits de la forme : un champ d'en-tête, l'ouverture d'une entrée, un
// champ, une ligne de bloc ou de suite.
constexpr std::string_view kHeaderIndent = "  ";
constexpr std::string_view kEntryStart = "  - ";
constexpr std::string_view kFieldIndent = "    ";
constexpr std::string_view kNestedIndent = "      ";
constexpr std::string_view kListItem = "      - ";
constexpr std::string_view kKeepBlock = "|+";
constexpr std::string_view kStripBlock = "|-";
constexpr std::string_view kClipBlock = "|";
constexpr std::string_view kEmptyMap = "{}";
constexpr std::string_view kEmptyList = "[]";
// Les bornes des points de code d'un `\uXXXX`.
constexpr char32_t kHighSurrogateFirst = 0xD800;
constexpr char32_t kLowSurrogateFirst = 0xDC00;
constexpr char32_t kLowSurrogateEnd = 0xE000;
constexpr char32_t kSurrogateSpan = 0x400;
constexpr char32_t kSupplementaryFirst = 0x10000;
constexpr int kSurrogateShift = 10;
constexpr std::size_t kHexDigits = 4;
constexpr int kHexBase = 16;
constexpr int kDecimalDigits = 10;
// L'encodage UTF-8 : bornes des longueurs, préfixes et masques.
constexpr char32_t kOneByteEnd = 0x80;
constexpr char32_t kTwoBytesEnd = 0x800;
constexpr char32_t kThreeBytesEnd = 0x10000;
constexpr unsigned kTwoBytesLead = 0xC0;
constexpr unsigned kThreeBytesLead = 0xE0;
constexpr unsigned kFourBytesLead = 0xF0;
constexpr unsigned kContinuation = 0x80;
constexpr unsigned kSixBits = 0x3F;
constexpr int kSixBitShift = 6;

[[nodiscard]] bool StartsWith(std::string_view line, std::string_view prefix) {
  return line.substr(0, prefix.size()) == prefix;
}

[[nodiscard]] std::string_view Trimmed(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
    text.remove_prefix(1);
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
    text.remove_suffix(1);
  }
  return text;
}

[[nodiscard]] bool IsKeyLetter(char letter) {
  return (letter >= 'A' && letter <= 'Z') || (letter >= 'a' && letter <= 'z') ||
         (letter >= '0' && letter <= '9') || letter == '_';
}

void AppendUtf8(std::string& text, char32_t point) {
  if (point < kOneByteEnd) {
    text += static_cast<char>(point);
  } else if (point < kTwoBytesEnd) {
    text += static_cast<char>(kTwoBytesLead | (point >> kSixBitShift));
    text += static_cast<char>(kContinuation | (point & kSixBits));
  } else if (point < kThreeBytesEnd) {
    text += static_cast<char>(kThreeBytesLead | (point >> (2 * kSixBitShift)));
    text += static_cast<char>(kContinuation | ((point >> kSixBitShift) & kSixBits));
    text += static_cast<char>(kContinuation | (point & kSixBits));
  } else {
    text += static_cast<char>(kFourBytesLead | (point >> (3 * kSixBitShift)));
    text += static_cast<char>(kContinuation | ((point >> (2 * kSixBitShift)) & kSixBits));
    text += static_cast<char>(kContinuation | ((point >> kSixBitShift) & kSixBits));
    text += static_cast<char>(kContinuation | (point & kSixBits));
  }
}

// La lecture : les lignes, une à une, et la position où elle s'arrête.
class Reader {
 public:
  Reader(std::string_view text, std::string_view label) : text_(text), label_(label) {}

  [[nodiscard]] core::Error Refused(std::string_view what) const {
    return core::Error{core::ErrorCode::kCorruptData, std::string(label_) + " : ligne " +
                                                          std::to_string(line_number_) + " : " +
                                                          std::string(what)};
  }

  // La ligne suivante, sans la consommer.
  [[nodiscard]] std::optional<std::string_view> Peek() const {
    if (at_ >= text_.size()) {
      return std::nullopt;
    }
    std::size_t end = text_.find('\n', at_);
    if (end == std::string_view::npos) {
      end = text_.size();
    }
    std::string_view line = text_.substr(at_, end - at_);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    return line;
  }

  void Skip() {
    const std::size_t end = text_.find('\n', at_);
    at_ = end == std::string_view::npos ? text_.size() : end + 1;
    ++line_number_;
  }

  // Les lignes vides et les commentaires, sautés.
  void SkipBlank() {
    while (const std::optional<std::string_view> line = Peek()) {
      const std::string_view kept = Trimmed(*line);
      if (!kept.empty() && kept.front() != '#') {
        return;
      }
      Skip();
    }
  }

 private:
  std::string_view text_;
  std::string_view label_;
  std::size_t at_ = 0;
  int line_number_ = 1;
};

// Un `\uXXXX`, à partir du `u`.
[[nodiscard]] bool ReadHex(std::string_view text, std::size_t& at, char32_t& unit) {
  if (at + kHexDigits > text.size()) {
    return false;
  }
  unit = 0;
  for (std::size_t digit = 0; digit < kHexDigits; ++digit) {
    const char letter = text[at + digit];
    int value = 0;
    if (letter >= '0' && letter <= '9') {
      value = letter - '0';
    } else if (letter >= 'a' && letter <= 'f') {
      value = letter - 'a' + kDecimalDigits;
    } else if (letter >= 'A' && letter <= 'F') {
      value = letter - 'A' + kDecimalDigits;
    } else {
      return false;
    }
    unit = unit * kHexBase + static_cast<char32_t>(value);
  }
  at += kHexDigits;
  return true;
}

// Un texte à la JSON qui commence à `at` ; `at` passe le guillemet fermant.
[[nodiscard]] bool ReadQuoted(std::string_view text, std::size_t& at, std::string& out) {
  if (at >= text.size() || text[at] != '"') {
    return false;
  }
  ++at;
  while (at < text.size()) {
    const char letter = text[at++];
    if (letter == '"') {
      return true;
    }
    if (letter != '\\') {
      out += letter;
      continue;
    }
    if (at >= text.size()) {
      return false;
    }
    switch (text[at++]) {
      case '"':
        out += '"';
        break;
      case '\\':
        out += '\\';
        break;
      case '/':
        out += '/';
        break;
      case 'b':
        out += '\b';
        break;
      case 'f':
        out += '\f';
        break;
      case 'n':
        out += '\n';
        break;
      case 'r':
        out += '\r';
        break;
      case 't':
        out += '\t';
        break;
      case 'u': {
        char32_t unit = 0;
        if (!ReadHex(text, at, unit)) {
          return false;
        }
        if (unit >= kHighSurrogateFirst && unit < kLowSurrogateFirst) {
          char32_t low = 0;
          if (at + 2 > text.size() || text[at] != '\\' || text[at + 1] != 'u') {
            return false;
          }
          at += 2;
          if (!ReadHex(text, at, low) || low < kLowSurrogateFirst || low >= kLowSurrogateEnd) {
            return false;
          }
          unit = kSupplementaryFirst + (unit - kHighSurrogateFirst) * kSurrogateSpan +
                 (low - kLowSurrogateFirst);
        }
        AppendUtf8(out, unit);
        break;
      }
      default:
        return false;
    }
  }
  return false;
}

// Une table d'une ligne, `{k: v, …}` entière.
[[nodiscard]] bool ReadFlowMap(std::string_view text, std::vector<RathenaPair>& pairs) {
  if (text.size() < kEmptyMap.size() || text.front() != '{' || text.back() != '}') {
    return false;
  }
  std::size_t at = 1;
  const std::size_t end = text.size() - 1;
  const auto skip_spaces = [&] {
    while (at < end && text[at] == ' ') {
      ++at;
    }
  };
  skip_spaces();
  while (at < end) {
    const std::size_t key_start = at;
    while (at < end && IsKeyLetter(text[at])) {
      ++at;
    }
    if (at == key_start || at + 1 >= end || text[at] != ':' || text[at + 1] != ' ') {
      return false;
    }
    RathenaPair pair{.key = text.substr(key_start, at - key_start), .value = {}};
    at += 2;
    if (at < end && text[at] == '"') {
      if (!ReadQuoted(text, at, pair.value)) {
        return false;
      }
    } else {
      const std::size_t value_start = at;
      while (at < end && text[at] != ',') {
        ++at;
      }
      pair.value = std::string(Trimmed(text.substr(value_start, at - value_start)));
    }
    pairs.push_back(std::move(pair));
    skip_spaces();
    if (at < end) {
      if (text[at] != ',') {
        return false;
      }
      ++at;
      skip_spaces();
    }
  }
  return true;
}

// Le contenu d'un bloc, ses lignes à six espaces jusqu'au premier champ.
void ReadBlock(Reader& reader, std::string_view header, std::string& out) {
  std::vector<std::string_view> lines;
  while (const std::optional<std::string_view> line = reader.Peek()) {
    if (!line->empty() && !StartsWith(*line, kNestedIndent)) {
      break;
    }
    lines.push_back(line->empty() ? std::string_view() : line->substr(kNestedIndent.size()));
    reader.Skip();
  }
  // `|+` garde les lignes vides finales ; `|` et `|-` les retirent.
  if (header != kKeepBlock) {
    while (!lines.empty() && lines.back().empty()) {
      lines.pop_back();
    }
  }
  for (std::size_t rank = 0; rank < lines.size(); ++rank) {
    if (rank > 0) {
      out += '\n';
    }
    out += lines[rank];
  }
  if (header != kStripBlock && !lines.empty()) {
    out += '\n';
  }
}

// Un champ, depuis sa clé ; ce qui le prolonge sur les lignes suivantes aussi.
[[nodiscard]] bool ReadField(Reader& reader, std::string_view line, RathenaField& field) {
  std::size_t at = 0;
  while (at < line.size() && IsKeyLetter(line[at])) {
    ++at;
  }
  if (at == 0 || at >= line.size() || line[at] != ':') {
    return false;
  }
  field.key = line.substr(0, at);
  const std::string_view rest = line.substr(at + 1);
  if (rest.empty()) {
    // UNE SUITE DE TABLES, une par ligne.
    field.shape = RathenaShape::kList;
    while (const std::optional<std::string_view> item = reader.Peek()) {
      if (!StartsWith(*item, kListItem)) {
        break;
      }
      field.list.emplace_back();
      if (!ReadFlowMap(Trimmed(item->substr(kListItem.size())), field.list.back())) {
        return false;
      }
      reader.Skip();
    }
    return true;
  }
  if (rest.front() != ' ') {
    return false;
  }
  const std::string_view value = Trimmed(rest);
  if (value == kClipBlock || value == kStripBlock || value == kKeepBlock) {
    ReadBlock(reader, value, field.text);
    return true;
  }
  if (value == kEmptyList) {
    field.shape = RathenaShape::kList;
    return true;
  }
  if (!value.empty() && value.front() == '{') {
    field.shape = RathenaShape::kMap;
    return ReadFlowMap(value, field.map);
  }
  if (!value.empty() && value.front() == '"') {
    std::size_t quoted = 0;
    return ReadQuoted(value, quoted, field.text) && quoted == value.size();
  }
  field.text = std::string(value);
  return true;
}

}  // namespace

core::Result<void> ReadRathenaDb(std::string_view text, std::string_view expected_type,
                                 std::string_view file_label,
                                 const std::function<void(const RathenaEntry&)>& visit) {
  Reader reader(text, file_label);
  reader.SkipBlank();
  if (reader.Peek() != kHeaderLine) {
    return std::unexpected(reader.Refused("« Header: » attendu"));
  }
  reader.Skip();
  bool typed = false;
  while (const std::optional<std::string_view> line = reader.Peek()) {
    if (!StartsWith(*line, kHeaderIndent)) {
      break;
    }
    RathenaField field;
    if (!ReadField(reader, line->substr(kHeaderIndent.size()), field)) {
      return std::unexpected(reader.Refused("champ d'en-tête illisible"));
    }
    typed = typed || (field.key == kTypeKey && field.text == expected_type);
    reader.Skip();
  }
  if (!typed) {
    return std::unexpected(reader.Refused("ce n'est pas une base " + std::string(expected_type)));
  }
  reader.SkipBlank();
  if (reader.Peek() == kEmptyBodyLine) {
    return {};
  }
  if (reader.Peek() != kBodyLine) {
    return std::unexpected(reader.Refused("« Body: » attendu"));
  }
  reader.Skip();
  RathenaEntry entry;
  bool open = false;
  while (const std::optional<std::string_view> line = reader.Peek()) {
    const std::string_view kept = Trimmed(*line);
    if (kept.empty() || kept.front() == '#') {
      reader.Skip();
      continue;
    }
    std::string_view field_line;
    if (StartsWith(*line, kEntryStart)) {
      if (open) {
        visit(entry);
      }
      entry.clear();
      open = true;
      field_line = line->substr(kEntryStart.size());
    } else if (open && StartsWith(*line, kFieldIndent) && !StartsWith(*line, kNestedIndent)) {
      field_line = line->substr(kFieldIndent.size());
    } else {
      return std::unexpected(reader.Refused("ligne hors de la forme stricte"));
    }
    // ⚠ LA LIGNE SE CONSOMME AVANT LE CHAMP : un bloc ou une suite lisent les
    // suivantes.
    reader.Skip();
    entry.emplace_back();
    if (!ReadField(reader, field_line, entry.back())) {
      return std::unexpected(reader.Refused("champ illisible"));
    }
  }
  if (open) {
    visit(entry);
  }
  return {};
}

}  // namespace ro::formats

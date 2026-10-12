// Le texte d'`item_info.yml`. Voir l'en-tête.

#include "ro/vfs/item_info_text.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace ro::vfs {
namespace {

constexpr std::string_view kVersionKey = "version";
constexpr std::string_view kItemsKey = "items";
constexpr std::string_view kNameField = "name";
constexpr std::string_view kResourceField = "resource";
constexpr std::string_view kSlotsField = "slots";
constexpr std::string_view kViewField = "view";
constexpr std::string_view kCostumeField = "costume";
// L'effet de l'objet au sol : la table le porte, ce client ne le montre pas.
constexpr std::string_view kEffectField = "effect";
constexpr std::string_view kDescriptionField = "description";
constexpr std::string_view kTrue = "true";
constexpr std::string_view kFalse = "false";

// Les bornes de l'UTF-16 qu'un `\uXXXX` écrit, et celles de l'UTF-8 qu'on en
// tire.
constexpr std::size_t kHexDigits = 4;
constexpr unsigned kHexBase = 16;
constexpr unsigned kDecimalBase = 10;
constexpr unsigned kHexLetterValue = 10;
constexpr char32_t kHighSurrogateFirst = 0xD800;
constexpr char32_t kLowSurrogateFirst = 0xDC00;
constexpr char32_t kLowSurrogateLast = 0xDFFF;
constexpr char32_t kSurrogateSpan = 0x400;
constexpr char32_t kSupplementaryFirst = 0x10000;
constexpr char32_t kOneByteLast = 0x7F;
constexpr char32_t kTwoBytesLast = 0x7FF;
constexpr char32_t kThreeBytesLast = 0xFFFF;
constexpr unsigned kContinuationBits = 6;
constexpr char32_t kContinuationMask = 0x3F;
constexpr char32_t kContinuationMark = 0x80;
constexpr char32_t kTwoBytesMark = 0xC0;
constexpr char32_t kThreeBytesMark = 0xE0;
constexpr char32_t kFourBytesMark = 0xF0;

void AppendUtf8(char32_t code_point, std::string& text) {
  const auto byte = [&text](char32_t value) { text.push_back(static_cast<char>(value)); };
  const auto tail = [&byte](char32_t value, unsigned shift) {
    byte(kContinuationMark | ((value >> shift) & kContinuationMask));
  };
  if (code_point <= kOneByteLast) {
    byte(code_point);
  } else if (code_point <= kTwoBytesLast) {
    byte(kTwoBytesMark | (code_point >> kContinuationBits));
    tail(code_point, 0);
  } else if (code_point <= kThreeBytesLast) {
    byte(kThreeBytesMark | (code_point >> (2 * kContinuationBits)));
    tail(code_point, kContinuationBits);
    tail(code_point, 0);
  } else {
    byte(kFourBytesMark | (code_point >> (3 * kContinuationBits)));
    tail(code_point, 2 * kContinuationBits);
    tail(code_point, kContinuationBits);
    tail(code_point, 0);
  }
}

class TableText {
 public:
  explicit TableText(std::string_view text) : text_(text) {}

  // Le refus, avec la ligne où la lecture s'est arrêtée.
  [[nodiscard]] core::Error Refused(std::string_view what) const {
    const auto line =
        std::count(text_.begin(), text_.begin() + static_cast<std::ptrdiff_t>(at_), '\n');
    return core::Error{
        core::ErrorCode::kCorruptData,
        "item_info.yml, ligne " + std::to_string(line + 1) + " : " + std::string(what)};
  }

  // Saute les blancs, les fins de ligne et les commentaires.
  void SkipBlank() {
    while (at_ < text_.size()) {
      const char c = text_[at_];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        ++at_;
      } else if (c == '#') {
        const std::size_t end = text_.find('\n', at_);
        at_ = end == std::string_view::npos ? text_.size() : end;
      } else {
        return;
      }
    }
  }

  [[nodiscard]] bool AtEnd() {
    SkipBlank();
    return at_ == text_.size();
  }

  // Le caractère attendu, après les blancs.
  [[nodiscard]] bool Take(char expected) {
    SkipBlank();
    if (at_ < text_.size() && text_[at_] == expected) {
      ++at_;
      return true;
    }
    return false;
  }

  // Un nom de champ : des minuscules et des soulignés.
  [[nodiscard]] bool ReadKey(std::string_view& key) {
    SkipBlank();
    const std::size_t start = at_;
    while (at_ < text_.size() && ((text_[at_] >= 'a' && text_[at_] <= 'z') || text_[at_] == '_')) {
      ++at_;
    }
    key = text_.substr(start, at_ - start);
    return !key.empty();
  }

  // Un entier sans signe, au plus `most`.
  [[nodiscard]] bool ReadWhole(std::uint32_t most, std::uint32_t& value) {
    SkipBlank();
    const std::size_t start = at_;
    std::uint64_t read = 0;
    while (at_ < text_.size() && text_[at_] >= '0' && text_[at_] <= '9') {
      read = read * kDecimalBase + static_cast<std::uint64_t>(text_[at_] - '0');
      if (read > most) {
        return false;
      }
      ++at_;
    }
    value = static_cast<std::uint32_t>(read);
    return at_ > start;
  }

  [[nodiscard]] bool ReadBoolean(bool& value) {
    std::string_view word;
    if (!ReadKey(word) || (word != kTrue && word != kFalse)) {
      return false;
    }
    value = word == kTrue;
    return true;
  }

  // Un texte entre guillemets doubles, ses échappements résolus.
  [[nodiscard]] bool ReadQuoted(std::string& text) {
    if (!Take('"')) {
      return false;
    }
    text.clear();
    while (at_ < text_.size()) {
      const std::size_t plain_end = text_.find_first_of("\"\\\n", at_);
      if (plain_end == std::string_view::npos) {
        return false;
      }
      text.append(text_.substr(at_, plain_end - at_));
      at_ = plain_end;
      const char stop = text_[at_++];
      if (stop == '"') {
        return true;
      }
      if (stop == '\n' || !ReadEscape(text)) {
        return false;
      }
    }
    return false;
  }

 private:
  [[nodiscard]] bool ReadEscape(std::string& text) {
    if (at_ >= text_.size()) {
      return false;
    }
    const char escaped = text_[at_++];
    switch (escaped) {
      case '"':
      case '\\':
      case '/':
        text.push_back(escaped);
        return true;
      case 'b':
        text.push_back('\b');
        return true;
      case 'f':
        text.push_back('\f');
        return true;
      case 'n':
        text.push_back('\n');
        return true;
      case 'r':
        text.push_back('\r');
        return true;
      case 't':
        text.push_back('\t');
        return true;
      case 'u':
        return ReadCodePoint(text);
      default:
        return false;
    }
  }

  // Le `XXXX` d'un `\uXXXX`, et le second d'une paire de substitution.
  [[nodiscard]] bool ReadHex(char32_t& unit) {
    if (text_.size() - at_ < kHexDigits) {
      return false;
    }
    unit = 0;
    for (std::size_t digit = 0; digit < kHexDigits; ++digit) {
      const char c = text_[at_++];
      unsigned value = 0;
      if (c >= '0' && c <= '9') {
        value = static_cast<unsigned>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        value = static_cast<unsigned>(c - 'a') + kHexLetterValue;
      } else if (c >= 'A' && c <= 'F') {
        value = static_cast<unsigned>(c - 'A') + kHexLetterValue;
      } else {
        return false;
      }
      unit = unit * kHexBase + value;
    }
    return true;
  }

  [[nodiscard]] bool ReadCodePoint(std::string& text) {
    char32_t unit = 0;
    if (!ReadHex(unit) || (unit >= kLowSurrogateFirst && unit <= kLowSurrogateLast)) {
      return false;
    }
    if (unit >= kHighSurrogateFirst && unit < kLowSurrogateFirst) {
      char32_t low = 0;
      if (text_.substr(at_, 2) != "\\u") {
        return false;
      }
      at_ += 2;
      if (!ReadHex(low) || low < kLowSurrogateFirst || low > kLowSurrogateLast) {
        return false;
      }
      unit = kSupplementaryFirst + (unit - kHighSurrogateFirst) * kSurrogateSpan +
             (low - kLowSurrogateFirst);
    }
    AppendUtf8(unit, text);
    return true;
  }

  std::string_view text_;
  std::size_t at_ = 0;
};

// Une suite `[…]` de textes.
[[nodiscard]] bool ReadLines(TableText& table, std::vector<std::string>& lines) {
  if (!table.Take('[')) {
    return false;
  }
  if (table.Take(']')) {
    return true;
  }
  do {
    std::string line;
    if (!table.ReadQuoted(line)) {
      return false;
    }
    lines.push_back(std::move(line));
  } while (table.Take(','));
  return table.Take(']');
}

// Un champ d'objet, après son nom et ses deux-points.
[[nodiscard]] bool ReadField(TableText& table, std::string_view field, ItemInfoRecord& record) {
  std::uint32_t number = 0;
  if (field == kNameField) {
    return table.ReadQuoted(record.name);
  }
  if (field == kResourceField) {
    return table.ReadQuoted(record.resource);
  }
  if (field == kSlotsField) {
    const bool read = table.ReadWhole(std::numeric_limits<std::uint8_t>::max(), number);
    record.slots = static_cast<std::uint8_t>(number);
    return read;
  }
  if (field == kViewField) {
    const bool read = table.ReadWhole(std::numeric_limits<std::uint16_t>::max(), number);
    record.view = static_cast<std::uint16_t>(number);
    return read;
  }
  if (field == kCostumeField) {
    return table.ReadBoolean(record.costume);
  }
  if (field == kEffectField) {
    return table.ReadWhole(std::numeric_limits<std::uint32_t>::max(), record.effect);
  }
  if (field == kDescriptionField) {
    return ReadLines(table, record.description);
  }
  return false;
}

// `{<champ>: <valeur>, …}`.
[[nodiscard]] core::Result<void> ReadRecord(TableText& table, ItemInfoRecord& record) {
  if (!table.Take('{')) {
    return std::unexpected(table.Refused("un objet s'ouvre par « { »"));
  }
  if (table.Take('}')) {
    return {};
  }
  do {
    std::string_view field;
    if (!table.ReadKey(field) || !table.Take(':')) {
      return std::unexpected(table.Refused("nom de champ attendu"));
    }
    if (!ReadField(table, field, record)) {
      return std::unexpected(
          table.Refused("champ « " + std::string(field) + " » inconnu ou mal formé"));
    }
  } while (table.Take(','));
  if (!table.Take('}')) {
    return std::unexpected(table.Refused("un objet se ferme par « } »"));
  }
  return {};
}

}  // namespace

core::Result<void> ReadItemInfoText(
    std::string_view text, const std::function<core::Result<void>(ItemInfoRecord&&)>& take) {
  TableText table(text);
  std::string_view key;
  std::uint32_t version = 0;
  if (!table.ReadKey(key) || key != kVersionKey || !table.Take(':') ||
      !table.ReadWhole(std::numeric_limits<std::uint32_t>::max(), version)) {
    return std::unexpected(table.Refused("« version » attendue en tête"));
  }
  if (version != kItemInfoVersion) {
    return std::unexpected(table.Refused("version non gérée"));
  }
  if (!table.ReadKey(key) || key != kItemsKey || !table.Take(':') || !table.Take('{')) {
    return std::unexpected(table.Refused("« items: { » attendu"));
  }
  bool closed = table.Take('}');
  while (!closed) {
    ItemInfoRecord record;
    if (!table.ReadWhole(std::numeric_limits<std::uint32_t>::max(), record.item_id) ||
        !table.Take(':')) {
      return std::unexpected(table.Refused("identifiant d'objet attendu"));
    }
    if (auto read = ReadRecord(table, record); !read) {
      return read;
    }
    if (auto taken = take(std::move(record)); !taken) {
      return taken;
    }
    // Une virgule avant la fermeture est du YAML valide : elle se tolère.
    const bool more = table.Take(',');
    closed = table.Take('}');
    if (!more && !closed) {
      return std::unexpected(table.Refused("« , » ou « } » attendu après un objet"));
    }
  }
  if (!table.AtEnd()) {
    return std::unexpected(table.Refused("texte après la fermeture de « items »"));
  }
  return {};
}

std::string_view ItemInfoHeader(std::string_view text) {
  std::size_t at = 0;
  while (at < text.size()) {
    const std::size_t end = text.find('\n', at);
    const std::size_t next = end == std::string_view::npos ? text.size() : end + 1;
    const std::string_view line = text.substr(at, next - at);
    const std::size_t first = line.find_first_not_of(" \t\r\n");
    if (first != std::string_view::npos && line[first] != '#') {
      break;
    }
    at = next;
  }
  return text.substr(0, at);
}

namespace {

// Le dernier caractère de contrôle : en deçà, tout s'écrit en `\u00xx`.
constexpr unsigned char kLastControl = 0x1F;
constexpr std::string_view kHexDigitsLower = "0123456789abcdef";
constexpr unsigned kNibbleBits = 4;
constexpr unsigned kNibbleMask = 0xF;

// Un texte entre guillemets, comme `json.dumps(…, ensure_ascii=False)` l'écrit.
void AppendQuoted(std::string_view text, std::string& out) {
  out.push_back('"');
  for (const char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      default:
        if (static_cast<unsigned char>(c) <= kLastControl) {
          const auto code = static_cast<unsigned>(static_cast<unsigned char>(c));
          out += "\\u00";
          out.push_back(kHexDigitsLower[(code >> kNibbleBits) & kNibbleMask]);
          out.push_back(kHexDigitsLower[code & kNibbleMask]);
        } else {
          out.push_back(c);
        }
    }
  }
  out.push_back('"');
}

// Un objet, `{…}` compris, ses champs non nuls dans l'ordre du script.
void AppendRecord(const ItemInfoRecord& record, std::string& out) {
  bool first = true;
  const auto field = [&out, &first](std::string_view key) {
    out += first ? "" : ", ";
    first = false;
    out += key;
    out += ": ";
  };
  out.push_back('{');
  if (!record.name.empty()) {
    field(kNameField);
    AppendQuoted(record.name, out);
  }
  if (!record.resource.empty()) {
    field(kResourceField);
    AppendQuoted(record.resource, out);
  }
  if (record.slots != 0) {
    field(kSlotsField);
    out += std::to_string(record.slots);
  }
  if (record.view != 0) {
    field(kViewField);
    out += std::to_string(record.view);
  }
  if (record.costume) {
    field(kCostumeField);
    out += kTrue;
  }
  if (record.effect != 0) {
    field(kEffectField);
    out += std::to_string(record.effect);
  }
  if (!record.description.empty()) {
    field(kDescriptionField);
    out.push_back('[');
    for (std::size_t rank = 0; rank < record.description.size(); ++rank) {
      out += rank == 0 ? "" : ", ";
      AppendQuoted(record.description[rank], out);
    }
    out.push_back(']');
  }
  out.push_back('}');
}

}  // namespace

std::string WriteItemInfoText(std::string_view header, std::span<const ItemInfoRecord> records) {
  std::vector<const ItemInfoRecord*> ordered;
  ordered.reserve(records.size());
  for (const ItemInfoRecord& record : records) {
    ordered.push_back(&record);
  }
  std::ranges::stable_sort(ordered, {},
                           [](const ItemInfoRecord* record) { return record->item_id; });
  std::string out(header);
  out += kVersionKey;
  out += ": ";
  out += std::to_string(kItemInfoVersion);
  out += "\n";
  out += kItemsKey;
  out += ": {\n";
  for (std::size_t rank = 0; rank < ordered.size(); ++rank) {
    out += "  ";
    out += std::to_string(ordered[rank]->item_id);
    out += ": ";
    AppendRecord(*ordered[rank], out);
    out += rank + 1 == ordered.size() ? "\n" : ",\n";
  }
  out += "}\n";
  return out;
}

}  // namespace ro::vfs

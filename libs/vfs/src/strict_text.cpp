// La forme stricte des grosses tables YAML. Voir l'en-tête.

#include "strict_text.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ro::vfs {
namespace {

constexpr std::string_view kTrueWord = "true";
constexpr std::string_view kFalseWord = "false";

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

}  // namespace

core::Error StrictText::Refused(std::string_view what) const {
  const auto line =
      std::count(text_.begin(), text_.begin() + static_cast<std::ptrdiff_t>(at_), '\n');
  return core::Error{
      core::ErrorCode::kCorruptData,
      std::string(file_label_) + ", ligne " + std::to_string(line + 1) + " : " + std::string(what)};
}

void StrictText::SkipBlank() {
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

bool StrictText::AtEnd() {
  SkipBlank();
  return at_ == text_.size();
}

bool StrictText::Take(char expected) {
  SkipBlank();
  if (at_ < text_.size() && text_[at_] == expected) {
    ++at_;
    return true;
  }
  return false;
}

bool StrictText::ReadKey(std::string_view& key) {
  SkipBlank();
  const std::size_t start = at_;
  while (at_ < text_.size() && ((text_[at_] >= 'a' && text_[at_] <= 'z') || text_[at_] == '_')) {
    ++at_;
  }
  key = text_.substr(start, at_ - start);
  return !key.empty();
}

bool StrictText::ReadWhole(std::uint32_t most, std::uint32_t& value) {
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

bool StrictText::ReadBoolean(bool& value) {
  std::string_view word;
  if (!ReadKey(word)) {
    return false;
  }
  value = word == kTrueWord;
  return value || word == kFalseWord;
}

bool StrictText::ReadQuoted(std::string& text) {
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

bool StrictText::ReadQuotedList(std::vector<std::string>& lines) {
  if (!Take('[')) {
    return false;
  }
  if (Take(']')) {
    return true;
  }
  do {
    std::string line;
    if (!ReadQuoted(line)) {
      return false;
    }
    lines.push_back(std::move(line));
  } while (Take(','));
  return Take(']');
}

bool StrictText::ReadWholeList(std::uint32_t most, std::vector<std::uint32_t>& values) {
  if (!Take('[')) {
    return false;
  }
  if (Take(']')) {
    return true;
  }
  do {
    std::uint32_t value = 0;
    if (!ReadWhole(most, value)) {
      return false;
    }
    values.push_back(value);
  } while (Take(','));
  return Take(']');
}

bool StrictText::ReadEscape(std::string& text) {
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

bool StrictText::ReadHex(char32_t& unit) {
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

bool StrictText::ReadCodePoint(std::string& text) {
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

}  // namespace ro::vfs

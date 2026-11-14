#include "ro/ui/color_code.h"

namespace ro::ui {
namespace {

constexpr std::string_view kHexDigits = "0123456789ABCDEF";
// Le préfixe qu'on écrit, et ceux qu'on accepte de lire en plus.
constexpr std::string_view kWrittenPrefix = "0x";
constexpr std::string_view kShoutedPrefix = "0X";
constexpr char kWebMark = '#';
constexpr char kDialogMark = '^';
constexpr std::size_t kChannelDigits = 2;
constexpr int kNibbleBits = 4;
constexpr unsigned kNibbleMask = 0xFU;
constexpr int kDecimalTen = 10;

[[nodiscard]] int FromHex(char digit) {
  if (digit >= '0' && digit <= '9') {
    return digit - '0';
  }
  if (digit >= 'a' && digit <= 'f') {
    return digit - 'a' + kDecimalTen;
  }
  if (digit >= 'A' && digit <= 'F') {
    return digit - 'A' + kDecimalTen;
  }
  return -1;
}

[[nodiscard]] std::string_view Trimmed(std::string_view text) {
  constexpr std::string_view kSpaces = " \t\r\n";
  const std::size_t first = text.find_first_not_of(kSpaces);
  if (first == std::string_view::npos) {
    return {};
  }
  const std::size_t last = text.find_last_not_of(kSpaces);
  return text.substr(first, last - first + 1);
}

// Le code sans son préfixe, s'il en porte un.
[[nodiscard]] std::string_view WithoutPrefix(std::string_view text) {
  if (text.starts_with(kWrittenPrefix) || text.starts_with(kShoutedPrefix)) {
    text.remove_prefix(kWrittenPrefix.size());
  } else if (text.starts_with(kWebMark) || text.starts_with(kDialogMark)) {
    text.remove_prefix(1);
  }
  return text;
}

}  // namespace

std::array<char, kColorCodeSize> FormatColorCode(std::array<std::uint8_t, 3> rgb) {
  std::array<char, kColorCodeSize> code{};
  std::size_t at = 0;
  for (const char letter : kWrittenPrefix) {
    code[at++] = letter;
  }
  for (const unsigned channel : rgb) {
    code[at++] = kHexDigits[(channel >> kNibbleBits) & kNibbleMask];
    code[at++] = kHexDigits[channel & kNibbleMask];
  }
  return code;
}

std::optional<std::array<std::uint8_t, 3>> ParseColorCode(std::string_view text) {
  const std::string_view digits = WithoutPrefix(Trimmed(text));
  std::array<std::uint8_t, 3> rgb{};
  if (digits.size() != rgb.size() * kChannelDigits) {
    return std::nullopt;
  }
  for (std::size_t channel = 0; channel < rgb.size(); ++channel) {
    const int high = FromHex(digits[channel * kChannelDigits]);
    const int low = FromHex(digits[channel * kChannelDigits + 1]);
    if (high < 0 || low < 0) {
      return std::nullopt;
    }
    rgb[channel] = static_cast<std::uint8_t>((high << kNibbleBits) | low);
  }
  return rgb;
}

}  // namespace ro::ui

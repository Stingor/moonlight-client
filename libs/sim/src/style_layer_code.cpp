#include "ro/sim/style_layer_code.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>

namespace ro::sim {
namespace {

constexpr std::string_view kPrefix = "L1:";
constexpr char kSeparator = ':';
// Les chiffres qu'on écrit, en minuscules comme le code v7 : un même style doit
// toujours s'écrire de la même façon.
constexpr std::string_view kHexDigits = "0123456789abcdef";
constexpr int kNibbleBits = 4;
constexpr int kByteBits = 8;
constexpr unsigned kNibbleMask = 0xFU;
constexpr unsigned kByteMask = 0xFFU;
constexpr int kDecimalTen = 10;
// Le masque : seize bits, quatre chiffres.
constexpr std::size_t kMaskDigits = 4;
// Un réglage : la teinte sur deux octets, puis la saturation, la luminosité et
// « absolu » sur un octet chacun, deux chiffres par octet.
constexpr std::size_t kByteDigits = 2;
constexpr std::size_t kHueDigits = 2 * kByteDigits;
constexpr std::size_t kSaturationAt = kHueDigits;
constexpr std::size_t kValueAt = kSaturationAt + kByteDigits;
constexpr std::size_t kAbsoluteAt = kValueAt + kByteDigits;
constexpr std::size_t kAdjustDigits = kAbsoluteAt + kByteDigits;

// Les bornes d'un réglage, celles du serveur.
constexpr int kHueTurn = 359;
constexpr int kPercentMost = 100;

void AppendByte(std::string& out, unsigned value) {
  out.push_back(kHexDigits[(value >> kNibbleBits) & kNibbleMask]);
  out.push_back(kHexDigits[value & kNibbleMask]);
}

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

// Un nombre hexadécimal de `digits` chiffres lu en tête de `text`, ou rien.
[[nodiscard]] std::optional<unsigned> ReadHex(std::string_view text, std::size_t digits) {
  if (text.size() < digits) {
    return std::nullopt;
  }
  unsigned value = 0;
  for (std::size_t at = 0; at < digits; ++at) {
    const int nibble = FromHex(text[at]);
    if (nibble < 0) {
      return std::nullopt;
    }
    value = (value << kNibbleBits) | static_cast<unsigned>(nibble);
  }
  return value;
}

// Un petit entier décimal, champ entier.
[[nodiscard]] std::optional<int> ReadNumber(std::string_view field) {
  int value = 0;
  const char* first = field.data();
  const char* last = first + field.size();
  const std::from_chars_result read = std::from_chars(first, last, value);
  if (field.empty() || read.ec != std::errc{} || read.ptr != last) {
    return std::nullopt;
  }
  return value;
}

[[nodiscard]] bool Neutral(const StyleAdjust& adjust) {
  return !adjust.absolute && adjust.hue == 0 && adjust.saturation == 0 && adjust.value == 0;
}

// Un réglage que le serveur accepterait.
[[nodiscard]] bool InBounds(const StyleAdjust& adjust) {
  const int least = adjust.absolute ? 0 : -kHueTurn;
  const int saturation_least = adjust.absolute ? 0 : -kPercentMost;
  return adjust.hue >= least && adjust.hue <= kHueTurn && adjust.saturation >= saturation_least &&
         adjust.saturation <= kPercentMost && adjust.value >= -kPercentMost &&
         adjust.value <= kPercentMost;
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

// Le champ jusqu'au prochain séparateur, retiré de `text`, ou rien.
[[nodiscard]] std::optional<std::string_view> TakeField(std::string_view& text) {
  const std::size_t end = text.find(kSeparator);
  if (end == std::string_view::npos) {
    return std::nullopt;
  }
  const std::string_view field = text.substr(0, end);
  text.remove_prefix(end + 1);
  return field;
}

}  // namespace

std::size_t StyleLayerRanks(StyleLayerKind layer, std::uint8_t profile) noexcept {
  // ⚠ PAS D'AIGUILLAGE SUR LE CALQUE : un calque qu'un code ne porte pas — la
  // suite du corps, qui voyage avec lui — rend zéro, et le code est refusé.
  const bool ramps = profile == kStyleLayerCodeRampProfile;
  if (layer == StyleLayerKind::kSkin) {
    return ramps ? 1 : 0;
  }
  if (layer == StyleLayerKind::kHead) {
    return ramps ? kStyleLayerCodeRampRanks : 0;
  }
  if (layer == StyleLayerKind::kHeadgear || layer == StyleLayerKind::kGarment) {
    if (ramps) {
      return kStyleLayerCodeRampRanks;
    }
    return profile == kStyleLayerCodeHueClassProfile ? kStyleLayerAdjustCount : 0;
  }
  return 0;
}

std::string EncodeStyleLayerCode(const StyleLayerCode& code) {
  const std::size_t ranks = StyleLayerRanks(code.layer, code.profile);
  unsigned mask = 0;
  for (std::size_t rank = 0; rank < ranks; ++rank) {
    if (!Neutral(code.adjusts[rank])) {
      mask |= 1U << rank;
    }
  }
  std::string out(kPrefix);
  out += std::to_string(static_cast<unsigned>(code.layer));
  out.push_back(kSeparator);
  out += std::to_string(static_cast<unsigned>(code.profile));
  out.push_back(kSeparator);
  AppendByte(out, (mask >> kByteBits) & kByteMask);
  AppendByte(out, mask & kByteMask);
  out.push_back(kSeparator);
  for (std::size_t rank = 0; rank < ranks; ++rank) {
    if ((mask & (1U << rank)) == 0) {
      continue;
    }
    const StyleAdjust& adjust = code.adjusts[rank];
    const auto hue = static_cast<std::uint16_t>(adjust.hue);
    AppendByte(out, (hue >> kByteBits) & kByteMask);
    AppendByte(out, hue & kByteMask);
    AppendByte(out, static_cast<std::uint8_t>(adjust.saturation));
    AppendByte(out, static_cast<std::uint8_t>(adjust.value));
    AppendByte(out, adjust.absolute ? 1U : 0U);
  }
  return out;
}

std::optional<StyleLayerCode> DecodeStyleLayerCode(std::string_view text) {
  text = Trimmed(text);
  if (!text.starts_with(kPrefix)) {
    return std::nullopt;
  }
  text.remove_prefix(kPrefix.size());
  const std::optional<std::string_view> layer_field = TakeField(text);
  const std::optional<std::string_view> profile_field = TakeField(text);
  const std::optional<std::string_view> mask_field = TakeField(text);
  if (!layer_field || !profile_field || !mask_field || mask_field->size() != kMaskDigits) {
    return std::nullopt;
  }
  const std::optional<int> layer = ReadNumber(*layer_field);
  const std::optional<int> profile = ReadNumber(*profile_field);
  const std::optional<unsigned> mask = ReadHex(*mask_field, kMaskDigits);
  if (!layer || !profile || !mask || *profile < 0 || *profile > static_cast<int>(kByteMask)) {
    return std::nullopt;
  }
  if (*layer < static_cast<int>(StyleLayerKind::kHead) ||
      *layer > static_cast<int>(StyleLayerKind::kSkin)) {
    return std::nullopt;
  }
  StyleLayerCode code;
  code.layer = static_cast<StyleLayerKind>(*layer);
  code.profile = static_cast<std::uint8_t>(*profile);
  const std::size_t ranks = StyleLayerRanks(code.layer, code.profile);
  if (ranks == 0 || (*mask >> ranks) != 0) {
    return std::nullopt;
  }
  for (std::size_t rank = 0; rank < ranks; ++rank) {
    if ((*mask & (1U << rank)) == 0) {
      continue;
    }
    if (text.size() < kAdjustDigits) {
      return std::nullopt;
    }
    const std::optional<unsigned> hue = ReadHex(text, kHueDigits);
    const std::optional<unsigned> saturation = ReadHex(text.substr(kSaturationAt), kByteDigits);
    const std::optional<unsigned> value = ReadHex(text.substr(kValueAt), kByteDigits);
    const std::optional<unsigned> absolute = ReadHex(text.substr(kAbsoluteAt), kByteDigits);
    if (!hue || !saturation || !value || !absolute || *absolute > 1) {
      return std::nullopt;
    }
    StyleAdjust& adjust = code.adjusts[rank];
    adjust.hue = static_cast<std::int16_t>(static_cast<std::uint16_t>(*hue));
    adjust.saturation = static_cast<std::int8_t>(static_cast<std::uint8_t>(*saturation));
    adjust.value = static_cast<std::int8_t>(static_cast<std::uint8_t>(*value));
    adjust.absolute = *absolute == 1;
    if (!InBounds(adjust)) {
      return std::nullopt;
    }
    text.remove_prefix(kAdjustDigits);
  }
  if (!text.empty()) {
    return std::nullopt;
  }
  return code;
}

}  // namespace ro::sim

#include "ro/sim/style_layer_code.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>

namespace ro::sim {
namespace {

constexpr std::string_view kPrefix = "L1:";
constexpr std::string_view kMixedPrefix = "L2:";
// Une clé : quatre octets ; un nombre de retouches : un octet.
constexpr std::size_t kKeyDigits = 8;
constexpr unsigned kKeyBits = 32;
// Une retouche : l'index, puis rouge, vert, bleu.
constexpr std::size_t kRetouchDigits = 4 * 2;
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

void AppendAdjusts(std::string& out, const std::array<StyleAdjust, kStyleLayerAdjustCount>& adjusts,
                   std::size_t ranks) {
  unsigned mask = 0;
  for (std::size_t rank = 0; rank < ranks; ++rank) {
    if (!Neutral(adjusts[rank])) {
      mask |= 1U << rank;
    }
  }
  AppendByte(out, (mask >> kByteBits) & kByteMask);
  AppendByte(out, mask & kByteMask);
  out.push_back(kSeparator);
  for (std::size_t rank = 0; rank < ranks; ++rank) {
    if ((mask & (1U << rank)) == 0) {
      continue;
    }
    const StyleAdjust& adjust = adjusts[rank];
    const auto hue = static_cast<std::uint16_t>(adjust.hue);
    AppendByte(out, (hue >> kByteBits) & kByteMask);
    AppendByte(out, hue & kByteMask);
    AppendByte(out, static_cast<std::uint8_t>(adjust.saturation));
    AppendByte(out, static_cast<std::uint8_t>(adjust.value));
    AppendByte(out, adjust.absolute ? 1U : 0U);
  }
}

// Le masque puis ses réglages, dans `ranks` rangs au plus ; faux si illisibles.
[[nodiscard]] bool ReadAdjusts(std::string_view& text,
                               std::array<StyleAdjust, kStyleLayerAdjustCount>& adjusts,
                               std::size_t ranks) {
  const std::optional<std::string_view> mask_field = TakeField(text);
  if (!mask_field || mask_field->size() != kMaskDigits) {
    return false;
  }
  const std::optional<unsigned> mask = ReadHex(*mask_field, kMaskDigits);
  if (!mask || (*mask >> ranks) != 0) {
    return false;
  }
  for (std::size_t rank = 0; rank < ranks; ++rank) {
    if ((*mask & (1U << rank)) == 0) {
      continue;
    }
    if (text.size() < kAdjustDigits) {
      return false;
    }
    const std::optional<unsigned> hue = ReadHex(text, kHueDigits);
    const std::optional<unsigned> saturation = ReadHex(text.substr(kSaturationAt), kByteDigits);
    const std::optional<unsigned> value = ReadHex(text.substr(kValueAt), kByteDigits);
    const std::optional<unsigned> absolute = ReadHex(text.substr(kAbsoluteAt), kByteDigits);
    if (!hue || !saturation || !value || !absolute || *absolute > 1) {
      return false;
    }
    StyleAdjust& adjust = adjusts[rank];
    adjust.hue = static_cast<std::int16_t>(static_cast<std::uint16_t>(*hue));
    adjust.saturation = static_cast<std::int8_t>(static_cast<std::uint8_t>(*saturation));
    adjust.value = static_cast<std::int8_t>(static_cast<std::uint8_t>(*value));
    adjust.absolute = *absolute == 1;
    if (!InBounds(adjust)) {
      return false;
    }
    text.remove_prefix(kAdjustDigits);
  }
  return true;
}

// Le profil mixte s'applique-t-il à ce calque, à ce profil ? Le reste aux
// rampes, les retouches aussi aux classes ; jamais à la peau.
[[nodiscard]] bool TakesRest(StyleLayerKind layer, std::uint8_t profile) {
  return layer != StyleLayerKind::kSkin && profile == kStyleLayerCodeRampProfile;
}

[[nodiscard]] bool HasMixed(const StyleLayerCode& code) {
  return !code.retouches.empty() || !std::ranges::all_of(code.rest, Neutral);
}

}  // namespace

std::size_t StyleLayerRanks(StyleLayerKind layer, std::uint8_t profile) noexcept {
  // ⚠ PAS D'AIGUILLAGE SUR LE CALQUE : un calque qu'un code ne porte pas — la
  // suite du corps, qui voyage avec lui — rend zéro, et le code est refusé.
  const bool ramps = profile == kStyleLayerCodeRampProfile;
  if (layer == StyleLayerKind::kSkin) {
    return ramps ? 1 : 0;
  }
  // Un compagnon n'est teint que par rampes : ses pièces sont celles de son
  // sprite, mesurées comme celles d'une tête.
  if (layer == StyleLayerKind::kHead || layer == StyleLayerKind::kCompanion) {
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
  const bool mixed = HasMixed(code);
  std::string out(mixed ? kMixedPrefix : kPrefix);
  out += std::to_string(static_cast<unsigned>(code.layer));
  out.push_back(kSeparator);
  out += std::to_string(static_cast<unsigned>(code.profile));
  out.push_back(kSeparator);
  if (mixed) {
    for (unsigned shift = kKeyBits; shift > 0; shift -= kByteBits) {
      AppendByte(out, (code.key >> (shift - kByteBits)) & kByteMask);
    }
    out.push_back(kSeparator);
  }
  AppendAdjusts(out, code.adjusts, ranks);
  if (!mixed) {
    return out;
  }
  out.push_back(kSeparator);
  AppendAdjusts(out, code.rest,
                TakesRest(code.layer, code.profile) ? kStyleLayerAdjustCount : std::size_t{0});
  out.push_back(kSeparator);
  AppendByte(out, static_cast<unsigned>(code.retouches.size()));
  out.push_back(kSeparator);
  for (const StyleRetouch& retouch : code.retouches) {
    AppendByte(out, retouch.index);
    AppendByte(out, retouch.red);
    AppendByte(out, retouch.green);
    AppendByte(out, retouch.blue);
  }
  return out;
}

std::optional<StyleLayerCode> DecodeStyleLayerCode(std::string_view text) {
  text = Trimmed(text);
  const bool mixed = text.starts_with(kMixedPrefix);
  if (!mixed && !text.starts_with(kPrefix)) {
    return std::nullopt;
  }
  text.remove_prefix(kPrefix.size());
  const std::optional<std::string_view> layer_field = TakeField(text);
  const std::optional<std::string_view> profile_field = TakeField(text);
  if (!layer_field || !profile_field) {
    return std::nullopt;
  }
  const std::optional<int> layer = ReadNumber(*layer_field);
  const std::optional<int> profile = ReadNumber(*profile_field);
  if (!layer || !profile || *profile < 0 || *profile > static_cast<int>(kByteMask)) {
    return std::nullopt;
  }
  if (*layer < static_cast<int>(StyleLayerKind::kHead) ||
      *layer > static_cast<int>(StyleLayerKind::kCompanion)) {
    return std::nullopt;
  }
  StyleLayerCode code;
  code.layer = static_cast<StyleLayerKind>(*layer);
  code.profile = static_cast<std::uint8_t>(*profile);
  const std::size_t ranks = StyleLayerRanks(code.layer, code.profile);
  if (ranks == 0) {
    return std::nullopt;
  }
  if (mixed) {
    const std::optional<std::string_view> key_field = TakeField(text);
    const std::optional<unsigned> key = key_field && key_field->size() == kKeyDigits
                                            ? ReadHex(*key_field, kKeyDigits)
                                            : std::nullopt;
    if (!key || code.layer == StyleLayerKind::kSkin) {
      return std::nullopt;
    }
    code.key = *key;
  }
  if (!ReadAdjusts(text, code.adjusts, ranks)) {
    return std::nullopt;
  }
  if (!mixed) {
    return text.empty() ? std::optional(code) : std::nullopt;
  }
  // Le séparateur qui clôt les réglages des pièces, puis le reste.
  if (!text.starts_with(kSeparator)) {
    return std::nullopt;
  }
  text.remove_prefix(1);
  if (!ReadAdjusts(text, code.rest,
                   TakesRest(code.layer, code.profile) ? kStyleLayerAdjustCount : 0) ||
      !text.starts_with(kSeparator)) {
    return std::nullopt;
  }
  text.remove_prefix(1);
  const std::optional<std::string_view> count_field = TakeField(text);
  const std::optional<unsigned> count = count_field && count_field->size() == kByteDigits
                                            ? ReadHex(*count_field, kByteDigits)
                                            : std::nullopt;
  if (!count || *count > kStyleRetouchesMost || text.size() != *count * kRetouchDigits) {
    return std::nullopt;
  }
  for (unsigned at = 0; at < *count; ++at) {
    const std::string_view digits = text.substr(at * kRetouchDigits, kRetouchDigits);
    const std::optional<unsigned> index = ReadHex(digits, kByteDigits);
    const std::optional<unsigned> red = ReadHex(digits.substr(kByteDigits), kByteDigits);
    const std::optional<unsigned> green = ReadHex(digits.substr(2 * kByteDigits), kByteDigits);
    const std::optional<unsigned> blue = ReadHex(digits.substr(3 * kByteDigits), kByteDigits);
    if (!index || !red || !green || !blue) {
      return std::nullopt;
    }
    const StyleRetouch retouch{.index = static_cast<std::uint8_t>(*index),
                               .red = static_cast<std::uint8_t>(*red),
                               .green = static_cast<std::uint8_t>(*green),
                               .blue = static_cast<std::uint8_t>(*blue)};
    // 🔴 COMME SUR LE FIL : permises, triées, une par index.
    if (!StyleRetouchAllowed(retouch) ||
        (!code.retouches.empty() && code.retouches.back().index >= retouch.index)) {
      return std::nullopt;
    }
    code.retouches.push_back(retouch);
  }
  return code;
}

}  // namespace ro::sim

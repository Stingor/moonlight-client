#include "ro/sim/style_code.h"

#include <array>
#include <charconv>
#include <cstddef>

namespace ro::sim {
namespace {

// Les chiffres hexadécimaux qu'on ÉCRIT. ⚠ EN MINUSCULES, comme la référence :
// deux clients qui n'écriraient pas la même casse produiraient deux chaînes
// différentes pour un même style, et la comparaison « ce réglage a-t-il changé
// depuis la validation ? » répondrait faux.
constexpr std::string_view kHexDigits = "0123456789abcdef";
constexpr int kHexBase = 16;
// La largeur du champ de corps, en chiffres : un condensé de trente-deux bits.
constexpr std::size_t kBodyKeyDigits = 8;
constexpr int kNibbleBits = 4;

void AppendByte(std::string& out, std::uint8_t value) {
  out.push_back(kHexDigits[(value >> kNibbleBits) & 0xFU]);
  out.push_back(kHexDigits[value & 0xFU]);
}

[[nodiscard]] int FromHex(char digit) {
  if (digit >= '0' && digit <= '9') {
    return digit - '0';
  }
  // ⚠ LES DEUX CASSES SE LISENT, même si l'on n'écrit qu'en minuscules : un
  // code recopié à la main ou passé par un traitement de texte remonte
  // parfois en majuscules, et le refuser pour cela n'apprendrait rien au
  // joueur.
  if (digit >= 'a' && digit <= 'f') {
    return digit - 'a' + 10;
  }
  if (digit >= 'A' && digit <= 'F') {
    return digit - 'A' + 10;
  }
  return -1;
}

// Un entier décimal signé, tel que la référence l'écrit. Rien si le champ
// n'en est pas un.
[[nodiscard]] std::optional<int> ReadNumber(std::string_view field) {
  if (field.empty()) {
    return std::nullopt;
  }
  int value = 0;
  const char* first = field.data();
  const char* last = first + field.size();
  const std::from_chars_result read = std::from_chars(first, last, value);
  if (read.ec != std::errc{} || read.ptr != last) {
    return std::nullopt;
  }
  return value;
}

}  // namespace

std::string EncodeStyleCode(const StyleRecipe& recipe, std::uint32_t body_key) {
  std::string out;
  out.reserve(kStyleCodeHexChars + kBodyKeyDigits + 24);
  out += std::to_string(static_cast<int>(kStyleWireVersion));
  out.push_back(':');
  for (std::size_t digit = kBodyKeyDigits; digit-- > 0;) {
    out.push_back(kHexDigits[(body_key >> (digit * kNibbleBits)) & 0xFU]);
  }
  out.push_back(':');
  out += std::to_string(static_cast<int>(recipe.body_palette));
  out.push_back(':');
  out += std::to_string(static_cast<int>(recipe.hair_palette));
  out.push_back(':');
  out += std::to_string(static_cast<int>(recipe.hair_style));
  out.push_back(':');
  for (const StyleAdjust& adjust : recipe.ramps) {
    // ⚠ CHAMP PAR CHAMP, ET EN PETIT-BOUTISTE, comme sur le réseau : la
    // structure porte du bourrage, et la recopier telle quelle écrirait des
    // octets que personne ne relit de la même façon.
    const auto hue = static_cast<std::uint16_t>(adjust.hue);
    AppendByte(out, static_cast<std::uint8_t>(hue & 0xFFU));
    AppendByte(out, static_cast<std::uint8_t>((hue >> 8) & 0xFFU));
    AppendByte(out, static_cast<std::uint8_t>(adjust.saturation));
    AppendByte(out, static_cast<std::uint8_t>(adjust.value));
    AppendByte(out, adjust.absolute ? 1U : 0U);
  }
  return out;
}

std::optional<StyleCode> DecodeStyleCode(std::string_view text) {
  const std::size_t start = text.find_first_not_of(" \t\r\n");
  if (start == std::string_view::npos) {
    return std::nullopt;
  }
  const std::size_t stop = text.find_last_not_of(" \t\r\n");
  const std::string_view code = text.substr(start, stop - start + 1);

  // Cinq champs séparés par des deux-points, puis la queue hexadécimale.
  std::array<std::size_t, 5> separator{};
  std::size_t at = 0;
  for (std::size_t field = 0; field < separator.size(); ++field) {
    const std::size_t found = code.find(':', at);
    if (found == std::string_view::npos) {
      return std::nullopt;
    }
    separator[field] = found;
    at = found + 1;
  }
  if (code.size() != separator.back() + 1 + kStyleCodeHexChars) {
    return std::nullopt;
  }

  // 🔴 LA VERSION D'ABORD, ET UNE SEULE EST ACCEPTÉE : la courante. Voir
  // l'en-tête — une autre version ne se migre pas, elle se refuse.
  const std::optional<int> version = ReadNumber(code.substr(0, separator[0]));
  if (!version.has_value() || *version != static_cast<int>(kStyleWireVersion)) {
    return std::nullopt;
  }

  StyleCode read;
  const std::string_view body = code.substr(separator[0] + 1, separator[1] - separator[0] - 1);
  if (body.size() != kBodyKeyDigits) {
    return std::nullopt;
  }
  for (const char digit : body) {
    const int nibble = FromHex(digit);
    if (nibble < 0) {
      return std::nullopt;
    }
    read.body_key = (read.body_key << kNibbleBits) | static_cast<std::uint32_t>(nibble);
  }

  const auto field = [&](std::size_t rank) {
    return code.substr(separator[rank] + 1, separator[rank + 1] - separator[rank] - 1);
  };
  const std::optional<int> body_palette = ReadNumber(field(1));
  const std::optional<int> hair_palette = ReadNumber(field(2));
  const std::optional<int> hair_style = ReadNumber(field(3));
  if (!body_palette.has_value() || !hair_palette.has_value() || !hair_style.has_value()) {
    return std::nullopt;
  }
  read.recipe.body_palette = static_cast<std::int16_t>(*body_palette);
  read.recipe.hair_palette = static_cast<std::int16_t>(*hair_palette);
  read.recipe.hair_style = static_cast<std::int16_t>(*hair_style);

  const std::string_view tail = code.substr(separator.back() + 1);
  std::array<std::uint8_t, kStyleRampCount * kStyleCodeRampBytes> bytes{};
  for (std::size_t rank = 0; rank < bytes.size(); ++rank) {
    const int high = FromHex(tail[rank * 2]);
    const int low = FromHex(tail[rank * 2 + 1]);
    if (high < 0 || low < 0) {
      return std::nullopt;
    }
    bytes[rank] = static_cast<std::uint8_t>((high << kNibbleBits) | low);
  }
  for (std::size_t rank = 0; rank < kStyleRampCount; ++rank) {
    const std::uint8_t* at_ramp = bytes.data() + rank * kStyleCodeRampBytes;
    StyleAdjust& adjust = read.recipe.ramps[rank];
    adjust.hue = static_cast<std::int16_t>(static_cast<std::uint16_t>(at_ramp[0]) |
                                           (static_cast<std::uint16_t>(at_ramp[1]) << 8));
    adjust.saturation = static_cast<std::int8_t>(at_ramp[2]);
    adjust.value = static_cast<std::int8_t>(at_ramp[3]);
    // ⚠ TOUT NON-ZÉRO VAUT « ABSOLU », comme à la lecture d'une trame : un
    // producteur plus récent pourrait y mettre autre chose que un.
    adjust.absolute = at_ramp[4] != 0;
  }
  return read;
}

// ── LA BALISE ───────────────────────────────────────────────────────────────

namespace {

constexpr std::string_view kTagOpen = "<STYL>";
constexpr std::string_view kTagClose = "</STYL>";
// Ce qui séparerait l'étiquette du code, et ce qui couperait la balise.
constexpr char kTagSeparator = ':';
constexpr char kTagBracket = '<';
// Ce qui les remplace dans une étiquette. Un tiret : il se lit, et il ne
// ressemble à rien d'autre dans un pseudo.
constexpr char kTagSafe = '-';

}  // namespace

std::string StyleTagLabel(std::string_view label) {
  std::string clean(label);
  for (char& letter : clean) {
    if (letter == kTagSeparator || letter == kTagBracket) {
      letter = kTagSafe;
    }
  }
  return clean;
}

std::string BuildStyleTag(std::string_view label, std::string_view code) {
  if (code.empty()) {
    return {};
  }
  std::string tag(kTagOpen);
  tag += StyleTagLabel(label);
  tag.push_back(kTagSeparator);
  tag.append(code);
  tag.append(kTagClose);
  return tag;
}

std::size_t FindStyleTag(std::string_view text, std::size_t from) {
  return from >= text.size() ? std::string_view::npos : text.find(kTagOpen, from);
}

StyleTagRead ParseStyleTag(std::string_view text, StyleTag* out, std::size_t* after) {
  if (after != nullptr) {
    *after = 0;
  }
  if (out == nullptr || !text.starts_with(kTagOpen)) {
    return StyleTagRead::kNotATag;
  }
  const std::size_t close = text.find(kTagClose, kTagOpen.size());
  if (close == std::string_view::npos) {
    // ⚠ TRONQUÉE PAR LE SERVEUR : rien à sauter, et l'appelant la laissera
    // telle quelle. Une ligne amputée en silence est pire qu'une ligne laide.
    return StyleTagRead::kNotATag;
  }
  if (after != nullptr) {
    *after = close + kTagClose.size();
  }
  const std::string_view body = text.substr(kTagOpen.size(), close - kTagOpen.size());
  // ⚠ LE PREMIER DEUX-POINTS SÉPARE, ET LUI SEUL : le code en porte quatre, et
  // chercher le dernier découperait la balise au milieu de la recette.
  const std::size_t split = body.find(kTagSeparator);
  // Sans séparateur, tout le corps vaut étiquette : c'est ce qui reste à
  // montrer, et l'appelant n'a pas à redécouper ce que nous savons découper.
  out->label = split == std::string_view::npos ? body : body.substr(0, split);
  if (split == std::string_view::npos) {
    return StyleTagRead::kUnreadable;
  }
  out->code = body.substr(split + 1);
  const std::optional<StyleCode> read = DecodeStyleCode(out->code);
  if (!read.has_value()) {
    return StyleTagRead::kUnreadable;
  }
  out->read = *read;
  return StyleTagRead::kOk;
}

}  // namespace ro::sim

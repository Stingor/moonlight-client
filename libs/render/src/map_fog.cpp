#include "ro/render/map_fog.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <string>
#include <system_error>
#include <utility>

namespace ro::render {
namespace {

// Une entrée : la clé, puis ses quatre valeurs.
constexpr std::size_t kValuesPerRow = 4;
constexpr char kWordEnd = '#';

// La plage de vue du client, en unités du monde : ses plans proche et lointain.
constexpr float kViewNearDepth = 10.0F;
constexpr float kViewFarDepth = 1500.0F;

// Le « 0x » qui ouvre une couleur, et que la lecture saute sans le regarder.
constexpr std::size_t kHexPrefixLength = 2;
constexpr unsigned kBitsPerHexDigit = 4;
constexpr unsigned kHexLetterOffset = 10;
// ⚠ AU-DELÀ DE HUIT CHIFFRES, LE DÉCALAGE REPART DE ZÉRO : le processeur du
// client ne garde que cinq bits du nombre de rangs à décaler.
constexpr unsigned kShiftModulo = 32;

// Le numéro d'une carte d'instance : trois caractères, qui finissent un rang
// avant le `@`.
constexpr char kInstanceMark = '@';
constexpr std::size_t kInstanceNumberEnd = 4;
constexpr std::size_t kInstanceNumberLength = 3;
constexpr std::string_view kRswExtension = ".rsw";

constexpr float kByteScale = 255.0F;
constexpr unsigned kAlphaShift = 24;
constexpr unsigned kRedShift = 16;
constexpr unsigned kGreenShift = 8;
constexpr std::uint32_t kByteMask = 0xFFU;

std::uint8_t ByteAt(std::uint32_t color, unsigned shift) {
  return static_cast<std::uint8_t>((color >> shift) & kByteMask);
}

// LE NOMBRE QUI OUVRE UN MOT, comme `atof` le lit : les blancs de tête sautent,
// un signe plus aussi, et ce qui ne se lit pas vaut zéro.
float LeadingFloat(std::string_view text) {
  std::size_t first = 0;
  while (first < text.size() && (text[first] == ' ' || text[first] == '\t')) {
    ++first;
  }
  if (first < text.size() && text[first] == '+') {
    ++first;
  }
  double value = 0.0;
  const std::from_chars_result parsed =
      std::from_chars(text.data() + first, text.data() + text.size(), value);
  return parsed.ec == std::errc() ? static_cast<float>(value) : 0.0F;
}

std::optional<unsigned> HexDigit(char character) {
  if (character >= '0' && character <= '9') {
    return static_cast<unsigned>(character - '0');
  }
  if (character >= 'A' && character <= 'F') {
    return static_cast<unsigned>(character - 'A') + kHexLetterOffset;
  }
  if (character >= 'a' && character <= 'f') {
    return static_cast<unsigned>(character - 'a') + kHexLetterOffset;
  }
  return std::nullopt;
}

}  // namespace

std::vector<FogTableRow> ParseFogTable(std::string_view text) {
  std::vector<FogTableRow> rows;
  std::string key;
  std::vector<std::string> values;
  std::size_t word_start = 0;
  for (std::size_t index = 0; index < text.size(); ++index) {
    const char character = text[index];
    if (character == '\n' || character == '\r') {
      word_start = index + 1;
      continue;
    }
    if (character != kWordEnd) {
      continue;
    }
    std::string word(text.substr(word_start, index - word_start));
    word_start = index + 1;
    if (key.empty()) {
      key = std::move(word);
      continue;
    }
    values.push_back(std::move(word));
    if (values.size() == kValuesPerRow) {
      FogTableRow row;
      row.map_key = std::move(key);
      std::ranges::move(values, row.values.begin());
      rows.push_back(std::move(row));
      key.clear();
      values.clear();
    }
  }
  return rows;
}

std::uint32_t FogColorOf(std::string_view text) {
  std::uint32_t color = 0;
  unsigned shift = 0;
  for (std::size_t index = text.size(); index > kHexPrefixLength; --index) {
    if (const std::optional<unsigned> digit = HexDigit(text[index - 1]); digit.has_value()) {
      color += *digit << (shift % kShiftModulo);
    }
    shift += kBitsPerHexDigit;
  }
  return color;
}

std::string FogTableKeyOf(std::string_view map_name) {
  std::string key(map_name);
  if (const std::size_t mark = key.rfind(kInstanceMark);
      mark != std::string::npos && mark >= kInstanceNumberEnd) {
    key.erase(mark - kInstanceNumberEnd, kInstanceNumberLength);
  }
  key += kRswExtension;
  return key;
}

std::optional<MapFog> FogOfMap(std::span<const FogTableRow> rows, std::string_view map_name) {
  const std::string key = FogTableKeyOf(map_name);
  const auto found = std::ranges::find(rows, key, &FogTableRow::map_key);
  if (found == rows.end()) {
    return std::nullopt;
  }
  MapFog fog;
  fog.near_fraction = LeadingFloat(found->values[0]);
  fog.far_fraction = LeadingFloat(found->values[1]);
  fog.color_argb = FogColorOf(found->values[2]);
  fog.density = LeadingFloat(found->values[3]);
  return fog;
}

std::optional<MapFog> LoadMapFog(vfs::Vfs& vfs, std::string_view map_name) {
  auto bytes = vfs.Read(kFogTablePath);
  if (!bytes) {
    return std::nullopt;
  }
  const std::string_view text(reinterpret_cast<const char*>(bytes->data()), bytes->size());
  return FogOfMap(ParseFogTable(text), map_name);
}

SceneFog SceneFogOf(const MapFog& fog) {
  constexpr float kViewSpan = kViewFarDepth - kViewNearDepth;
  SceneFog scene;
  scene.on = true;
  if (fog.far_fraction == fog.near_fraction) {
    scene.start_depth = kViewNearDepth;
    scene.end_depth = kViewFarDepth;
  } else {
    scene.start_depth = kViewNearDepth + fog.near_fraction * kViewSpan;
    scene.end_depth = kViewNearDepth + fog.far_fraction * kViewSpan;
  }
  scene.color = {static_cast<float>(ByteAt(fog.color_argb, kRedShift)) / kByteScale,
                 static_cast<float>(ByteAt(fog.color_argb, kGreenShift)) / kByteScale,
                 static_cast<float>(ByteAt(fog.color_argb, 0)) / kByteScale};
  return scene;
}

std::array<float, 4> FoggedClearColor(const std::array<float, 4>& clear, const MapFog& fog) {
  const float weight = static_cast<float>(ByteAt(fog.color_argb, kAlphaShift)) / kByteScale;
  constexpr std::array<unsigned, 3> kShifts = {kRedShift, kGreenShift, 0};
  std::array<float, 4> mixed = clear;
  for (std::size_t channel = 0; channel < kShifts.size(); ++channel) {
    const float background = std::round(clear[channel] * kByteScale);
    const auto fog_byte = static_cast<float>(ByteAt(fog.color_argb, kShifts[channel]));
    const auto truncated = static_cast<int>(background * (1.0F - weight) + fog_byte * weight);
    mixed[channel] = static_cast<float>(truncated) / kByteScale;
  }
  return mixed;
}

}  // namespace ro::render

#include "ro/render/map_fog.h"

#include <cmath>
#include <cstddef>
#include <expected>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"
#include "ro/vfs/encoding.h"

namespace ro::render {
namespace {

constexpr int kSupportedVersion = 1;

// La plage de vue du client, en unités du monde : ses plans proche et lointain.
constexpr float kViewNearDepth = 10.0F;
constexpr float kViewFarDepth = 1500.0F;

// Les quatre octets d'une couleur, dans l'ordre de la table : alpha, rouge,
// vert, bleu.
constexpr std::size_t kColorBytes = 4;
constexpr unsigned kBitsPerByte = 8;
constexpr int kByteMost = 255;

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

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     std::string(kFogTablePath) + " : " + std::move(detail)};
}

// UNE FRACTION, lue en double puis ramenée à un float : le client lisait le
// nombre ainsi, et la table l'écrit tel que le double le donne.
[[nodiscard]] float FractionOf(const YAML::Node& map, const char* field) {
  const YAML::Node value = map[field];
  if (!value) {
    throw YAML::KeyNotFound(map.Mark(), std::string(field));
  }
  return static_cast<float>(value.as<double>());
}

// La couleur : quatre octets, alpha d'abord.
[[nodiscard]] std::uint32_t ColorOf(const YAML::Node& map) {
  const YAML::Node color = map["color"];
  if (!color || !color.IsSequence() || color.size() != kColorBytes) {
    throw YAML::RepresentationException(map.Mark(), "une couleur sans ses quatre octets");
  }
  std::uint32_t argb = 0;
  for (const YAML::Node& part : color) {
    const int byte = part.as<int>();
    if (byte < 0 || byte > kByteMost) {
      throw YAML::RepresentationException(part.Mark(), "un octet hors de 0 à 255");
    }
    argb = (argb << kBitsPerByte) | static_cast<std::uint32_t>(byte);
  }
  return argb;
}

}  // namespace

std::string FogTableKeyOf(std::string_view map_name) {
  std::string key(map_name);
  if (const std::size_t mark = key.rfind(kInstanceMark);
      mark != std::string::npos && mark >= kInstanceNumberEnd) {
    key.erase(mark - kInstanceNumberEnd, kInstanceNumberLength);
  }
  key += kRswExtension;
  return key;
}

core::Result<MapFogTable> MapFogTable::Parse(std::string_view yaml_text) {
  MapFogTable table;
  // yaml-cpp signale par exception : elle ne franchit pas cette fonction.
  try {
    const YAML::Node whole = YAML::Load(std::string(yaml_text));
    if (!whole.IsMap()) {
      return std::unexpected(Refused("le texte n'est pas une table YAML"));
    }
    const YAML::Node version = whole["version"];
    if (!version || version.as<int>() != kSupportedVersion) {
      return std::unexpected(Refused("version non gérée"));
    }
    const YAML::Node maps = whole["maps"];
    if (!maps || !maps.IsMap()) {
      return std::unexpected(Refused("« maps » n'est pas une table"));
    }
    for (const auto& pair : maps) {
      auto key = vfs::Utf8ToCp949(pair.first.as<std::string>());
      if (!key) {
        return std::unexpected(Refused("un nom de carte qui ne s'écrit pas en CP949"));
      }
      if (!pair.second.IsMap()) {
        return std::unexpected(Refused(*key + " n'est pas une table"));
      }
      MapFog fog;
      fog.near_fraction = FractionOf(pair.second, "near");
      fog.far_fraction = FractionOf(pair.second, "far");
      fog.color_argb = ColorOf(pair.second);
      fog.density = FractionOf(pair.second, "density");
      table.by_key_.emplace(*key + std::string(kRswExtension), fog);
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return table;
}

std::optional<MapFog> MapFogTable::FogOf(std::string_view map_name) const {
  const auto found = by_key_.find(FogTableKeyOf(map_name));
  if (found == by_key_.end()) {
    return std::nullopt;
  }
  return found->second;
}

std::optional<MapFog> LoadMapFog(vfs::Vfs& vfs, std::string_view map_name) {
  auto bytes = vfs.Read(kFogTablePath);
  if (!bytes) {
    return std::nullopt;
  }
  auto table = MapFogTable::Parse(
      std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
  if (!table) {
    core::LogWarning("{} — {} s'ouvre sans brouillard", core::Describe(table.error()), map_name);
    return std::nullopt;
  }
  return table->FogOf(map_name);
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

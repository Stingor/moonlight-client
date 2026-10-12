#include "ro/render/map_effect_table.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"
#include "ro/vfs/encoding.h"

namespace ro::render {
namespace {

constexpr std::string_view kTableFolder = "data\\yaml_files\\map_effects\\";
constexpr std::string_view kTableExtension = ".yml";
constexpr int kSupportedVersion = 1;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData, "effets de carte : " + std::move(detail)};
}

// Une suite de `Count` nombres.
template <typename Number, std::size_t Count>
[[nodiscard]] std::array<Number, Count> NumbersOf(const YAML::Node& node) {
  if (!node.IsSequence() || node.size() != Count) {
    throw YAML::RepresentationException(node.Mark(), "suite de longueur inattendue");
  }
  std::array<Number, Count> values{};
  for (std::size_t index = 0; index < Count; ++index) {
    values[index] = node[index].as<Number>();
  }
  return values;
}

[[nodiscard]] glm::vec3 VectorOf(const YAML::Node& node) {
  const auto values = NumbersOf<float, 3>(node);
  return {values[0], values[1], values[2]};
}

[[nodiscard]] std::array<float, 2> PairOf(const YAML::Node& node) {
  return NumbersOf<float, 2>(node);
}

// Un octet : yaml-cpp lirait un `uint8_t` comme un caractère.
[[nodiscard]] std::uint8_t ByteOf(const YAML::Node& node) {
  const unsigned value = node.as<unsigned>();
  if (value > UINT8_MAX) {
    throw YAML::RepresentationException(node.Mark(), "octet hors bornes");
  }
  return static_cast<std::uint8_t>(value);
}

// Le YAML est en UTF-8 ; une texture se cherche, comme les noms du RSW, en CP949.
[[nodiscard]] std::string TextureOf(const YAML::Node& node) {
  auto cp949 = vfs::Utf8ToCp949(node.as<std::string>());
  if (!cp949) {
    throw YAML::RepresentationException(node.Mark(), "texture sans équivalent CP949");
  }
  return std::move(*cp949);
}

// Un champ d'un réglage. Un champ que la table ne nomme pas garde le défaut du
// réglage ; un champ inconnu refuse la table.
void ReadEmitterField(std::string_view field, const YAML::Node& value, MapEmitterSetting& setting) {
  if (field == "max_count") {
    setting.max_count = value.as<std::int32_t>();
  } else if (field == "first_direction") {
    setting.first_direction = VectorOf(value);
  } else if (field == "second_direction") {
    setting.second_direction = VectorOf(value);
  } else if (field == "gravity") {
    setting.gravity = VectorOf(value);
  } else if (field == "radius") {
    setting.radius = VectorOf(value);
  } else if (field == "color") {
    setting.color = NumbersOf<std::int32_t, 4>(value);
  } else if (field == "rate") {
    setting.rate = PairOf(value);
  } else if (field == "size") {
    setting.size = PairOf(value);
  } else if (field == "life") {
    setting.life = PairOf(value);
  } else if (field == "scale") {
    setting.scale = PairOf(value);
  } else if (field == "speed") {
    setting.speed = value.as<float>();
  } else if (field == "texture") {
    setting.texture = TextureOf(value);
  } else if (field == "blend_source") {
    setting.blend_source = value.as<std::int32_t>();
  } else if (field == "blend_destination") {
    setting.blend_destination = value.as<std::int32_t>();
  } else if (field == "depth_tested") {
    setting.depth_tested = value.as<bool>();
  } else if (field == "flat") {
    setting.flat = value.as<bool>();
  } else if (field == "rotation_degrees") {
    setting.rotation_degrees = VectorOf(value);
  } else if (field == "eternal") {
    setting.eternal = value.as<bool>();
  } else {
    throw YAML::RepresentationException(value.Mark(), "champ d'émetteur inconnu");
  }
}

void ReadWindField(std::string_view field, const YAML::Node& value, MapWindSetting& setting) {
  if (field == "position") {
    setting.position = VectorOf(value);
  } else if (field == "rings") {
    setting.rings = value.as<std::uint16_t>();
  } else if (field == "alpha_peak") {
    setting.alpha_peak = ByteOf(value);
  } else if (field == "rgb") {
    if (!value.IsSequence() || value.size() != setting.rgb.size()) {
      throw YAML::RepresentationException(value.Mark(), "teinte de longueur inattendue");
    }
    for (std::size_t channel = 0; channel < setting.rgb.size(); ++channel) {
      setting.rgb[channel] = ByteOf(value[channel]);
    }
  } else if (field == "radius") {
    setting.radius = value.as<float>();
  } else if (field == "thickness") {
    setting.thickness = value.as<float>();
  } else if (field == "height") {
    setting.height = value.as<float>();
  } else if (field == "speed") {
    setting.speed = value.as<float>();
  } else if (field == "full_angle") {
    setting.full_angle = value.as<std::uint16_t>();
  } else if (field == "tilt_x_degrees") {
    setting.tilt_x_degrees = value.as<float>();
  } else if (field == "tilt_z_degrees") {
    setting.tilt_z_degrees = value.as<float>();
  } else if (field == "blend_source") {
    setting.blend_source = value.as<std::int32_t>();
  } else if (field == "blend_destination") {
    setting.blend_destination = value.as<std::int32_t>();
  } else if (field == "texture") {
    setting.texture = TextureOf(value);
  } else {
    throw YAML::RepresentationException(value.Mark(), "champ d'anneau de vent inconnu");
  }
}

// Les réglages d'une famille, par clé. Absente, la famille est vide.
template <typename Setting>
void ReadSettings(const YAML::Node& family,
                  void (*read_field)(std::string_view, const YAML::Node&, Setting&),
                  std::map<std::int32_t, Setting>& settings) {
  if (!family) {
    return;
  }
  if (!family.IsMap()) {
    throw YAML::RepresentationException(family.Mark(), "famille qui n'est pas une table");
  }
  for (const auto& pair : family) {
    if (!pair.second.IsMap()) {
      throw YAML::RepresentationException(pair.second.Mark(), "réglage qui n'est pas une table");
    }
    Setting setting;
    for (const auto& field : pair.second) {
      read_field(field.first.as<std::string>(), field.second, setting);
    }
    settings.insert_or_assign(pair.first.as<std::int32_t>(), std::move(setting));
  }
}

}  // namespace

std::string MapEffectTablePath(std::string_view map_name) {
  return std::string(kTableFolder) + std::string(map_name) + std::string(kTableExtension);
}

core::Result<MapEffectTable> ParseMapEffectTable(std::string_view yaml_text) {
  MapEffectTable table;
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
    if (const YAML::Node declared = whole["table_version"]; declared) {
      table.version = declared.as<std::uint32_t>();
    }
    ReadSettings<MapEmitterSetting>(whole["emitters"], &ReadEmitterField, table.emitters);
    ReadSettings<MapWindSetting>(whole["winds"], &ReadWindField, table.winds);
    if (const YAML::Node others = whole["other_tables"]; others) {
      if (!others.IsMap()) {
        return std::unexpected(Refused("« other_tables » n'est pas une table"));
      }
      for (const auto& pair : others) {
        table.other_tables.emplace(pair.first.as<std::string>(), pair.second.as<std::size_t>());
      }
    }
    if (const YAML::Node count = whole["malformed_emitters"]; count) {
      table.malformed_emitters = count.as<std::size_t>();
    }
    if (const YAML::Node count = whole["malformed_winds"]; count) {
      table.malformed_winds = count.as<std::size_t>();
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  table.present = true;
  return table;
}

MapEffectTable LoadMapEffectTable(vfs::Vfs& vfs, std::string_view map_name) {
  const std::string path = MapEffectTablePath(map_name);
  auto bytes = vfs.Read(path);
  if (!bytes) {
    return {};
  }
  core::Result<MapEffectTable> table = ParseMapEffectTable(
      std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
  if (!table) {
    core::LogWarning("{} : {} — aucun effet de la carte n'est réglé", path,
                     core::Describe(table.error()));
    return {};
  }
  return std::move(*table);
}

}  // namespace ro::render

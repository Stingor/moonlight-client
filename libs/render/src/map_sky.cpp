#include "ro/render/map_sky.h"

#include <algorithm>
#include <array>
#include <expected>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"

namespace ro::render {
namespace {

// D'UN STYLE DE CIEL À SON EFFET, tel que le client les associe.
//
// ⚠ LA SUITE A DES TROUS, et ils sont réels : six, douze, treize et quatorze ne
// lancent rien chez le client. Les combler par interpolation — 6 → 6xx par
// exemple — allumerait un ciel qu'aucune carte ne demande, sur un effet qui
// appartient à une compétence.
struct SkyStyleEffect {
  std::uint8_t style;
  std::uint32_t effect_id;
};

constexpr std::array<SkyStyleEffect, 12> kSkyStyleEffects = {{
    {.style = 0, .effect_id = 229},
    {.style = 1, .effect_id = 230},
    {.style = 2, .effect_id = 233},
    {.style = 3, .effect_id = 515},
    {.style = 4, .effect_id = 516},
    {.style = 5, .effect_id = 592},
    {.style = 7, .effect_id = 697},
    {.style = 8, .effect_id = 698},
    {.style = 9, .effect_id = 945},
    {.style = 10, .effect_id = 1045},
    {.style = 11, .effect_id = 1115},
    {.style = 15, .effect_id = 1145},
}};

constexpr int kSupportedVersion = 1;

constexpr std::string_view kBackgroundField = "background";
constexpr std::string_view kBackgroundFogField = "background_fog";
constexpr std::string_view kStarField = "star";
constexpr std::string_view kStylesField = "styles";
constexpr std::string_view kCloudsField = "clouds";

// La table donne ses composantes de zéro à 255, dans l'ordre rouge, vert, bleu.
constexpr float kByteScale = 255.0F;
constexpr int kLargestStyle = 255;

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     std::string(kMapSkyPath) + " : " + std::move(detail)};
}

// La couleur, ramenée à l'unité. Une couleur à deux composantes n'est pas une
// couleur : mieux vaut refuser la table que d'en compléter une au noir sans le
// dire.
[[nodiscard]] core::Result<std::array<float, 3>> BackgroundOf(const YAML::Node& node,
                                                              const std::string& map_name) {
  std::array<float, 3> color{};
  if (!node.IsSequence() || node.size() != color.size()) {
    return std::unexpected(Refused(map_name + " : la couleur n'a pas trois composantes"));
  }
  for (std::size_t index = 0; index < color.size(); ++index) {
    const auto component = node[index].as<float>();
    if (component < 0.0F || component > kByteScale) {
      return std::unexpected(Refused(map_name + " : composante hors de 0 à 255"));
    }
    color.at(index) = component / kByteScale;
  }
  return color;
}

// Une carte. Une exception de yaml-cpp, ou un refus.
[[nodiscard]] core::Result<MapSkyLook> LookFrom(const YAML::Node& node,
                                                const std::string& map_name) {
  if (!node.IsMap()) {
    return std::unexpected(Refused(map_name + " n'est pas une table"));
  }
  MapSkyLook look;
  bool star = false;
  std::vector<std::uint8_t> styles;
  for (const auto& field : node) {
    const auto name = field.first.as<std::string>();
    if (name == kBackgroundField) {
      auto color = BackgroundOf(field.second, map_name);
      if (!color) {
        return std::unexpected(color.error());
      }
      look.background = *color;
    } else if (name == kBackgroundFogField) {
      look.background_fog = field.second.as<bool>();
    } else if (name == kStarField) {
      star = field.second.as<bool>();
    } else if (name == kStylesField) {
      if (!field.second.IsSequence()) {
        return std::unexpected(Refused(map_name + " : « styles » n'est pas une liste"));
      }
      for (const YAML::Node& style : field.second) {
        const int number = style.as<int>();
        if (number < 0 || number > kLargestStyle) {
          return std::unexpected(Refused(map_name + " : style hors d'un octet"));
        }
        styles.push_back(static_cast<std::uint8_t>(number));
      }
    } else if (name == kCloudsField) {
      if (!field.second.IsSequence()) {
        return std::unexpected(Refused(map_name + " : « clouds » n'est pas une liste"));
      }
      look.parameterised_clouds = field.second.size();
    } else {
      return std::unexpected(Refused(map_name + " : champ inconnu " + name));
    }
  }

  // ⚠ LE CHAMP D'ÉTOILES D'ABORD, comme le client : il le lance AVANT de
  // parcourir la liste des styles. L'ordre décide de ce qui se peint par-dessus
  // quand une carte porte les deux.
  if (star) {
    if (const std::optional<std::uint32_t> id = SkyEffectIdForStyle(kStarSkyStyle)) {
      look.effects.push_back(*id);
    }
  }
  // ⚠ UN STYLE INCONNU NE S'INVENTE PAS : le client tombe sur le défaut de son
  // switch et ne lance rien. Le nôtre non plus.
  for (const std::uint8_t style : styles) {
    if (const std::optional<std::uint32_t> id = SkyEffectIdForStyle(style)) {
      look.effects.push_back(*id);
    }
  }
  return look;
}

}  // namespace

std::optional<std::uint32_t> SkyEffectIdForStyle(std::uint8_t style) {
  const auto found = std::ranges::find(kSkyStyleEffects, style, &SkyStyleEffect::style);
  if (found == kSkyStyleEffects.end()) {
    return std::nullopt;
  }
  return found->effect_id;
}

core::Result<MapSkyTable> MapSkyTable::Parse(std::string_view yaml_text) {
  MapSkyTable table;
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
      auto map_name = pair.first.as<std::string>();
      core::Result<MapSkyLook> look = LookFrom(pair.second, map_name);
      if (!look) {
        return std::unexpected(look.error());
      }
      table.by_map_[std::move(map_name)] = std::move(*look);
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return table;
}

MapSkyLook MapSkyTable::LookOf(std::string_view map_name) const {
  const auto found = by_map_.find(std::string(map_name));
  if (found == by_map_.end()) {
    return {};
  }
  return found->second;
}

MapSkyLook LoadMapSky(vfs::Vfs& vfs, std::string_view map_name) {
  auto bytes = vfs.Read(kMapSkyPath);
  if (!bytes) {
    core::LogWarning("{} absente — {} s'ouvre sans ciel", kMapSkyPath, map_name);
    return {};
  }
  auto table = MapSkyTable::Parse(
      std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
  if (!table) {
    core::LogWarning("{} — {} s'ouvre sans ciel", core::Describe(table.error()), map_name);
    return {};
  }
  return table->LookOf(map_name);
}

}  // namespace ro::render

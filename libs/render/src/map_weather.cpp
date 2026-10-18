#include "ro/render/map_weather.h"

#include <expected>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/log.h"
#include "ro/vfs/encoding.h"

namespace ro::render {

namespace {

constexpr int kSupportedVersion = 1;
constexpr std::string_view kMapFileSuffix = ".rsw";

core::Error Refused(std::string detail) {
  return core::Error{core::ErrorCode::kCorruptData,
                     std::string(kMapWeatherPath) + " : " + std::move(detail)};
}

// Un champ texte d'une ligne, en CP949, ou une exception.
[[nodiscard]] std::string ArchivedField(const YAML::Node& line, const char* field) {
  const YAML::Node value = line[field];
  if (!value) {
    throw YAML::KeyNotFound(line.Mark(), std::string(field));
  }
  auto archived = vfs::Utf8ToCp949(value.as<std::string>());
  if (!archived) {
    throw YAML::RepresentationException(value.Mark(), "un texte qui ne s'écrit pas en CP949");
  }
  return std::move(*archived);
}

}  // namespace

core::Result<std::vector<WeatherLine>> ParseMapWeather(std::string_view yaml_text) {
  std::vector<WeatherLine> lines_read;
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
    const YAML::Node weather = whole["weather"];
    if (!weather || !weather.IsSequence()) {
      return std::unexpected(Refused("« weather » n'est pas une liste"));
    }
    for (const YAML::Node& line : weather) {
      lines_read.push_back(WeatherLine{ArchivedField(line, "map") + std::string(kMapFileSuffix),
                                       ArchivedField(line, "weather")});
    }
  } catch (const YAML::Exception& failure) {
    return std::unexpected(Refused(failure.what()));
  }
  return lines_read;
}

std::vector<std::uint32_t> MapWeatherEffects(std::span<const WeatherLine> lines,
                                             std::string_view map_name) {
  std::vector<std::uint32_t> effects;
  const std::string file = std::string(map_name) + std::string(kMapFileSuffix);
  for (const WeatherLine& line : lines) {
    if (line.map_file != file) {
      continue;
    }
    if (line.weather == kSnowWeather) {
      effects.push_back(kSnowEffect);
    } else if (line.weather == kSakuraWeather) {
      effects.push_back(kSakuraEffect);
    } else if (line.weather == kFireworksWeather) {
      effects.push_back(kFireworksOpeningEffect);
      effects.insert(effects.end(), kFireworksRockets, kFireworksRocketEffect);
    }
  }
  // LUTIE, APRÈS LA TABLE : le client vérifie ses deux cartes une fois la
  // table parcourue.
  if (map_name == kLutieMap || map_name == kLutieFieldMap) {
    effects.push_back(kSnowEffect);
  }
  return effects;
}

std::vector<std::uint32_t> LoadMapWeather(vfs::Vfs& vfs, std::string_view map_name) {
  std::vector<WeatherLine> lines_read;
  if (auto bytes = vfs.Read(kMapWeatherPath)) {
    auto parsed = ParseMapWeather(
        std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
    if (parsed) {
      lines_read = std::move(*parsed);
    } else {
      core::LogWarning("{} — {} n'a que la météo du client", core::Describe(parsed.error()),
                       map_name);
    }
  }
  return MapWeatherEffects(lines_read, map_name);
}

}  // namespace ro::render

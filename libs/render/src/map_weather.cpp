#include "ro/render/map_weather.h"

#include <string>

#include "ro/core/ascii.h"

namespace ro::render {

namespace {

constexpr char kTokenEnd = '#';
constexpr char kCarriageReturn = '\r';
constexpr char kLineFeed = '\n';
constexpr std::string_view kCommentMark = "//";
constexpr std::string_view kMapFileSuffix = ".rsw";

// Le mot-clé de la météo, et combien de jetons il prend après lui.
constexpr std::string_view kWeatherKeyword = "weather";
constexpr std::size_t kWeatherArguments = 2;

// Découpe le texte en jetons, comme le lecteur du client : un jeton finit à son
// `#`, une fin de ligne jette ce qui n'a pas été terminé, et un début de jeton
// en `//` fait taire les `#` jusqu'à la fin de la ligne.
[[nodiscard]] std::vector<std::string_view> Tokens(std::string_view text) {
  std::vector<std::string_view> tokens;
  std::size_t start = 0;
  for (std::size_t rank = 0; rank < text.size(); ++rank) {
    const char lu = text[rank];
    if (lu == kCarriageReturn || lu == kLineFeed) {
      start = rank + 1;
    } else if (lu == kTokenEnd && !text.substr(start).starts_with(kCommentMark)) {
      tokens.push_back(text.substr(start, rank - start));
      start = rank + 1;
    }
  }
  return tokens;
}

}  // namespace

std::vector<WeatherLine> ParseEtcInfoWeather(std::string_view text) {
  std::vector<WeatherLine> lines_read;
  const std::vector<std::string_view> tokens = Tokens(text);
  for (std::size_t rank = 0; rank < tokens.size(); ++rank) {
    const std::string_view word = tokens[rank];
    if (word == kWeatherKeyword) {
      if (rank + kWeatherArguments >= tokens.size()) {
        break;
      }
      lines_read.push_back(WeatherLine{core::AsciiLower(tokens[rank + 1]),
                                       core::AsciiLower(tokens[rank + kWeatherArguments])});
      rank += kWeatherArguments;
    }
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
  // LUTIE, APRÈS LE FICHIER : le client vérifie ses deux cartes une fois la
  // table parcourue.
  if (map_name == kLutieMap || map_name == kLutieFieldMap) {
    effects.push_back(kSnowEffect);
  }
  return effects;
}

std::vector<std::uint32_t> LoadMapWeather(vfs::Vfs& vfs, std::string_view map_name) {
  std::vector<WeatherLine> lines_read;
  if (auto bytes = vfs.Read(std::string(kEtcInfoPath))) {
    const std::string_view text_view(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    lines_read = ParseEtcInfoWeather(text_view);
  }
  return MapWeatherEffects(lines_read, map_name);
}

}  // namespace ro::render

// LA MÉTÉO D'UNE CARTE : la table convertie des lignes `weather`
// d'`etcinfo.txt`, et la neige que le client pose de lui-même sur Lutie.

#include "ro/render/map_weather.h"

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::render::MapWeatherEffects;
using ro::render::ParseMapWeather;
using ro::render::WeatherLine;

namespace {

// Ce que la conversion tire du fichier de l'installation, et d'un écran de
// chargement qui ne mange pas son argument :
//
//   weather#comodo.rsw#pokjuk#
//   weather#prontera.rsw#sakura#
//   //weather#pay_arche.rsw#sakura#
//   //weather#prontera.rsw#snow#
//   loadingscreen#weather#Geffen.RSW#SNOW#
//   weather#geffen.rsw#weather#payon.rsw#snow#
//   //file end
constexpr std::string_view kConverted =
    "version: 1\n"
    "weather:\n"
    "- {map: comodo, weather: pokjuk}\n"
    "- {map: prontera, weather: sakura}\n"
    "- {map: geffen, weather: snow}\n"
    "- {map: geffen, weather: weather}\n";

[[nodiscard]] std::vector<WeatherLine> Parsed(std::string_view yaml_text) {
  auto lines = ParseMapWeather(yaml_text);
  REQUIRE(lines.has_value());
  return std::move(*lines);
}

}  // namespace

TEST_CASE("les lignes de la table gardent leur ordre et leur extension", "[render][map_weather]") {
  const std::vector<WeatherLine> lines = Parsed(kConverted);
  REQUIRE(lines.size() == 4);
  CHECK(lines[0].map_file == "comodo.rsw");
  CHECK(lines[0].weather == "pokjuk");
  CHECK(lines[1].map_file == "prontera.rsw");
  CHECK(lines[1].weather == "sakura");
  CHECK(lines[2].map_file == "geffen.rsw");
  CHECK(lines[3].weather == "weather");
}

TEST_CASE("une table mal formée est refusée", "[render][map_weather]") {
  CHECK_FALSE(ParseMapWeather("version: 1\nweather: {a: b}\n").has_value());
  CHECK_FALSE(ParseMapWeather("version: 1\nweather:\n- {map: a}\n").has_value());
  CHECK_FALSE(ParseMapWeather("version: 2\nweather: []\n").has_value());
  CHECK(ParseMapWeather("version: 1\nweather: []\n")->empty());
}

TEST_CASE("chaque temps pose son effet sur la bonne carte", "[render][map_weather]") {
  const std::vector<WeatherLine> lines = Parsed(kConverted);
  CHECK(MapWeatherEffects(lines, "prontera") ==
        std::vector<std::uint32_t>{ro::render::kSakuraEffect});
  CHECK(MapWeatherEffects(lines, "payon").empty());
  // Deux lignes pour une carte : chacune pose son effet, et « weather » n'en
  // est pas un.
  CHECK(MapWeatherEffects(lines, "geffen") == std::vector<std::uint32_t>{ro::render::kSnowEffect});
  // Les feux d'artifice : l'ouverture, puis cent soixante fusées.
  const std::vector<std::uint32_t> comodo = MapWeatherEffects(lines, "comodo");
  REQUIRE(comodo.size() == 1 + ro::render::kFireworksRockets);
  CHECK(comodo.front() == ro::render::kFireworksOpeningEffect);
  CHECK(comodo.back() == ro::render::kFireworksRocketEffect);
}

TEST_CASE("Lutie a la neige sans que rien ne la déclare", "[render][map_weather]") {
  const std::vector<std::uint32_t> snow{ro::render::kSnowEffect};
  CHECK(MapWeatherEffects({}, "xmas") == snow);
  CHECK(MapWeatherEffects({}, "xmas_fild01") == snow);
  CHECK(MapWeatherEffects({}, "xmas_dun01").empty());
}

TEST_CASE("un temps inconnu ne pose rien", "[render][map_weather]") {
  const std::vector<WeatherLine> lines =
      Parsed("version: 1\nweather:\n- {map: prontera, weather: rain}\n");
  CHECK(MapWeatherEffects(lines, "prontera").empty());
}

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// ── LA MÉTÉO D'UNE CARTE ────────────────────────────────────────────────────
//
// La neige, les pétales de sakura, les feux d'artifice : des effets ordinaires
// que le client pose sur le JOUEUR à son entrée sur la carte, et qui le suivent.
// Le serveur n'y est pour rien — il ne les envoie jamais.
//
// Deux sources, dans cet ordre :
//   · la table `map_weather.yml`, que `scripts/yaml_tables/install_text_tables.py`
//     tire des lignes `weather` de `data\etcinfo.txt` de l'installation :
//
//         version: 1
//         weather:
//         - {map: comodo, weather: pokjuk}
//
//   · une règle ÉCRITE DANS LE CLIENT : les deux cartes de Lutie ont la neige,
//     sans que rien ne le déclare.
//
// ⚠ LA MÉTÉO NE SE LÈVE QUE SI LES EFFETS SONT MONTRÉS : `/effect` coupé à
// l'entrée, le client ne la pose pas du tout. C'est à l'appelant de le lire.

inline constexpr std::string_view kMapWeatherPath = "data\\yaml_files\\map_weather.yml";

// Les deux cartes où il neige toujours, telles que le client les nomme.
inline constexpr std::string_view kLutieMap = "xmas";
inline constexpr std::string_view kLutieFieldMap = "xmas_fild01";

// Les temps que le client sait jouer, et l'effet de chacun.
inline constexpr std::string_view kSnowWeather = "snow";
inline constexpr std::string_view kSakuraWeather = "sakura";
inline constexpr std::string_view kFireworksWeather = "pokjuk";
inline constexpr std::uint32_t kSnowEffect = 162;
inline constexpr std::uint32_t kSakuraEffect = 163;
// LES FEUX D'ARTIFICE : un effet qui ouvre le spectacle, puis cent soixante
// fusées tirées d'un coup, chacune tirant sa propre place.
inline constexpr std::uint32_t kFireworksOpeningEffect = 301;
inline constexpr std::uint32_t kFireworksRocketEffect = 297;
inline constexpr std::size_t kFireworksRockets = 160;

// Une ligne de la table : la carte, suffixe `.rsw` compris, et le temps. Les
// deux en minuscules, comme le client les range.
struct WeatherLine {
  std::string map_file;
  std::string weather;
};

// Le texte de `map_weather.yml`, ses lignes dans l'ordre. Refuse un texte qui
// n'est pas du YAML, dont `version` n'est pas 1, ou dont une ligne n'a pas sa
// carte et son temps. Les noms se rendent en CP949, comme l'installation les
// écrit.
[[nodiscard]] core::Result<std::vector<WeatherLine>> ParseMapWeather(std::string_view yaml_text);

// Les effets à poser sur le joueur à son entrée sur cette carte, dans l'ordre
// où le client les lance. `map_name` est le nom sans `.rsw`.
//
// ⚠ UN TEMPS INCONNU NE POSE RIEN, et c'est la règle du client : il ne
// reconnaît que ses trois noms.
[[nodiscard]] std::vector<std::uint32_t> MapWeatherEffects(std::span<const WeatherLine> lines,
                                                           std::string_view map_name);

// Lit la table et rend les effets de cette carte. Une table absente ou
// illisible ne retire que la première source : Lutie garde sa neige.
[[nodiscard]] std::vector<std::uint32_t> LoadMapWeather(vfs::Vfs& vfs, std::string_view map_name);

}  // namespace ro::render

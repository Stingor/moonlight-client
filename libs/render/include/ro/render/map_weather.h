#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/vfs/vfs.h"

namespace ro::render {

// ── LA MÉTÉO D'UNE CARTE ────────────────────────────────────────────────────
//
// La neige, les pétales de sakura, les feux d'artifice : des effets ordinaires
// que le client pose sur le JOUEUR à son entrée sur la carte, et qui le suivent.
// Le serveur n'y est pour rien — il ne les envoie jamais.
//
// Deux sources, dans cet ordre :
//   · le fichier `etcinfo.txt` de l'installation, dont chaque ligne
//     `weather#<carte>.rsw#<temps>#` nomme le temps d'une carte ;
//   · une règle ÉCRITE DANS LE CLIENT : les deux cartes de Lutie ont la neige,
//     sans que rien ne le déclare.
//
// ⚠ LA MÉTÉO NE SE LÈVE QUE SI LES EFFETS SONT MONTRÉS : `/effect` coupé à
// l'entrée, le client ne la pose pas du tout. C'est à l'appelant de le lire.

inline constexpr std::string_view kEtcInfoPath = "data\\etcinfo.txt";

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

// Une ligne `weather` du fichier : la carte, suffixe `.rsw` compris, et le
// temps. Les deux en minuscules, comme le client les range.
struct WeatherLine {
  std::string map_file;
  std::string weather;
};

// Les lignes `weather` du texte d'`etcinfo.txt`.
//
// Le fichier est une suite de JETONS, chacun TERMINÉ par un `#` — deux `#` de
// suite en font un vide. Ce qui suit le dernier `#` d'une ligne ne compte pas,
// et une ligne qui commence par `//` n'en donne aucun : ses `#` n'y coupent
// plus rien. `weather` prend ses deux arguments dans les jetons qui suivent, et
// la lecture reprend après eux. ⚠ LES AUTRES MOTS-CLÉS NE MANGENT RIEN :
// `loadingscreen` et `event` lisent le jeton suivant sans le consommer, si bien
// qu'il est relu comme un mot-clé à son tour. Les mots-clés se comparent à la
// casse près ; la carte et le temps, eux, se rangent en minuscules.
[[nodiscard]] std::vector<WeatherLine> ParseEtcInfoWeather(std::string_view text);

// Les effets à poser sur le joueur à son entrée sur cette carte, dans l'ordre
// où le client les lance. `map_name` est le nom sans `.rsw`.
//
// ⚠ UN TEMPS INCONNU NE POSE RIEN, et c'est la règle du client : il ne
// reconnaît que ses trois noms.
[[nodiscard]] std::vector<std::uint32_t> MapWeatherEffects(std::span<const WeatherLine> lines,
                                                           std::string_view map_name);

// Lit `etcinfo.txt` et rend les effets de cette carte. Un fichier absent ne
// retire que la première source : Lutie garde sa neige.
[[nodiscard]] std::vector<std::uint32_t> LoadMapWeather(vfs::Vfs& vfs, std::string_view map_name);

}  // namespace ro::render

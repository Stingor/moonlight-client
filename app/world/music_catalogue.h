#pragma once

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/formats/bgm_table.h"

// LES MUSIQUES QUE LE LECTEUR PROPOSE : celles de l'installation, et les cartes
// qui jouent chacune.
namespace moonclient {

// Le dossier des musiques, tel que la table du jeu le nomme.
inline constexpr std::string_view kMusicFolder = "bgm";

struct MusicTrack {
  // Le nom sous lequel le jeu la désigne, « bgm\08.mp3 » : c'est celui que
  // `MapMusic` joue, et que le mélangeur rend pour le morceau en cours.
  std::string game_path;
  // Le fichier sans dossier ni extension, « 08 ».
  std::string label;
  // Les cartes qui la jouent, en minuscules, dans l'ordre alphabétique. Vide
  // pour un morceau qu'aucune carte ne déclare.
  std::vector<std::string> maps;
};

// LA MÊME MUSIQUE SOUS DEUX ÉCRITURES ? La table écrit « bgm\08.mp3 », un
// fichier de l'installation peut s'appeler `BGM/08.MP3` : la casse et le
// séparateur ne distinguent pas deux morceaux.
[[nodiscard]] bool SameMusic(std::string_view left, std::string_view right) noexcept;

// LE CATALOGUE, à partir des noms de fichiers du dossier des musiques et de la
// table des cartes.
//
// ⚠ UN MORCEAU QUE LA TABLE NOMME SANS QU'IL SOIT SUR LE DISQUE N'Y FIGURE PAS :
// le lecteur ne propose que ce qu'il peut jouer. Celui qu'aucune carte ne
// déclare y figure, lui, sans carte — c'est même ce qu'on vient y chercher.
//
// Rangé par NUMÉRO quand le nom en est un, « 9 » avant « 10 » ; les autres
// suivent, dans l'ordre de leur nom.
[[nodiscard]] std::vector<MusicTrack> CatalogueMusic(std::span<const std::string> file_names,
                                                     const ro::formats::BgmTable& table);

// Le catalogue de l'installation sous `client_root`. Vide sans dossier des
// musiques.
[[nodiscard]] std::vector<MusicTrack> ListMusicTracks(const std::filesystem::path& client_root,
                                                      const ro::formats::BgmTable& table);

}  // namespace moonclient

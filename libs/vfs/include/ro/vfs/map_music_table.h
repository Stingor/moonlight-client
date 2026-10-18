#pragma once

#include <string_view>

#include "ro/core/error.h"
#include "ro/formats/bgm_table.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LA MUSIQUE DE CHAQUE CARTE : `data\yaml_files\map_music.yml`, que
// `scripts/yaml_tables/install_text_tables.py` tire de `data\mp3nametable.txt`
// de l'installation :
//
//     version: 1
//     maps:
//       prontera: bgm\08.mp3
//
// La carte est nommée en minuscules et sans extension ; le fichier est relatif
// à la racine du client. Voir `formats::BgmTable`.
inline constexpr std::string_view kMapMusicPath = "data\\yaml_files\\map_music.yml";

// Le texte de `map_music.yml`. Refuse un texte qui n'est pas du YAML, dont
// `version` n'est pas 1, sans table `maps`, ou dont un nom ne s'écrit pas en
// CP949. Les noms se rendent en CP949, comme l'installation les écrit.
[[nodiscard]] core::Result<formats::BgmTable> ParseMapMusic(std::string_view yaml_text);

// La table de l'installation. ABSENTE, une erreur `kNotFound` : c'est à
// l'appelant de jouer alors toute carte en silence.
[[nodiscard]] core::Result<formats::BgmTable> ReadMapMusic(Vfs& vfs);

}  // namespace ro::vfs

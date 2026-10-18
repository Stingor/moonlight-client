#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LES MOTS QUE LE CLIENT MASQUE : `data\yaml_files\banned_words.yml`, que
// `scripts/yaml_tables/install_text_tables.py` tire de `data\manner.txt` de
// l'installation :
//
//     version: 1
//     words:
//     - un mot
//
// Les mots sont en UTF-8, dans l'ordre où le masque les essaie. Voir
// `sim::BannedWords`.
inline constexpr std::string_view kBannedWordsPath = "data\\yaml_files\\banned_words.yml";

// Le texte de `banned_words.yml`. Refuse un texte qui n'est pas du YAML, dont
// `version` n'est pas 1, ou dont `words` n'est pas une liste de textes.
[[nodiscard]] core::Result<std::vector<std::string>> ParseBannedWordList(
    std::string_view yaml_text);

// ABSENTE, la table rend une liste vide : rien ne se masque. Illisible, une
// erreur.
[[nodiscard]] core::Result<std::vector<std::string>> ReadBannedWordList(Vfs& vfs);

}  // namespace ro::vfs

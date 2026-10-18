#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LES RÉPLIQUES du barde et de la danseuse.
//
// Deux compétences font DIRE quelque chose au personnage plutôt que de dessiner
// quoi que ce soit : la plaisanterie glaciale et le cri. Le client tire une
// réplique au sort et l'envoie au serveur comme une parole ordinaire.
//
// Les tables sont tirées par `scripts/yaml_tables/install_text_tables.py` de
// `data\BA_frostjoke.txt` et `data\DC_scream.txt` de l'installation :
//
//     version: 1
//     title: FROST JOKE
//     lines:
//     - Me..me..mememe...
//
// Le titre ne se dit jamais.
inline constexpr std::string_view kFrostJokeLinesPath = "data\\yaml_files\\frost_joke_lines.yml";
inline constexpr std::string_view kScreamLinesPath = "data\\yaml_files\\scream_lines.yml";

struct TalkLines {
  std::vector<std::string> lines;

  // LA RÉPLIQUE NUMÉRO `number`, comptée depuis UN, ou vide s'il n'y en a pas.
  // Zéro ne désigne rien : c'est le rang du titre, que le client n'atteint pas.
  [[nodiscard]] std::string_view Line(std::size_t number) const;
};

// Le texte d'une des deux tables. Refuse un texte qui n'est pas du YAML, dont
// `version` n'est pas 1, ou dont `lines` n'est pas une liste de textes.
[[nodiscard]] core::Result<TalkLines> ParseTalkLines(std::string_view yaml_text,
                                                     std::string_view table_path);

// La table `table_path` de l'installation. ABSENTE, une erreur `kNotFound`.
[[nodiscard]] core::Result<TalkLines> ReadTalkLines(Vfs& vfs, std::string_view table_path);

}  // namespace ro::vfs

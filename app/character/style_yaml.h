#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

#include <yaml-cpp/yaml.h>

#include "ro/sim/style_code.h"

namespace moonclient {

// LES FICHIERS DE STYLE DU JOUEUR, lus et écrits d'une seule façon : le magasin
// de l'éditeur — préréglages et brouillon, voir `style_store.h` — et la mémoire
// de la tablée, voir `character_styles.h`.
//
// 🔴 ILS RANGENT DES CODES, la même chaîne que le presse-papiers et que la base
// du serveur — voir `ro/sim/style_code.h`. Un code d'une autre version ne se lit
// pas, ici comme ailleurs : le format refuse la migration.
//
// ⚠ UN FICHIER ABÎMÉ NE DOIT PAS EMPÊCHER DE JOUER. Lire rend rien plutôt que de
// lever, et le dit au journal ; un fichier absent est le cas NORMAL de la
// première partie, et ne se dit pas.

// Où vit un fichier de style : dans le dossier des sauvegardes de
// l'installation.
[[nodiscard]] std::filesystem::path StyleFilePath(const std::filesystem::path& client_root,
                                                  std::string_view file_name);

// La racine d'un fichier de style, si c'est une table ; rien sinon.
[[nodiscard]] std::optional<YAML::Node> ReadStyleFile(const std::filesystem::path& path,
                                                      std::string_view subject);

// Écrit `header` puis le document, d'une écriture atomique, en créant le dossier
// au besoin. Rend faux, après l'avoir dit au journal, si le fichier n'a pas pu
// être écrit.
[[nodiscard]] bool WriteStyleFile(const std::filesystem::path& path, std::string_view header,
                                  const YAML::Emitter& document, std::string_view subject);

// Le code que porte ce nœud, ou rien : absent, autre chose qu'un scalaire, ou
// d'une version que ce client ne lit pas.
[[nodiscard]] std::optional<ro::sim::StyleCode> StyleCodeAt(const YAML::Node& node);

}  // namespace moonclient

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// LES CARTES INTÉRIEURES : celles où la caméra se resserre — recul et altitude
// bornés de près, lacet tenu autour de 45°.
//
// La table est `data\yaml_files\indoor_maps.yml`, que
// `scripts/yaml_tables/install_text_tables.py` tire de
// `data\indoorrswtable.txt` de l'installation :
//
//     version: 1
//     maps:
//     - 1@infi
//     - moc_para0a
//
// Chaque nom est déjà la CLÉ de table de la caméra, sans son extension : en
// minuscules, le numéro d'une carte d'instance retiré.
inline constexpr std::string_view kIndoorMapsPath = "data\\yaml_files\\indoor_maps.yml";

// Le texte d'`indoor_maps.yml`. Refuse un texte qui n'est pas du YAML, dont
// `version` n'est pas 1, ou dont `maps` n'est pas une liste de noms. Les noms
// se rendent en CP949, comme l'installation les écrit.
[[nodiscard]] core::Result<std::vector<std::string>> ParseIndoorMaps(std::string_view yaml_text);

// ABSENTE, la table rend une liste vide : toute carte garde les bornes du
// plein air. Illisible, une erreur.
[[nodiscard]] core::Result<std::vector<std::string>> ReadIndoorMaps(Vfs& vfs);

}  // namespace ro::vfs

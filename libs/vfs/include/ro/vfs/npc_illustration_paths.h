#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ro::vfs {

// OÙ CHERCHER L'ILLUSTRATION QU'UN SCRIPT NOMME (`cutin`), dans l'ordre : le
// premier chemin que l'installation porte est le bon.
//
// Le nom arrive du serveur en CP949, et les chemins rendus le sont aussi : on
// les passe tels quels à `GameImages::Get`.
//
// ⚠ DEUX DOSSIERS, comme chez le client d'origine : `유저인터페이스\illust\`, puis
// `유저인터페이스\cardbmp\`, où il cherche ensuite.
//
// 🔴 UN NOM SANS EXTENSION SE COMPLÈTE, et c'est mieux que le client d'origine :
// lui ne reconnaît le type d'une image qu'à son extension, et n'affiche rien
// pour `cutin "16lei_01"` — que des milliers de scripts écrivent pourtant. On
// essaie `.bmp`, le format des illustrations anciennes, puis `.png`, celui des
// récentes.
[[nodiscard]] std::vector<std::string> NpcIllustrationPaths(std::string_view cp949_name);

}  // namespace ro::vfs

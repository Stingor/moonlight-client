#pragma once

#include <string>
#include <string_view>

#include "ro/core/error.h"

namespace ro::vfs {

// Transcodage entre l'UTF-8 — ce que manipule tout le reste du client — et le
// CP949, dans lequel sont écrits les noms de fichiers des archives RO.
//
// Cette frontière existe à UN seul endroit, ici. Au-dessus de `vfs`, plus rien
// ne manipule de CP949 ; en dessous, `formats` n'indexe que des octets bruts et
// ignore qu'un encodage existe. Laisser fuir le CP949 dans les couches hautes,
// c'est se condamner à des noms illisibles et à des recherches qui échouent
// selon la locale de la machine.
[[nodiscard]] core::Result<std::string> Utf8ToCp949(std::string_view utf8);
[[nodiscard]] core::Result<std::string> Cp949ToUtf8(std::string_view cp949);

// Variantes tolérantes, pour l'affichage et la journalisation : un octet
// intraduisible devient '?' au lieu de faire échouer l'appel. À ne JAMAIS
// utiliser pour construire une clé de recherche.
[[nodiscard]] std::string Cp949ToUtf8Lossy(std::string_view cp949);

}  // namespace ro::vfs

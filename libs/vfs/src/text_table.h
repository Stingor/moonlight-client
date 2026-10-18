#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// ── LES TABLES TEXTE DE L'INSTALLATION ───────────────────────────────────────
//
// Plusieurs fichiers du client sont des tables plates de la même forme :
// `cardprefixnametable.txt`, `cardpostfixnametable.txt`,
// `num2cardillustnametable.txt` — une ligne par entrée, `<identifiant>#<champ>#`,
// et des lignes de commentaire `//` que rien ne distingue d'une ligne vide.
//
// ⚠ PRIVÉ À `vfs` : cet en-tête vit dans `src/`, pas dans `include/`. Ce n'est
// pas une bibliothèque de texte, c'est le peu que ces lecteurs partagent, et
// aucune autre couche n'a de raison de le voir.

// Découpe en lignes, sans le retour chariot de Windows. La dernière ligne compte
// même sans fin de ligne.
[[nodiscard]] std::vector<std::string_view> SplitLines(std::string_view text);

// Lit un fichier de l'installation. Une ABSENCE rend des octets vides plutôt
// qu'une erreur : ces tables ornent, elles ne conditionnent pas.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> ReadOptionalFile(Vfs& vfs,
                                                                       std::string_view path);

// L'identifiant en tête d'une ligne, ou rien si le champ n'est pas entièrement
// fait de chiffres.
//
// ⚠ C'EST AUSSI CE QUI ÉCARTE LES COMMENTAIRES. Une ligne `// 2011-11-18 …` n'a
// pas de tête numérique, et l'installation en porte quatre-vingt-deux dans la
// seule table des illustrations : les reconnaître par leurs deux barres serait
// une seconde règle pour le même effet.
[[nodiscard]] bool ParseTableId(std::string_view field, std::uint32_t& out);

// Le champ qui suit l'identifiant, entre les deux dièses, ou rien si la ligne
// n'en porte pas. Rend `false` quand la ligne n'est pas une entrée.
[[nodiscard]] bool SplitTableRow(std::string_view line, std::uint32_t& id, std::string_view& field);

}  // namespace ro::vfs

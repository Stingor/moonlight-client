#pragma once

// L'EMBLÈME DU GRADE D'UN RÉSULTAT de la recherche d'échoppes : la petite image
// que la fenêtre pose sur l'icône d'un objet gradé. Se juge sur un banc, sans
// fenêtre ni archive.

#include <cstdint>
#include <string>

namespace moonclient {

// Le chemin de l'emblème, sous la racine de l'habillage, en UTF-8 ; vide quand
// l'objet n'a pas de grade.
//
// Un grade POSITIF pose une image, numérotée par lui : de 1 (D) à 4 (A),
// celles que l'installation fournit. Un grade au-delà nomme une image absente,
// et la case reste sans emblème, comme chez le client d'origine.
[[nodiscard]] std::string StoreSearchGradeBadgePath(std::uint8_t grade);

}  // namespace moonclient

#pragma once

#include <string_view>

namespace ro::mapedit::detail {

// L'EN-TÊTE QUE L'ÉDITEUR ÉCRIT en tête de chaque document : la mention
// d'origine, le format de chaque section, et les commandes qui servent à
// écrire et à vérifier un document.
extern const std::string_view kMapEditHeader;

}  // namespace ro::mapedit::detail

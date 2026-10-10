#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"

namespace ro::formats {

// LES APPELS D'UNE FONCTION DE DONNÉES, dans un chunk Lua 5.1 compilé ou dans
// son source en clair — la signature tranche, comme chez le client.
//
// Certaines tables du client ne construisent pas de tables : elles définissent
// une fonction, et le client l'appelle après avoir posé ses propres fonctions
// d'insertion. Le corps de cette fonction n'est alors qu'une suite d'appels à
// arguments littéraux — `InsertEvolutionRecipeLGU(9001, 9069, 610, 10)` — et
// c'est cette suite qui porte la donnée.
//
// Le fichier ne doit faire que définir des fonctions ; la fonction demandée ne
// doit faire qu'appeler des globales nommées avec des arguments littéraux, puis
// au plus rendre des littéraux. Un saut, un calcul, un appel dont on garde le
// résultat ou dont un argument n'est pas littéral : le fichier se REFUSE. Le
// lire à moitié donnerait une donnée tronquée que personne ne verrait.
//
// Les appels sont rendus dans leur ordre, avec leurs arguments tels quels : le
// sens de chacun appartient à qui s'en sert.
[[nodiscard]] core::Result<std::vector<LuaCall>> ReadLuaFunctionCalls(
    std::span<const std::uint8_t> bytes, std::string_view function_name);

}  // namespace ro::formats

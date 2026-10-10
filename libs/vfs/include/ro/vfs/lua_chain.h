#pragma once

#include <span>
#include <string_view>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"
#include "ro/vfs/vfs.h"

namespace ro::vfs {

// PLUSIEURS TABLES DU CLIENT, ÉVALUÉES À LA SUITE DANS LE MÊME ÉTAT.
//
// ⚠ CE N'EST PAS UNE COMMODITÉ, C'EST LA SEULE LECTURE POSSIBLE. Ces fichiers
// se citent l'un l'autre : `skilltreeview` écrit `[JOBID.JT_SWORDMAN]` et
// `skillinfolist` écrit `[SKID.SM_BASH]`, des clés que seuls les fichiers
// précédents savent résoudre. Lu seul, un tel fichier se REFUSE — comme chez le
// client, qui enchaîne les siens dans un même état Lua.
//
// L'ordre de la liste est donc une dépendance, pas une préférence.
//
// Un fichier absent ou illisible refuse la chaîne entière, et l'erreur porte son
// nom : lire à moitié ferait porter à une compétence la fiche d'une autre.
[[nodiscard]] core::Result<formats::LuaTable> ReadLuaChain(
    Vfs& vfs, std::span<const std::string_view> internal_paths);

}  // namespace ro::vfs

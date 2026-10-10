#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"

// Aides partagées par les DEUX lecteurs de tables Lua — le chunk compilé et le
// texte en clair. Elles portent la sémantique d'affectation de Lua, qui doit
// être la même quel que soit le chemin par lequel une valeur arrive dans une
// table.
//
// Interne à ro_formats : rien ici n'est une promesse d'API.
namespace ro::formats::lua_internal {

// Pose une valeur dans une table, en REMPLAÇANT la clé si elle s'y trouve déjà.
//
// Une valeur nulle EFFACE la clé, comme en Lua. Garder l'entrée ferait répondre
// « présent » à une recherche portant sur ce que le fichier a justement retiré.
void SetByName(LuaTable& table, const std::string& name, LuaValue value);
void SetByIndex(LuaTable& table, double index, LuaValue value);

// Rend faux pour une clé d'un type que l'on ne sait pas retrouver.
//
// Une clé booléenne ou une table en guise de clé sont légales en Lua et
// n'apparaissent dans aucun fichier de données : les accepter demanderait de
// décider comment les retrouver, sans usage pour le faire.
[[nodiscard]] bool Assign(LuaTable& table, const LuaValue& key, LuaValue value);

// La valeur d'un nom, MODIFIABLE. Rend nullptr pour un nom absent.
//
// ⚠ ELLE EXISTE POUR UNE SEULE FORME : `Table[clé] = valeur`, qui écrit DANS une
// table déjà posée au lieu d'en construire une. Partout ailleurs on lit.
[[nodiscard]] LuaValue* FindMutable(LuaTable& table, std::string_view name);

// ── LE BYTECODE LUA 5.1, partagé par les lecteurs de chunks compilés ────────

// Découpage d'une instruction : six bits d'opcode, huit pour A, neuf pour B et
// pour C, ou dix-huit pour Bx. Les valeurs viennent de lopcodes.h.
constexpr std::uint32_t OpcodeOf(std::uint32_t word) { return word & 0x3F; }
constexpr std::uint32_t ArgA(std::uint32_t word) { return (word >> 6) & 0xFF; }
constexpr std::uint32_t ArgC(std::uint32_t word) { return (word >> 14) & 0x1FF; }
constexpr std::uint32_t ArgB(std::uint32_t word) { return (word >> 23) & 0x1FF; }
constexpr std::uint32_t ArgBx(std::uint32_t word) { return word >> 14; }

// Un argument B ou C désigne une CONSTANTE quand ce bit est posé, un registre
// sinon. C'est le « RK » de l'interpréteur.
inline constexpr std::uint32_t kConstantFlag = 0x100;

// Les opcodes que ces fichiers emploient. Tout le reste est refusé.
namespace opcodes {
enum Opcode : std::uint32_t {
  kOpMove = 0,
  kOpLoadK = 1,
  kOpLoadBool = 2,
  kOpLoadNil = 3,
  kOpGetGlobal = 5,
  kOpGetTable = 6,
  kOpSetGlobal = 7,
  kOpSetTable = 9,
  kOpNewTable = 10,
  kOpCall = 28,
  kOpReturn = 30,
  kOpSetList = 34,
  kOpClose = 35,
  kOpClosure = 36,
};
}  // namespace opcodes

// Une fonction compilée : ses instructions, ses constantes et les fonctions
// qu'elle définit.
struct Prototype {
  std::vector<std::uint32_t> code;
  std::vector<LuaValue> constants;
  std::vector<Prototype> protos;
  std::uint8_t upvalue_count = 0;
  std::uint8_t max_stack = 0;
};

// La fonction principale d'un chunk compilé, sans l'exécuter.
//
// Échoue si l'en-tête n'est pas celui de Lua 5.1 en 32 bits petit-boutiste, ou
// si le chunk ne se lit pas jusqu'au bout.
[[nodiscard]] core::Result<Prototype> ReadCompiledMain(std::span<const std::uint8_t> bytes);

}  // namespace ro::formats::lua_internal

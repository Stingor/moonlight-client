#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ro/core/error.h"

namespace ro::formats {

// Les tables de données du client, écrites en Lua et livrées COMPILÉES.
//
// POURQUOI UN LECTEUR PLUTÔT QU'UN INTERPRÉTEUR
//
// Le client range dans des .lub — du bytecode Lua 5.1 — tout ce qui relie un
// identifiant à un nom : le sprite d'un métier, le libellé d'un objet, la
// couleur du ciel d'une carte. Rien de tout cela ne se déduit des autres
// fichiers.
//
// Embarquer Lua serait la réponse générale, mais aucune version disponible ne
// charge le bytecode 5.1 : le format a changé après. Écrire un interpréteur
// complet serait démesuré — et inutile ici, parce que ces fichiers ne CALCULENT
// rien.
//
// Le relevé du chunk principal de MapSkyData.lub le montre : sept opcodes,
// 832 SETTABLE, 393 LOADK, 310 NEWTABLE, 167 SETLIST, 9 SETGLOBAL, 8 CLOSURE et
// un RETURN. Aucun saut, aucun appel, aucune arithmétique. Ce qu'il faut savoir
// exécuter tient donc en une page, et tout ce qui sortirait de ce cadre est
// REFUSÉ plutôt que deviné.
//
// DEUX FORMES, UNE SEULE GRAMMAIRE
//
// Le client charge indifféremment un fichier compilé ou un source en clair —
// son chargeur regarde la signature et rien d'autre — et les tables traduites
// que livrent les serveurs sont en CLAIR. Le lecteur de texte couvre donc la
// même grammaire de données que celui du bytecode : des affectations de
// globales, des constructeurs de tables, des références aux globales déjà
// lues. Une fonction est sautée et gardée opaque, et tout le reste — appel,
// calcul, variable locale — est refusé de la même façon qu'un opcode inconnu.
//
// Les deux lecteurs prennent les globales DÉJÀ LUES : le client enchaîne ses
// fichiers dans un même état, et jobName lit les identifiants que NPCIdentity
// a nommés juste avant. Lire un fichier seul, c'est lire des clés nulles.

struct LuaTable;

// Une valeur Lua.
//
// Une fonction est gardée comme une valeur OPAQUE : ces fichiers en définissent
// (huit dans MapSkyData), mais personne ne les appelle pour lire des données, et
// prétendre les évaluer demanderait justement l'interpréteur qu'on évite.
struct LuaValue {
  enum class Kind : std::uint8_t { kNil, kBoolean, kNumber, kString, kTable, kFunction };

  Kind kind = Kind::kNil;
  bool boolean = false;
  double number = 0.0;
  std::string text;
  std::shared_ptr<LuaTable> table;

  [[nodiscard]] bool is_nil() const noexcept { return kind == Kind::kNil; }
};

// Un APPEL de premier niveau, gardé TEL QUEL.
//
// ⚠ IL NE S'EXÉCUTE PAS, ET IL NE SE JETTE PAS NON PLUS. Certains de ces
// fichiers portent de la DONNÉE sous forme d'appels : `skilltreeview` nomme les
// onglets du grimoire par dix appels à `JobSkillTab.ChangeSkillTabName`, et rien
// d'autre ne le dit. Les sauter perdrait ces noms sans que personne ne le sache ;
// refuser le fichier perdrait aussi la grille, qui est dans le même.
//
// On garde donc l'appel : son nom et ses arguments, ceux qui se lisent sans
// calcul. À qui s'en sert d'en faire ce que le client en ferait.
//
// ⚠ EN CLAIR SEULEMENT. Un fichier COMPILÉ qui appelle se refuse toujours : ses
// opcodes d'appel sortent du cadre que le lecteur de bytecode sait exécuter, et
// les traduire demanderait l'interpréteur qu'on évite. Les tables livrées en
// clair — ce qui est le cas de celles-ci — passent par le lecteur de texte.
struct LuaCall {
  // Le nom complet, points compris : « JobSkillTab.ChangeSkillTabName ».
  std::string name;
  std::vector<LuaValue> arguments;
};

// Une table Lua : des clés nommées, des clés numérotées.
//
// Les deux sont rangées SÉPARÉMENT et dans leur ordre d'apparition. Lua ne
// distingue pas les unes des autres — une table mélange les deux librement — mais
// les fichiers du client s'en servent pour deux usages distincts : le nom pour un
// champ, le nombre pour un identifiant ou un rang. Les séparer rend la lecture
// directe, et l'ordre d'insertion rend le résultat reproductible.
//
// ⚠ Les deux listes se lisent librement mais ne se modifient que par les
// lecteurs : les index qui les accompagnent doivent les suivre. La table des
// objets du client compte vingt-sept mille entrées, et sans index chaque
// insertion parcourait tout ce qui précède.
// UN HACHAGE QUI ACCEPTE UNE VUE : chercher un champ par son nom ne doit pas
// construire une chaîne à chaque question — les lecteurs de tables en posent
// des centaines de milliers au démarrage.
struct LuaNameHash {
  using is_transparent = void;
  [[nodiscard]] std::size_t operator()(std::string_view name) const noexcept {
    return std::hash<std::string_view>{}(name);
  }
};

struct LuaTable {
  std::vector<std::pair<std::string, LuaValue>> by_name;
  std::vector<std::pair<double, LuaValue>> by_index;
  std::unordered_map<std::string, std::size_t, LuaNameHash, std::equal_to<>> slot_by_name;
  std::unordered_map<double, std::size_t> slot_by_index;
  // Les appels rencontrés, dans leur ordre. Seule la table des GLOBALES en
  // porte : un appel ne se fait qu'au premier niveau, jamais dans un
  // constructeur de table.
  std::vector<LuaCall> calls;

  // Rend nullptr si la clé est absente. Une clé absente est un cas NORMAL — un
  // champ facultatif, une carte sans ciel déclaré — et non une erreur.
  [[nodiscard]] const LuaValue* Find(std::string_view name) const;
  [[nodiscard]] const LuaValue* Find(double index) const;
};

// UN NOMBRE LUA LU COMME ENTIER : fini, sans partie décimale, et dans [0, most].
// Rien sinon.
//
// 🔴 C'EST LA SEULE CONVERSION PERMISE vers un identifiant. Un `static_cast`
// d'un double hors de la plage de l'entier visé est un comportement indéfini,
// et ces nombres viennent de fichiers que l'on ne contrôle pas : `ViewID =
// 70000` ou `[5e9]` rendaient au mieux un identifiant replié, qui écrasait
// l'entrée d'un autre.
[[nodiscard]] std::optional<std::uint32_t> LuaWholeNumber(double value, std::uint32_t most);

// Profondeur d'imbrication acceptée.
//
// Ces fichiers descendent à quatre ou cinq niveaux. Le plafond n'est pas là pour
// eux mais pour un fichier abîmé : sans lui, une table qui se contiendrait
// elle-même ferait tourner la lecture sans fin.
inline constexpr int kMaxLuaTableDepth = 32;

// CE QU'ON FAIT D'UNE CLÉ QUE RIEN N'A NOMMÉE.
//
// 🔴 REFUSER EST LE DÉFAUT, et il le reste : dans une table d'objets ou un arbre
// de compétences, une clé nulle veut dire que le fichier précédent n'a pas été lu
// — laisser passer donnerait une table à trous que personne ne verrait.
//
// ⚠ UN SEUL CAS DEMANDE L'AUTRE, et il est mesuré : `efstids.lub` décrit plus de
// statuts que les tables qui les nomment et les dessinent, et une installation
// dépareillée ferait sinon tomber tous les statuts pour une constante manquante.
//
// ⚠ LE CHOIX VAUT POUR LES DEUX FORMES, compilée comme en clair : « même
// contrat, même résultat ». Un fichier compilé refusait sinon en entier ce que
// sa version en clair aurait lu.
enum class UnnamedKey : std::uint8_t {
  // La clé nulle refuse le fichier.
  kRefuse,
  // La clé nulle saute son entrée, et la lecture continue.
  kSkipEntry,
};

// Évalue un chunk Lua 5.1 compilé qui ne fait que construire des tables.
//
// Rend la table des GLOBALES : ce que le fichier a nommé, prêt à être parcouru.
//
// Échoue si l'en-tête n'est pas celui de Lua 5.1 en 32 bits petit-boutiste, si le
// chunk emploie un opcode hors du cadre, ou si un index sort de sa table. Aucune
// de ces conditions ne se rattrape en devinant : un fichier qu'on ne comprend pas
// entièrement se refuse, sinon il se lit à moitié et personne ne le sait.
[[nodiscard]] core::Result<LuaTable> ReadLuaChunk(std::span<const std::uint8_t> bytes,
                                                  LuaTable globals = {},
                                                  UnnamedKey unnamed = UnnamedKey::kRefuse);

// Évalue un source Lua en clair qui ne fait que construire des tables.
//
// Même contrat, même résultat. Une clé qui se résout à nil — un identifiant
// absent des globales — refuse le fichier, comme Lua le ferait : le client
// n'aurait pas chargé cette table non plus, et la lire à moitié ferait porter
// à un monstre le sprite d'un autre. Les chaînes gardent leurs OCTETS : les
// noms coréens y sont en CP949, et c'est à qui s'en sert de le savoir.
//
// L'erreur porte le numéro de ligne : un fichier de cinq mille entrées ne se
// relit pas en entier pour trouver une virgule.
[[nodiscard]] core::Result<LuaTable> ReadLuaSource(std::string_view source, LuaTable globals = {},
                                                   UnnamedKey unnamed = UnnamedKey::kRefuse);

// Lit un fichier de tables, compilé ou en clair, selon sa signature.
//
// C'est le point d'entrée pour un fichier tiré du client : on ne sait pas
// d'avance sous quelle forme un serveur livre ses tables, et le client non
// plus.
[[nodiscard]] core::Result<LuaTable> ReadLuaFile(std::span<const std::uint8_t> bytes,
                                                 LuaTable globals = {},
                                                 UnnamedKey unnamed = UnnamedKey::kRefuse);

}  // namespace ro::formats

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"
#include "ro/vfs/vfs.h"

namespace ro::render {

// LES TITRES, et ils vivent dans les DONNÉES de l'installation.
//
// 🔴 UN TITRE N'A AUCUN TEXTE SUR LE FIL. Le serveur n'envoie qu'un ENTIER —
// `title_id` de `ZC_ACK_REQNAMEALL`, persisté dans `char.title_id` — et le
// libellé est à cent pour cent du côté du client : `title_tbl`, dans
// `titletable.lub`, que le global `GetTitleString(id)` interroge. Chercher le
// texte dans un paquet ou dans l'exécutable ne rend rien.
//
// Troisième table de ce genre après les effets de chapeau et les effets
// nommés : elle se lit à l'exécution, et un correctif de l'installation la fait
// grandir sans qu'on touche au code. ADR 0027.
inline constexpr std::string_view kTitleTablePath =
    "data\\luafiles514\\lua files\\datainfo\\titletable.lub";

// La globale sous laquelle le fichier range ses libellés, indexés par le numéro
// de titre lui-même.
inline constexpr std::string_view kTitleTableGlobal = "title_tbl";

// Le premier numéro que la table emploie — `TITLE_BASE` du serveur. Zéro veut
// donc dire « aucun titre », et non « le titre numéro zéro ».
inline constexpr std::int32_t kFirstTitleId = 1000;

// La table entière, triée par numéro.
class TitleTable {
 public:
  [[nodiscard]] static core::Result<TitleTable> Load(vfs::Vfs& vfs);

  // La MÊME lecture, à partir de globales déjà lues — la seule moitié qui se
  // mette sous test, puisqu'aucun asset du jeu n'est versé au dépôt.
  [[nodiscard]] static core::Result<TitleTable> Parse(const formats::LuaTable& globals);

  // Le libellé, ou vide. ⚠ UN NUMÉRO SANS LIBELLÉ N'EST PAS UNE ERREUR : le
  // serveur en connaît jusqu'à `TITLE_MAX`, et l'installation n'en traduit que
  // ce que son lot de données porte. Le client natif n'écrit alors rien non
  // plus.
  [[nodiscard]] std::string_view Find(std::int32_t title_id) const;

  struct Entry {
    std::int32_t title_id = 0;
    std::string label;
  };

  [[nodiscard]] std::span<const Entry> entries() const noexcept { return entries_; }

 private:
  std::vector<Entry> entries_;
};

}  // namespace ro::render

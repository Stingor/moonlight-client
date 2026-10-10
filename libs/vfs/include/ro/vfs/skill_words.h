#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/lua_table.h"

namespace ro::vfs {

// CE QU'UNE COMPÉTENCE FAIT, écrit par le jeu.
//
// 🔴 LE FIL N'EN PORTE RIEN. Le serveur envoie un identifiant, un rang et un
// coût ; ce que la compétence FAIT — sa portée, sa cible, ses conditions, le
// paragraphe de conseils qui suit — vit dans une table de l'installation, et
// nulle part ailleurs. Sans elle, le grimoire montre des icônes muettes.
//
// ⚠ CES TEXTES NE SONT PAS TRADUITS PAR NOUS : l'ADR 0034 a tranché, les
// descriptions du JEU viennent de l'installation du joueur. Ils
// n'entrent pas dans `lang/`, et `core::Tr` n'a rien à y voir.
//
// ⚠ LE BALISAGE EST CONSERVÉ TEL QUEL — `^RRGGBB`, `<NAVI>`, `<URL>`. C'est
// `ro/ui/rich_text.h` qui le lit ; le retirer ici déciderait à sa place, et
// perdrait les couleurs que la table pose exprès.

// LA TABLE DU CLIENT, et la clé sous laquelle chaque description est rangée.
//
// ⚠ ELLE S'INDEXE PAR NUMÉRO DE COMPÉTENCE, pas par nom : le fichier écrit
// `[SKID.AL_HEAL]`, et `SKID` — évaluée plus tôt dans la même chaîne — rend le
// numéro. C'est pour cela que la lecture n'a pas besoin des noms internes.
inline constexpr std::string_view kSkillDescriptGlobal = "SKILL_DESCRIPT";

class SkillWords {
 public:
  // Les lignes d'une compétence, balisage compris. VIDE pour une compétence que
  // la table ne porte pas — le cas est NORMAL : le serveur en accorde que le
  // client ne décrit pas.
  //
  // ⚠ LA VUE POINTE DANS LA TABLE, qui ne bouge plus une fois montée. Elle vit
  // donc aussi longtemps qu'elle.
  [[nodiscard]] std::span<const std::string> Of(std::uint16_t skill_id) const;

  [[nodiscard]] std::size_t skill_count() const noexcept { return by_skill_.size(); }

  // Monte la table à partir de globales DÉJÀ évaluées. Séparé de la lecture des
  // fichiers pour la même raison que l'arbre : elle se vérifie sans VFS ni
  // archive.
  [[nodiscard]] static core::Result<SkillWords> FromGlobals(const formats::LuaTable& globals);

 private:
  // OÙ LES LIGNES D'UNE COMPÉTENCE COMMENCENT, et combien il y en a.
  //
  // 🔴 TOUTES LES LIGNES DANS UN SEUL TAMPON, et non un vecteur par compétence :
  // dix-neuf cents descriptions de vingt lignes font dix-neuf cents petites
  // allocations qu'on ne relira jamais séparément.
  struct Block {
    std::size_t at = 0;
    std::size_t count = 0;
  };

  std::vector<std::string> lines_;
  std::unordered_map<std::uint16_t, Block> by_skill_;
};

}  // namespace ro::vfs

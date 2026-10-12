#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "ro/vfs/skill_info.h"

namespace ro::vfs {

// CE QU'UNE COMPÉTENCE FAIT, écrit par le jeu.
//
// 🔴 LE FIL N'EN PORTE RIEN. Le serveur envoie un identifiant, un rang et un
// coût ; ce que la compétence FAIT — sa portée, sa cible, ses conditions, le
// paragraphe de conseils qui suit — vit dans `skill_info.yml`, et nulle part
// ailleurs. Sans lui, le grimoire montre des icônes muettes.
//
// ⚠ CES TEXTES NE SONT PAS TRADUITS PAR NOUS : l'ADR 0034 a tranché, les
// descriptions du JEU viennent de l'installation (sa traduction ROenglishRE,
// versée dans la table). Ils n'entrent pas dans `lang/`, et `core::Tr` n'a rien
// à y voir.
//
// ⚠ LE BALISAGE EST CONSERVÉ TEL QUEL — `^RRGGBB`, `<NAVI>`, `<URL>`. C'est
// `ro/ui/rich_text.h` qui le lit ; le retirer ici déciderait à sa place, et
// perdrait les couleurs que la table pose exprès.
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

  // Monte la table à partir des fiches lues. Les lignes sont DÉPLACÉES hors des
  // fiches, qui restent sans description : un méga-octet et demi de texte ne se
  // copie pas pour rien. Une fiche sans ligne n'a pas de description.
  [[nodiscard]] static SkillWords FromRecords(std::span<SkillInfoRecord> records);

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

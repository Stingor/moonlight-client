#pragma once

#include <string>
#include <string_view>

namespace ro::core {

// ── LA CLÉ DE RECHERCHE ET DE TRI D'UN TEXTE UTF-8 ─────────────────────────
//
// Le texte replié (voir `case_fold.h`), puis ses lettres latines rendues à leur
// lettre de base : « États » et « etats » ont la même clé, et « Œuf » devient
// « oeuf ». C'est ce qu'un joueur tape dans un filtre, et c'est l'ordre d'un
// dictionnaire : comparées octet à octet, les capitales accentuées se rangeraient
// après « z ».
//
// ⚠ LES LIGATURES S'ÉCLATENT : « æ » → « ae », « œ » → « oe », « ĳ » → « ij »
// et « ß » → « ss ». Les signes diacritiques combinants (U+0300 à U+036F)
// disparaissent. Le reste — grec, cyrillique, coréen — garde son repli de
// casse seul.
//
// ⚠ UNE CLÉ, PAS UN TEXTE À MONTRER : elle se compare et se cherche, elle ne
// s'affiche jamais.
[[nodiscard]] std::string SearchKey(std::string_view utf8);

// `needle_key` se trouve-t-il dans `haystack_key` ? Deux clés déjà calculées :
// une clé vide se trouve partout.
[[nodiscard]] bool SearchKeyContains(std::string_view haystack_key, std::string_view needle_key);

}  // namespace ro::core

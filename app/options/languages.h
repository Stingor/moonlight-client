#pragma once

#include <span>
#include <string_view>

namespace moonclient {

// La langue par défaut : celle dans laquelle le code est écrit. ⚠ ELLE SUIT SON
// CATALOGUE hors de `game_options.h`, pour la même raison et parce que c'est le
// même fait — la première entrée de la liste ci-dessous.
inline constexpr std::string_view kDefaultLanguage = "fr";

// LES LANGUES QU'ON SAIT PARLER, et leur code.
//
// ⚠ LA PREMIÈRE EST LE FRANÇAIS et n'a pas de catalogue : c'est la langue dans
// laquelle le code est écrit, donc la CLÉ de toutes les autres. Voir
// `lang/*.txt`.
//
// ⚠ CE CATALOGUE VIVAIT DANS `game_options.h`, qui touchait son plafond de
// lignes. Il n'a rien à y faire : ce n'est pas un réglage, c'est la liste de ce
// que le client sait parler — et deux fichiers la lisent sans rien devoir aux
// réglages (l'écran de connexion, qui la propose avant toute partie).
struct Language {
  const char* code;
  const char* name;
};

[[nodiscard]] std::span<const Language> KnownLanguages();

}  // namespace moonclient

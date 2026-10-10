#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace ro::core {

// ── LA CASSE D'UN TEXTE UTF-8, hors de l'ASCII ─────────────────────────────
//
// Le REPLI DE CASSE SIMPLE d'Unicode : un point de code vers un seul point de
// code, de sorte que « ÉLODIE » et « Élodie » se replient sur le même texte. Il
// couvre l'ASCII, Latin-1, le Latin étendu A et B, l'alphabet phonétique, le
// grec, le cyrillique, le Latin étendu additionnel et le grec étendu : tout ce
// que CP1252 porte, et ce qu'un joueur tape en UTF-8.
//
// ⚠ « ß » RESTE « ß ». Le replier en « ss » est le repli COMPLET, qui allonge le
// texte ; « ẞ » se replie, lui, en « ß ». Et le « İ » turc ne se replie pas : son
// minuscule est un « i » suivi d'un point combinant.
//
// ⚠ À NE PAS CONFONDRE AVEC `AsciiLower`, qui sert aux noms de fichiers CP949 et
// ne doit justement toucher à aucun octet non ASCII.

// Une plage de la table : chaque point de code de `first` à `last`, un sur
// `stride`, se replie en lui-même plus `delta`.
struct CaseFoldRun {
  char32_t first = 0;
  char32_t last = 0;
  std::int32_t delta = 0;
  std::uint8_t stride = 1;
};

// La table engendrée, hors ASCII, triée par `first` ; les plages sont
// disjointes.
[[nodiscard]] std::span<const CaseFoldRun> CaseFoldRuns();

// Le repli d'un point de code ; lui-même s'il n'en a pas.
[[nodiscard]] char32_t FoldedCodePoint(char32_t code);

// Le texte replié, en UTF-8. Un octet qui ne forme pas une séquence valide est
// recopié tel quel : un texte mal encodé se compare encore à lui-même.
[[nodiscard]] std::string CaseFolded(std::string_view utf8);

// L'égalité à la casse près.
[[nodiscard]] bool EqualsFolded(std::string_view left, std::string_view right);

}  // namespace ro::core

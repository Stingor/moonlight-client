// LA CLÉ DE RECHERCHE D'UN TEXTE. Voir l'en-tête.

#include "ro/core/search_key.h"

#include <cstddef>

#include "ro/core/case_fold.h"
#include "ro/core/text.h"

namespace ro::core {
namespace {

// LATIN-1 SUPPLÉMENT, de « À » à « ÿ » : la lettre de base de chaque point de
// code. « - » garde le point de code tel quel (×, ÷, Þ, þ) ; « * » est une
// ligature, éclatée par `Ligature`.
constexpr char32_t kLatin1First = U'À';
constexpr std::string_view kLatin1Bases =
    "aaaaaa*ceeeeiiiidnooooo-ouuuuy-*"
    "aaaaaa*ceeeeiiiidnooooo-ouuuuy-y";

// LATIN ÉTENDU A, de « Ā » à « ſ », mêmes conventions ; la table s'arrête où
// commence le Latin étendu B.
constexpr char32_t kLatinAFirst = U'Ā';
constexpr char32_t kLatinBFirst = U'ƀ';
constexpr std::string_view kLatinABases =
    "aaaaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiiiiiii**jjkkklllllll"
    "lllnnnnnnnnnoooooo**rrrrrrssssssssttttttuuuuuuuuuuuuwwyyyzzzzzzs";

// ⚠ UNE LETTRE PAR POINT DE CODE, NI PLUS NI MOINS : une table décalée d'un cran
// rangerait chaque lettre sous sa voisine.
static_assert(kLatin1Bases.size() == kLatinAFirst - kLatin1First);
static_assert(kLatinABases.size() == kLatinBFirst - kLatinAFirst);

constexpr char kKeep = '-';
constexpr char kLigature = '*';

// Les signes diacritiques combinants : ils ne portent que l'accent.
constexpr char32_t kFirstCombiningMark = U'̀';
constexpr char32_t kLastCombiningMark = U'ͯ';

// La lettre de base, ou zéro quand le point de code se garde ou n'est pas une
// lettre latine accentuée.
[[nodiscard]] char BaseLetter(char32_t code) {
  if (code >= kLatin1First && code < kLatinAFirst) {
    return kLatin1Bases[code - kLatin1First];
  }
  if (code >= kLatinAFirst && code < kLatinBFirst) {
    return kLatinABases[code - kLatinAFirst];
  }
  return '\0';
}

[[nodiscard]] std::string_view Ligature(char32_t code) {
  switch (code) {
    case U'æ':
    case U'Æ':
      return "ae";
    case U'ß':
      return "ss";
    case U'ĳ':
    case U'Ĳ':
      return "ij";
    case U'œ':
    case U'Œ':
      return "oe";
    default:
      return {};
  }
}

}  // namespace

std::string SearchKey(std::string_view utf8) {
  const std::string folded = CaseFolded(utf8);
  std::string key;
  key.reserve(folded.size());
  std::size_t at = 0;
  while (at < folded.size()) {
    const std::size_t from = at;
    const char32_t code = NextCodePoint(folded, at);
    if (code >= kFirstCombiningMark && code <= kLastCombiningMark) {
      continue;
    }
    const char base = BaseLetter(code);
    if (base == kLigature) {
      key.append(Ligature(code));
    } else if (base != '\0' && base != kKeep) {
      key.push_back(base);
    } else {
      // ⚠ OCTET POUR OCTET, comme le repli : une séquence invalide ressort telle
      // qu'elle est entrée.
      key.append(folded, from, at - from);
    }
  }
  return key;
}

bool SearchKeyContains(std::string_view haystack_key, std::string_view needle_key) {
  return haystack_key.find(needle_key) != std::string_view::npos;
}

}  // namespace ro::core

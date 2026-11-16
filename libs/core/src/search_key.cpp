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

[[nodiscard]] bool IsDigit(char byte) { return byte >= '0' && byte <= '9'; }

// La suite de chiffres qui commence à `at`, sans ses zéros de tête ; `at`
// passe après elle.
[[nodiscard]] std::string_view DigitRun(std::string_view key, std::size_t& at) {
  while (at < key.size() && key[at] == '0') {
    ++at;
  }
  const std::size_t from = at;
  while (at < key.size() && IsDigit(key[at])) {
    ++at;
  }
  return key.substr(from, at - from);
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

int CompareSearchKeys(std::string_view left_key, std::string_view right_key) {
  std::size_t left_at = 0;
  std::size_t right_at = 0;
  while (left_at < left_key.size() && right_at < right_key.size()) {
    if (IsDigit(left_key[left_at]) && IsDigit(right_key[right_at])) {
      const std::string_view left_number = DigitRun(left_key, left_at);
      const std::string_view right_number = DigitRun(right_key, right_at);
      // Sans zéros de tête, le plus long est le plus grand.
      if (left_number.size() != right_number.size()) {
        return left_number.size() < right_number.size() ? -1 : 1;
      }
      if (const int order = left_number.compare(right_number); order != 0) {
        return order;
      }
      continue;
    }
    const auto left_byte = static_cast<unsigned char>(left_key[left_at]);
    const auto right_byte = static_cast<unsigned char>(right_key[right_at]);
    if (left_byte != right_byte) {
      return left_byte < right_byte ? -1 : 1;
    }
    ++left_at;
    ++right_at;
  }
  const bool left_done = left_at >= left_key.size();
  const bool right_done = right_at >= right_key.size();
  if (left_done != right_done) {
    return left_done ? -1 : 1;
  }
  // Égales au sens des nombres (« 07 » et « 7 ») : l'octet départage, pour
  // qu'un tri ne tienne jamais deux clés différentes pour une seule.
  return left_key.compare(right_key);
}

}  // namespace ro::core

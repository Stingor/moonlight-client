// LA CASSE D'UN TEXTE UTF-8. Voir l'en-tête.

#include "ro/core/case_fold.h"

#include <algorithm>

#include "ro/core/ascii.h"
#include "ro/core/text.h"

namespace ro::core {
namespace {

// Le dernier point de code ASCII : en deçà, `AsciiLower` suffit.
constexpr char32_t kLastAscii = 0x7F;

}  // namespace

char32_t FoldedCodePoint(char32_t code) {
  if (code <= kLastAscii) {
    return AsciiLower(static_cast<std::uint8_t>(code));
  }
  const std::span<const CaseFoldRun> runs = CaseFoldRuns();
  // La dernière plage qui commence au plus tard à `code`.
  const auto after = std::ranges::upper_bound(runs, code, {}, &CaseFoldRun::first);
  if (after == runs.begin()) {
    return code;
  }
  const CaseFoldRun& run = *(after - 1);
  if (code > run.last || (code - run.first) % char32_t{run.stride} != 0) {
    return code;
  }
  return static_cast<char32_t>(static_cast<std::int32_t>(code) + run.delta);
}

std::string CaseFolded(std::string_view utf8) {
  std::string folded;
  folded.reserve(utf8.size());
  std::size_t at = 0;
  while (at < utf8.size()) {
    const std::size_t from = at;
    const char32_t code = NextCodePoint(utf8, at);
    const char32_t target = FoldedCodePoint(code);
    // ⚠ CE QUI NE SE REPLIE PAS SE RECOPIE OCTET POUR OCTET : une séquence
    // invalide, lue comme U+FFFD, ressort donc telle qu'elle est entrée.
    if (target == code) {
      folded.append(utf8.substr(from, at - from));
    } else {
      AppendUtf8(folded, target);
    }
  }
  return folded;
}

bool EqualsFolded(std::string_view left, std::string_view right) {
  return CaseFolded(left) == CaseFolded(right);
}

}  // namespace ro::core

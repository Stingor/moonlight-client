#include "ro/core/text.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <system_error>
#include <utility>

namespace ro::core {

namespace {

// Le code : un accent circonflexe et SIX chiffres hexadécimaux, pas un de plus.
constexpr std::size_t kColorCodeLength = 7;

[[nodiscard]] bool IsHexDigit(char character) {
  return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') ||
         (character >= 'A' && character <= 'F');
}

[[nodiscard]] std::uint32_t HexValue(char character) {
  if (character <= '9') {
    return static_cast<std::uint32_t>(character - '0');
  }
  return static_cast<std::uint32_t>((character | 0x20) - 'a') + 10;
}

}  // namespace

std::vector<ColorRun> SplitColorRuns(std::string_view text) {
  std::vector<ColorRun> runs;
  ColorRun current;
  std::size_t start = 0;

  const auto close = [&](std::size_t end) {
    if (end > start) {
      current.text = text.substr(start, end - start);
      runs.push_back(current);
    }
  };

  for (std::size_t at = 0; at + kColorCodeLength <= text.size();) {
    if (text[at] != '^') {
      ++at;
      continue;
    }
    const std::string_view digits = text.substr(at + 1, kColorCodeLength - 1);
    if (!std::ranges::all_of(digits, IsHexDigit)) {
      // Un accent circonflexe qui n'annonce pas six chiffres est du TEXTE. Le
      // client le laisse passer, et un dialogue peut en contenir.
      ++at;
      continue;
    }
    close(at);
    std::uint32_t value = 0;
    for (const char digit : digits) {
      value = (value << 4) | HexValue(digit);
    }
    // ⚠ ZÉRO N'EST PAS DU NOIR : c'est le retour à la couleur par défaut.
    current.colored = value != 0;
    current.rgb = value;
    at += kColorCodeLength;
    start = at;
  }
  close(text.size());
  return runs;
}

std::optional<float> NumberAfterCommand(std::string_view line, std::string_view command) {
  if (!line.starts_with(command)) {
    return std::nullopt;
  }
  std::string_view rest = line.substr(command.size());
  while (rest.starts_with(' ')) {
    rest.remove_prefix(1);
  }
  if (rest.empty()) {
    return std::nullopt;
  }
  float value = 0.0F;
  const std::from_chars_result read =
      std::from_chars(rest.data(), rest.data() + rest.size(), value);
  return read.ec == std::errc{} ? std::optional<float>(value) : std::nullopt;
}
namespace {

// CP1252 ne diffère de Latin-1 que sur trente-deux octets, ceux de 0x80 à 0x9F,
// que Latin-1 laisse aux commandes et que Windows a remplis de ponctuation.
// Ailleurs, l'octet EST le point de code.
constexpr std::uint8_t kFirstRemapped = 0x80;
constexpr std::uint8_t kLastRemapped = 0x9F;

// Zéro pour les cinq octets que CP1252 ne définit pas.
constexpr std::array<char32_t, 32> kRemapped = {
    0x20AC, 0x0000, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
    0x2039, 0x0152, 0x0000, 0x017D, 0x0000, 0x0000, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
    0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x0000, 0x017E, 0x0178};

// Ce qu'on écrit à la place d'un caractère que CP1252 ne porte pas.
constexpr char kUnmappable = '?';
// Le dernier point de code qu'un octet porte tel quel.
constexpr char32_t kLastLatin1 = 0xFF;

// Les seuils et masques de l'UTF-8, écrits une fois.
constexpr char32_t kOneByteMax = 0x7F;
constexpr char32_t kTwoByteMax = 0x7FF;
constexpr char32_t kThreeByteMax = 0xFFFF;
constexpr std::uint8_t kContinuationMark = 0x80;
constexpr std::uint8_t kContinuationMask = 0xC0;
constexpr char32_t kSixBits = 0x3F;
// Les moitiés de paire UTF-16 n'ont pas de forme UTF-8 légale, et Unicode
// s'arrête à U+10FFFF.
constexpr char32_t kFirstSurrogate = 0xD800;
constexpr char32_t kLastSurrogate = 0xDFFF;
constexpr char32_t kLastCodePoint = 0x10FFFF;

bool IsContinuation(std::string_view utf8, std::size_t at) {
  return (static_cast<unsigned char>(utf8[at]) & kContinuationMask) == kContinuationMark;
}

}  // namespace

char32_t NextCodePoint(std::string_view utf8, std::size_t& at) {
  const auto lead = static_cast<unsigned char>(utf8[at]);
  const std::size_t remaining = utf8.size() - at;
  if (lead <= kOneByteMax) {
    at += 1;
    return lead;
  }
  // ⚠ UNE SÉQUENCE BIEN FORMÉE NE SUFFIT PAS : elle doit aussi être la plus
  // COURTE possible, et ne désigner ni une moitié de paire UTF-16 ni rien
  // au-delà de U+10FFFF. Sans ce contrôle, du 1252 comme « À… » (C0 85) passe
  // pour de l'UTF-8, et `LooksLikeUtf8` le laisse partir sans conversion.
  if ((lead & 0xE0U) == 0xC0U && remaining >= 2 && IsContinuation(utf8, at + 1)) {
    const char32_t code =
        ((lead & 0x1FU) << 6) | (static_cast<unsigned char>(utf8[at + 1]) & kSixBits);
    if (code > kOneByteMax) {
      at += 2;
      return code;
    }
  } else if ((lead & 0xF0U) == 0xE0U && remaining >= 3 && IsContinuation(utf8, at + 1) &&
             IsContinuation(utf8, at + 2)) {
    const char32_t code = ((lead & 0x0FU) << 12) |
                          ((static_cast<unsigned char>(utf8[at + 1]) & kSixBits) << 6) |
                          (static_cast<unsigned char>(utf8[at + 2]) & kSixBits);
    if (code > kTwoByteMax && (code < kFirstSurrogate || code > kLastSurrogate)) {
      at += 3;
      return code;
    }
  } else if ((lead & 0xF8U) == 0xF0U && remaining >= 4 && IsContinuation(utf8, at + 1) &&
             IsContinuation(utf8, at + 2) && IsContinuation(utf8, at + 3)) {
    const char32_t code = ((lead & 0x07U) << 18) |
                          ((static_cast<unsigned char>(utf8[at + 1]) & kSixBits) << 12) |
                          ((static_cast<unsigned char>(utf8[at + 2]) & kSixBits) << 6) |
                          (static_cast<unsigned char>(utf8[at + 3]) & kSixBits);
    if (code > kThreeByteMax && code <= kLastCodePoint) {
      at += 4;
      return code;
    }
  }
  at += 1;
  return kReplacementCharacter;
}

void AppendUtf8(std::string& out, char32_t code) {
  if (code <= kOneByteMax) {
    out.push_back(static_cast<char>(code));
    return;
  }
  if (code <= kTwoByteMax) {
    out.push_back(static_cast<char>(0xC0U | (code >> 6)));
    out.push_back(static_cast<char>(kContinuationMark | (code & kSixBits)));
    return;
  }
  if (code <= kThreeByteMax) {
    out.push_back(static_cast<char>(0xE0U | (code >> 12)));
    out.push_back(static_cast<char>(kContinuationMark | ((code >> 6) & kSixBits)));
    out.push_back(static_cast<char>(kContinuationMark | (code & kSixBits)));
    return;
  }
  out.push_back(static_cast<char>(0xF0U | (code >> 18)));
  out.push_back(static_cast<char>(kContinuationMark | ((code >> 12) & kSixBits)));
  out.push_back(static_cast<char>(kContinuationMark | ((code >> 6) & kSixBits)));
  out.push_back(static_cast<char>(kContinuationMark | (code & kSixBits)));
}

std::string Utf8FromCp1252(std::string_view bytes) {
  std::string out;
  out.reserve(bytes.size());
  for (const char letter : bytes) {
    const auto byte = static_cast<std::uint8_t>(letter);
    if (byte < kFirstRemapped) {
      out.push_back(letter);
      continue;
    }
    if (byte > kLastRemapped) {
      AppendUtf8(out, byte);
      continue;
    }
    const char32_t code = kRemapped[byte - kFirstRemapped];
    AppendUtf8(out, code != 0 ? code : kReplacementCharacter);
  }
  return out;
}

// Le sélecteur de variante « en couleur », et son voisin « en texte ».
constexpr char32_t kFirstVariationSelector = 0xFE00;
constexpr char32_t kLastVariationSelector = 0xFE0F;

// Un caractère UTF-8 fait au plus quatre octets : une coupure, s'il y en a une,
// est dans les TROIS derniers.
constexpr std::size_t kLongestUtf8Sequence = 4;

bool LooksLikeUtf8(std::string_view bytes) {
  if (bytes.empty()) {
    return false;
  }
  bool high = false;
  for (const char letter : bytes) {
    if (static_cast<std::uint8_t>(letter) >= 0x80U) {
      high = true;
      break;
    }
  }
  // ASCII pur : les deux tables coïncident, autant garder le chemin habituel.
  if (!high) {
    return false;
  }
  std::size_t at = 0;
  while (at < bytes.size()) {
    const std::size_t before = at;
    if (NextCodePoint(bytes, at) == kReplacementCharacter && at == before + 1) {
      // 🔴 LE LECTEUR N'AVANCE QUE D'UN OCTET SUR UNE SÉQUENCE INVALIDE, et
      // c'est à cela qu'on la reconnaît. Un vrai U+FFFD, lui, fait trois octets.
      return false;
    }
  }
  return true;
}

std::size_t DanglingUtf8TailLength(std::string_view bytes) {
  for (std::size_t back = 1; back < kLongestUtf8Sequence && back <= bytes.size(); ++back) {
    const auto byte = static_cast<std::uint8_t>(bytes[bytes.size() - back]);
    if (byte < 0x80U) {
      return 0;  // de l'ASCII : rien n'est coupé
    }
    if ((byte & 0xC0U) == 0x80U) {
      continue;  // octet de continuation : on remonte
    }
    // Octet de TÊTE. Combien d'octets annonce-t-il, et en reste-t-il assez ?
    const std::size_t need = byte >= 0xF0U ? 4 : byte >= 0xE0U ? 3 : byte >= 0xC0U ? 2 : 1;
    return need > back ? back : 0;
  }
  return 0;
}

std::string_view Utf8Prefix(std::string_view text, std::size_t most_bytes) {
  if (text.size() <= most_bytes) {
    return text;
  }
  std::size_t end = most_bytes;
  while (end > 0 && IsContinuation(text, end)) {
    --end;
  }
  return text.substr(0, end);
}

// Retire les sélecteurs de variante d'une chaîne déjà en UTF-8.
[[nodiscard]] std::string WithoutVariationSelectors(std::string_view utf8) {
  std::string out;
  out.reserve(utf8.size());
  std::size_t at = 0;
  while (at < utf8.size()) {
    const std::size_t before = at;
    const char32_t code = NextCodePoint(utf8, at);
    if (code >= kFirstVariationSelector && code <= kLastVariationSelector) {
      continue;
    }
    out.append(utf8.substr(before, at - before));
  }
  return out;
}

std::string Utf8FromWire(std::string_view bytes) {
  if (LooksLikeUtf8(bytes)) {
    return WithoutVariationSelectors(bytes);
  }
  // Deuxième chance : de l'UTF-8 que le serveur a coupé au milieu d'un caractère.
  // On jette la queue orpheline — un caractère perdu vaut mieux qu'une phrase
  // entière en mojibake — et on revalide le reste.
  //
  // ⚠ AUCUN RISQUE POUR UN VRAI TEXTE 1252 finissant par un accent : le préfixe
  // qui resterait devrait être de l'UTF-8 VALIDE et contenir du non-ASCII pour
  // passer, ce qu'une phrase latine ordinaire ne fait jamais.
  if (const std::size_t tail = DanglingUtf8TailLength(bytes); tail > 0) {
    const std::string_view head = bytes.substr(0, bytes.size() - tail);
    if (LooksLikeUtf8(head)) {
      return WithoutVariationSelectors(head);
    }
  }
  return Utf8FromCp1252(bytes);
}

namespace {

// L'OCTET CP1252 D'UN POINT DE CODE, ou rien s'il n'en a pas.
[[nodiscard]] std::optional<char> Cp1252ByteOf(char32_t code) {
  if (code < kFirstRemapped || (code > kLastRemapped && code <= kLastLatin1)) {
    return static_cast<char>(code);
  }
  // Les trente-deux déplacés se retrouvent par leur point de code : la table
  // est courte, et la parcourir vaut mieux qu'en tenir une seconde à jour.
  for (std::size_t index = 0; index < kRemapped.size(); ++index) {
    if (kRemapped[index] != 0 && kRemapped[index] == code) {
      return static_cast<char>(kFirstRemapped + index);
    }
  }
  return std::nullopt;
}

}  // namespace

std::string Cp1252FromUtf8(std::string_view utf8) {
  std::string out;
  out.reserve(utf8.size());
  std::size_t at = 0;
  while (at < utf8.size()) {
    out.push_back(Cp1252ByteOf(NextCodePoint(utf8, at)).value_or(kUnmappable));
  }
  return out;
}

std::optional<std::string> ExactCp1252FromUtf8(std::string_view utf8) {
  std::string out;
  out.reserve(utf8.size());
  std::size_t at = 0;
  while (at < utf8.size()) {
    const std::optional<char> byte = Cp1252ByteOf(NextCodePoint(utf8, at));
    if (!byte.has_value()) {
      return std::nullopt;
    }
    out.push_back(*byte);
  }
  return out;
}

std::string WireFromUtf8Text(std::string_view utf8) {
  // ⚠ LE REFUS SE LIT SUR LA CONVERSION, pas sur une inspection préalable : un
  // « ? » tapé par le joueur est parfaitement représentable, et le chercher
  // dans le résultat ferait basculer une phrase qui n'en avait pas besoin.
  if (std::optional<std::string> exact = ExactCp1252FromUtf8(utf8); exact.has_value()) {
    return std::move(*exact);
  }
  // Hors 1252 : l'UTF-8 part tel quel.
  return std::string(utf8);
}

}  // namespace ro::core

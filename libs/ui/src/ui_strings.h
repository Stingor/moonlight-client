#pragma once

// LES PETITES CHAÎNES DE L'INTERFACE, en un seul endroit : la saisie d'un tampon
// et sa copie bornée, des lignes jointes, des libellés à identité ImGui, et les
// tests d'octets qu'une saisie UTF-8 demande.
//
// En-tête PRIVÉ de `libs/ui/src`. 🔴 AUCUNE COPIE LOCALE : une copie gardée
// dans un espace anonyme ou dans un sous-espace de famille masque l'aide
// commune SANS ERREUR dans tout l'espace qui l'entoure. Elle ne devient
// ambiguë — donc visible — que déclarée dans `ro::ui` même, avec la même
// signature, et pour les appels faits hors de son espace anonyme.

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace ro::ui {

// LA SAISIE D'UN TAMPON, jusqu'à son premier zéro — ou jusqu'à sa fin s'il n'en
// porte pas. ⚠ LA FIN SE CHERCHE, ELLE NE SE SUPPOSE PAS : un tampon plein, ou
// rempli à la main, ferait lire un `strlen` au-delà du tableau.
[[nodiscard]] inline std::string_view TypedText(std::span<const char> buffer) {
  const auto end = std::ranges::find(buffer, '\0');
  return {buffer.data(), static_cast<std::size_t>(end - buffer.begin())};
}

// Une copie terminée par un zéro, pour un « %s » qui ne sait pas lire une vue.
[[nodiscard]] inline std::string Owned(std::string_view text) { return std::string(text); }

// COPIE `text` DANS `buffer`, tronqué à la place du tampon moins un octet, puis
// terminé par un zéro. ⚠ Le tampon ne doit pas être vide. La coupe se fait à
// l'octet : un nom qui ne doit pas finir sur un demi-caractère se coupe d'abord
// par `core::WithoutDanglingUtf8`.
inline void CopyBounded(std::span<char> buffer, std::string_view text) {
  const std::size_t kept = std::min(text.size(), buffer.size() - 1);
  std::copy_n(text.begin(), kept, buffer.begin());
  buffer[kept] = '\0';
}

// Les morceaux joints par un saut de ligne, sans ligne vide au bout : une raison
// par ligne d'infobulle.
[[nodiscard]] inline std::string JoinedLines(std::span<const std::string_view> lines) {
  std::string joined;
  for (const std::string_view line : lines) {
    if (!joined.empty()) {
      joined.push_back('\n');
    }
    joined.append(line);
  }
  return joined;
}

// `text` porte `tag` à partir de `at`. ⚠ Un `at` au-delà de la fin lève
// `std::out_of_range`, comme `std::string_view::compare`.
[[nodiscard]] inline bool StartsWithAt(std::string_view text, std::size_t at,
                                       std::string_view tag) {
  return text.compare(at, tag.size(), tag) == 0;
}

// Un chiffre hexadécimal ASCII. ⚠ Pas `std::isxdigit`, qui prend un `int` et
// dépend de la locale.
[[nodiscard]] inline bool IsHexDigit(char letter) {
  return (letter >= '0' && letter <= '9') || (letter >= 'a' && letter <= 'f') ||
         (letter >= 'A' && letter <= 'F');
}

// Un octet de suite d'UTF-8, qui ne commence pas de caractère : `10xxxxxx`.
[[nodiscard]] inline bool IsUtf8Continuation(char byte) {
  constexpr unsigned kContinuationMask = 0xC0U;
  constexpr unsigned kContinuationBits = 0x80U;
  return (static_cast<unsigned char>(byte) & kContinuationMask) == kContinuationBits;
}

// Les points de code d'un texte UTF-8 : chaque octet qui n'en continue pas un
// autre.
[[nodiscard]] inline std::size_t Utf8CodePoints(std::string_view text) {
  return static_cast<std::size_t>(
      std::ranges::count_if(text, [](char byte) { return !IsUtf8Continuation(byte); }));
}

// UN TEXTE VENU D'UN AUTRE JOUEUR, montré sans que ses « ## » deviennent des
// identifiants d'ImGui : ils couperaient le libellé, et « ### » changerait même
// l'identité d'une fenêtre. Une espace sépare chaque « # » du précédent.
[[nodiscard]] inline std::string Unhashed(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char letter : text) {
    if (letter == '#' && !out.empty() && out.back() == '#') {
      out.push_back(' ');
    }
    out.push_back(letter);
  }
  return out;
}

// UN LIBELLÉ D'IMGUI À IDENTITÉ FIXE : le texte montré, puis « ### » et une
// identité qui ne bouge pas quand le texte change de langue.
[[nodiscard]] inline std::string HiddenIdLabel(std::string_view text, std::string_view id) {
  std::string label(text);
  label += "###";
  label += id;
  return label;
}

// Un libellé suivi d'un identifiant qui porte déjà ses « ## » ou « ### » : deux
// boutons de même texte ne partagent pas leur identité.
[[nodiscard]] inline std::string SuffixedLabel(std::string_view text, std::string_view id) {
  std::string label(text);
  label += id;
  return label;
}

}  // namespace ro::ui

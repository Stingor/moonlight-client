#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ro::core {

// ── UNE ADRESSE WEB DANS UNE LIGNE DE TEXTE ─────────────────────────────────
//
// Ce qu'un joueur colle dans le chat : la repérer, la borner, la rendre
// ouvrable, et dire ce qu'elle a de suspect. Tout est pur et sans réseau.
//
// ⚠ LES OCTETS ≥ 0x80 FONT PARTIE DE L'ADRESSE : une adresse accentuée est une
// adresse, et la couper au premier « é » enverrait le navigateur ailleurs.

// Une adresse ne commence qu'en début de mot : début du texte, ou octet
// précédent parmi ' ' '\t' '\n' '\r' '('. Amorces sans casse : http:// https:// www.
[[nodiscard]] bool IsWebAddressStart(std::string_view text, std::size_t at);

// Fin de l'adresse : arrêt sur un octet <= ' ', '<' ou '"'. La ponctuation
// finale . , ; : ! ? est rendue au texte, et ')' aussi quand l'adresse n'a pas
// de '(' ouvrante.
[[nodiscard]] std::size_t WebAddressEnd(std::string_view text, std::size_t at);

// "www." devient "https://www." ; un schéma http(s) passe en minuscules ; le
// reste est rendu tel quel.
[[nodiscard]] std::string OpenableWebAddress(std::string_view written);

// Les morceaux d'une adresse ouvrable. Les vues regardent l'adresse reçue :
// elles ne lui survivent pas.
struct WebAddressParts {
  std::string_view scheme;    // "http" | "https"
  std::string_view userinfo;  // ce qui précède '@' dans l'autorité
  std::string host;           // en minuscules ASCII
  std::string_view port;
  std::string_view path;
  bool valid = false;
};
[[nodiscard]] WebAddressParts ParseWebAddress(std::string_view openable);

// CE QU'UNE ADRESSE A DE SUSPECT, en drapeaux : la confirmation en dit une
// ligne par drapeau levé.
enum WebAddressWarning : std::uint8_t {
  kWebWarnPlainHttp = 1U << 0,
  kWebWarnUserInfo = 1U << 1,
  kWebWarnPunycode = 1U << 2,
  kWebWarnNonAsciiHost = 1U << 3,
};
[[nodiscard]] std::uint8_t WebAddressWarnings(std::string_view openable);

// Une règle qui commence par '.' vaut pour le suffixe ; sinon il faut l'égalité.
// Un hôte qui contient '@' ou qui est vide est toujours refusé.
[[nodiscard]] bool HostMatchesRule(std::string_view host, std::string_view rule);

// Une règle qu'on accepte dans une liste : un nom d'au moins deux étiquettes
// non vides — « .com » admettrait tout un domaine —, avec son point initial
// s'il vaut pour les sous-domaines.
[[nodiscard]] bool IsUsableHostRule(std::string_view rule);

// "a.com;b.net" : nettoyée, en minuscules, sans doublon, réduite aux règles
// utilisables.
[[nodiscard]] std::vector<std::string> SplitHostList(std::string_view listed);
[[nodiscard]] std::string JoinHostList(std::span<const std::string> hosts);

}  // namespace ro::core

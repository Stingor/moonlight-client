#include "ro/core/web_address.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <initializer_list>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace ro::core {
namespace {

// Les trois amorces d'une adresse. Rien d'autre : tout ce qui ressemble de loin
// à un domaine (« truc.fr ») changerait en lien la moitié des phrases, fins de
// phrase comprises (« ...voilà.Et »).
constexpr std::string_view kPlainPrefix = "http://";
constexpr std::string_view kSecurePrefix = "https://";
constexpr std::string_view kBarePrefix = "www.";
constexpr std::string_view kSchemeSeparator = "://";
constexpr std::string_view kPunycodeLabel = "xn--";

// Les noms de schéma rendus par `ParseWebAddress` : des littéraux, qui
// survivent à l'adresse qu'on a lue.
constexpr std::string_view kPlainScheme = "http";
constexpr std::string_view kSecureScheme = "https";

constexpr unsigned char kFirstNonAscii = 0x80;
constexpr char kHostListSeparator = ';';

[[nodiscard]] char LowerAscii(char letter) {
  return (letter >= 'A' && letter <= 'Z') ? static_cast<char>(letter - 'A' + 'a') : letter;
}

[[nodiscard]] bool SameAsciiNoCase(std::string_view left, std::string_view right) {
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(),
                    [](char one, char other) { return LowerAscii(one) == LowerAscii(other); });
}

[[nodiscard]] bool StartsNoCase(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() && SameAsciiNoCase(text.substr(0, prefix.size()), prefix);
}

[[nodiscard]] bool EndsNoCase(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() &&
         SameAsciiNoCase(text.substr(text.size() - suffix.size()), suffix);
}

[[nodiscard]] std::string LoweredAscii(std::string_view text) {
  std::string lowered(text);
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), LowerAscii);
  return lowered;
}

[[nodiscard]] bool NonAscii(char letter) {
  return static_cast<unsigned char>(letter) >= kFirstNonAscii;
}

[[nodiscard]] bool AsciiBlank(char letter) {
  return letter == ' ' || letter == '\t' || letter == '\n' || letter == '\r';
}

// L'octet qui précède une adresse : elle n'en commence une qu'en début de MOT.
// Sans cela, « voirhttps://… » ouvrirait un lien au milieu d'un mot.
[[nodiscard]] bool OpensWord(char before) { return AsciiBlank(before) || before == '('; }

// La ponctuation qui clôt une phrase plutôt qu'une adresse.
[[nodiscard]] bool SentencePunctuation(char letter) {
  return letter == '.' || letter == ',' || letter == ';' || letter == ':' || letter == '!' ||
         letter == '?';
}

// UN OCTET PERMIS DANS UN NOM D'HÔTE. Le reste rend l'adresse invalide : un
// hôte qu'on ne sait pas lire est un hôte qu'on ne sait pas comparer à la liste
// blanche, et un « \ » ou un « % » s'y lisent autrement selon le programme
// qui les reçoit.
[[nodiscard]] bool HostLetter(char letter) {
  return (letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9') || letter == '-' ||
         letter == '.' || letter == '_' || NonAscii(letter);
}

// Un hôte IPv6 entre crochets : chiffres hexadécimaux, « : » et « . ».
[[nodiscard]] bool BracketedHostLetter(char letter) {
  return (letter >= 'a' && letter <= 'f') || (letter >= '0' && letter <= '9') || letter == ':' ||
         letter == '.';
}

[[nodiscard]] bool Digit(char letter) { return letter >= '0' && letter <= '9'; }

// L'AUTORITÉ D'UNE ADRESSE : ce qui suit « :// », jusqu'à « / », « ? » ou « # ».
// Vide sans « :// ».
//
// 🔴 ELLE NE S'ARRÊTE PAS À « \ ». Il y reste, et l'adresse qui le porte est
// refusée : « https://cdn.discordapp.com\@evil.tld » se lit Discord pour un
// navigateur, evil.tld pour un autre lecteur, et on ne compare pas à la liste
// blanche un hôte dont on ne sait pas lequel sera contacté.
[[nodiscard]] std::string_view AuthorityOf(std::string_view openable) {
  const std::size_t separator = openable.find(kSchemeSeparator);
  if (separator == std::string_view::npos) {
    return {};
  }
  const std::string_view rest = openable.substr(separator + kSchemeSeparator.size());
  return rest.substr(0, rest.find_first_of("/?#"));
}

// LES ÉTIQUETTES D'UN NOM : aucune vide — ni point en tête, ni « .. », ni
// point final —, aucune qui commence ou finit par un tiret, et des octets
// permis. Rend leur nombre, zéro si le nom est refusé.
[[nodiscard]] std::size_t LabelCount(std::string_view name) {
  std::size_t count = 0;
  while (true) {
    const std::size_t dot = name.find('.');
    const std::string_view label = name.substr(0, dot);
    if (label.empty() || label.front() == '-' || label.back() == '-' ||
        !std::all_of(label.begin(), label.end(), HostLetter)) {
      return 0;
    }
    ++count;
    if (dot == std::string_view::npos) {
      return count;
    }
    name.remove_prefix(dot + 1);
  }
}

// Le plus grand numéro de port.
constexpr unsigned long kMostPort = 65535;
constexpr std::size_t kMostPortDigits = 5;

// Une règle vaut pour au moins deux étiquettes : « .com » admettrait tout un
// domaine de premier niveau.
constexpr std::size_t kLeastRuleLabels = 2;

}  // namespace

bool IsUsableHostRule(std::string_view rule) {
  if (rule.starts_with('.')) {
    rule.remove_prefix(1);
  }
  return LabelCount(rule) >= kLeastRuleLabels;
}

bool IsWebAddressStart(std::string_view text, std::size_t at) {
  if (at >= text.size()) {
    return false;
  }
  if (at > 0 && !OpensWord(text[at - 1])) {
    return false;
  }
  const std::string_view rest = text.substr(at);
  return StartsNoCase(rest, kPlainPrefix) || StartsNoCase(rest, kSecurePrefix) ||
         StartsNoCase(rest, kBarePrefix);
}

std::size_t WebAddressEnd(std::string_view text, std::size_t at) {
  if (at >= text.size()) {
    return text.size();
  }
  std::size_t stop = at;
  while (stop < text.size()) {
    const char letter = text[stop];
    // ⚠ LES OCTETS ≥ 0x80 RESTENT : seuls les octets de contrôle, l'espace, « < »
    // et « " » arrêtent l'adresse.
    if ((!NonAscii(letter) && static_cast<unsigned char>(letter) <= ' ') || letter == '<' ||
        letter == '"') {
      break;
    }
    ++stop;
  }
  // La ponctuation finale revient au texte : « regarde https://x.fr. » n'ouvre
  // pas une adresse terminée par un point. La « ) » ne revient que si l'adresse
  // n'a pas de « ( » : les adresses de wiki en portent.
  while (stop > at) {
    const char last = text[stop - 1];
    if (SentencePunctuation(last)) {
      --stop;
      continue;
    }
    if (last == ')' && text.substr(at, stop - at).find('(') == std::string_view::npos) {
      --stop;
      continue;
    }
    break;
  }
  return stop;
}

std::string OpenableWebAddress(std::string_view written) {
  if (StartsNoCase(written, kBarePrefix)) {
    return std::string(kSecurePrefix) + std::string(written);
  }
  std::string openable(written);
  // ⚠ LE SCHÉMA EN MINUSCULES : « HTTPS:// » et « https:// » sont une même
  // adresse, et les accords comme la liste blanche la comparent à l'octet.
  for (const std::string_view prefix : {kSecurePrefix, kPlainPrefix}) {
    if (StartsNoCase(openable, prefix)) {
      const std::size_t scheme_size = prefix.size() - kSchemeSeparator.size();
      std::transform(openable.begin(), openable.begin() + static_cast<std::ptrdiff_t>(scheme_size),
                     openable.begin(), LowerAscii);
      break;
    }
  }
  return openable;
}

WebAddressParts ParseWebAddress(std::string_view openable) {
  WebAddressParts parts;
  std::size_t authority_begin = 0;
  if (StartsNoCase(openable, kSecurePrefix)) {
    parts.scheme = kSecureScheme;
    authority_begin = kSecurePrefix.size();
  } else if (StartsNoCase(openable, kPlainPrefix)) {
    parts.scheme = kPlainScheme;
    authority_begin = kPlainPrefix.size();
  } else {
    return parts;
  }

  const std::string_view authority = AuthorityOf(openable);
  parts.path = openable.substr(authority_begin + authority.size());
  if (authority.find('\\') != std::string_view::npos) {
    return parts;
  }

  // L'identité éventuelle va jusqu'au DERNIER « @ » : c'est l'hôte qui suit que
  // le réseau contactera.
  std::string_view host_and_port = authority;
  if (const std::size_t at_sign = authority.rfind('@'); at_sign != std::string_view::npos) {
    parts.userinfo = authority.substr(0, at_sign);
    host_and_port = authority.substr(at_sign + 1);
  }

  std::string_view host;
  std::string_view after_host;
  if (host_and_port.starts_with('[')) {
    const std::size_t closing = host_and_port.find(']');
    if (closing == std::string_view::npos) {
      return parts;
    }
    host = host_and_port.substr(0, closing + 1);
    after_host = host_and_port.substr(closing + 1);
  } else {
    const std::size_t colon = host_and_port.find(':');
    host = host_and_port.substr(0, colon);
    after_host = colon == std::string_view::npos ? std::string_view() : host_and_port.substr(colon);
  }
  if (!after_host.empty()) {
    if (after_host.front() != ':') {
      return parts;
    }
    parts.port = after_host.substr(1);
    unsigned long port = 0;
    if (parts.port.size() > kMostPortDigits ||
        !std::all_of(parts.port.begin(), parts.port.end(), Digit) ||
        (!parts.port.empty() &&
         (std::from_chars(parts.port.data(), parts.port.data() + parts.port.size(), port).ec !=
              std::errc() ||
          port > kMostPort))) {
      return parts;
    }
  }

  parts.host = LoweredAscii(host);
  if (parts.host.empty()) {
    return parts;
  }
  const bool bracketed = parts.host.front() == '[';
  const std::string_view inside =
      bracketed ? std::string_view(parts.host).substr(1, parts.host.size() - 2)
                : std::string_view(parts.host);
  const bool readable =
      bracketed ? !inside.empty() && std::all_of(inside.begin(), inside.end(), BracketedHostLetter)
                : LabelCount(inside) > 0;
  parts.valid = readable;
  return parts;
}

std::uint8_t WebAddressWarnings(std::string_view openable) {
  std::uint8_t warnings = 0;
  if (StartsNoCase(openable, kPlainPrefix)) {
    warnings |= kWebWarnPlainHttp;
  }
  // « https://@evil.tld » n'a pas d'identité, mais son « @ » trompe l'œil
  // autant. Il se dit même quand l'hôte est illisible.
  if (AuthorityOf(openable).find('@') != std::string_view::npos) {
    warnings |= kWebWarnUserInfo;
  }
  const WebAddressParts parts = ParseWebAddress(openable);
  if (!parts.valid) {
    return warnings;
  }
  // Un nom en punycode, à n'importe quel étage : « xn--pple-43d.com » s'affiche
  // « аpple.com » avec un а cyrillique dans un navigateur.
  std::string_view labels = parts.host;
  while (!labels.empty()) {
    const std::size_t dot = labels.find('.');
    if (labels.substr(0, dot).starts_with(kPunycodeLabel)) {
      warnings |= kWebWarnPunycode;
      break;
    }
    labels = dot == std::string_view::npos ? std::string_view() : labels.substr(dot + 1);
  }
  if (std::any_of(parts.host.begin(), parts.host.end(), NonAscii)) {
    warnings |= kWebWarnNonAsciiHost;
  }
  return warnings;
}

bool HostMatchesRule(std::string_view host, std::string_view rule) {
  if (host.empty() || rule.empty() || host.find('@') != std::string_view::npos) {
    return false;
  }
  if (rule.front() == '.') {
    // « .discordapp.com » vaut pour cdn.discordapp.com, pas pour
    // discordapp.com lui-même : la règle sans point le dit à part.
    return host.size() > rule.size() && EndsNoCase(host, rule);
  }
  return SameAsciiNoCase(host, rule);
}

std::vector<std::string> SplitHostList(std::string_view listed) {
  std::vector<std::string> hosts;
  while (true) {
    const std::size_t separator = listed.find(kHostListSeparator);
    std::string_view piece = listed.substr(0, separator);
    while (!piece.empty() && AsciiBlank(piece.front())) {
      piece.remove_prefix(1);
    }
    while (!piece.empty() && AsciiBlank(piece.back())) {
      piece.remove_suffix(1);
    }
    std::string host = LoweredAscii(piece);
    if (IsUsableHostRule(host) && std::find(hosts.begin(), hosts.end(), host) == hosts.end()) {
      hosts.push_back(std::move(host));
    }
    if (separator == std::string_view::npos) {
      break;
    }
    listed.remove_prefix(separator + 1);
  }
  return hosts;
}

std::string JoinHostList(std::span<const std::string> hosts) {
  std::string joined;
  for (const std::string& host : hosts) {
    if (!joined.empty()) {
      joined.push_back(kHostListSeparator);
    }
    joined += host;
  }
  return joined;
}

}  // namespace ro::core

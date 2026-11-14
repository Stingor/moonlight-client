// UNE ADRESSE WEB DANS UNE LIGNE DE CHAT : où elle commence, où elle finit, ce
// qu'on ouvre, et ce qu'elle a de suspect.

#include "ro/core/web_address.h"

#include <array>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using ro::core::HostMatchesRule;
using ro::core::IsWebAddressStart;
using ro::core::JoinHostList;
using ro::core::OpenableWebAddress;
using ro::core::ParseWebAddress;
using ro::core::SplitHostList;
using ro::core::WebAddressEnd;
using ro::core::WebAddressWarnings;

namespace {

// L'adresse que le fil découperait à partir de `at`.
[[nodiscard]] std::string_view AddressAt(std::string_view text, std::size_t at) {
  return text.substr(at, WebAddressEnd(text, at) - at);
}

}  // namespace

TEST_CASE("Adresse web : elle ne commence qu'en début de mot", "[core][web]") {
  constexpr std::string_view kGlued = "voirhttps://x.fr";
  CHECK_FALSE(IsWebAddressStart(kGlued, kGlued.find('h')));

  constexpr std::string_view kSpaced = "voir https://x.fr";
  CHECK(IsWebAddressStart(kSpaced, kSpaced.find('h')));
  CHECK(IsWebAddressStart("https://x.fr", 0));
  CHECK(IsWebAddressStart("a\thttp://x.fr", 2));
  CHECK(IsWebAddressStart("a\nwww.x.fr", 2));

  // Entre parenthèses : l'adresse sort sans la parenthèse fermante.
  constexpr std::string_view kWrapped = "(https://x.fr)";
  REQUIRE(IsWebAddressStart(kWrapped, 1));
  CHECK(AddressAt(kWrapped, 1) == "https://x.fr");
}

TEST_CASE("Adresse web : les amorces se lisent sans casse, et rien d'autre", "[core][web]") {
  CHECK(IsWebAddressStart("WWW.Moonlight-Destiny.fr", 0));
  CHECK(IsWebAddressStart("HTTPS://X.FR", 0));
  CHECK(IsWebAddressStart("Http://x.fr", 0));
  // Un domaine nu n'est pas une adresse : la moitié des phrases en seraient.
  CHECK_FALSE(IsWebAddressStart("moonlight-destiny.fr", 0));
  CHECK_FALSE(IsWebAddressStart("ftp://x.fr", 0));
  CHECK_FALSE(IsWebAddressStart("https:/x.fr", 0));
  CHECK_FALSE(IsWebAddressStart("http", 0));
  CHECK_FALSE(IsWebAddressStart("", 0));
  CHECK_FALSE(IsWebAddressStart("x", 3));
}

TEST_CASE("Adresse web : la ponctuation finale revient au texte", "[core][web]") {
  CHECK(AddressAt("https://moonlight-destiny.fr.", 0) == "https://moonlight-destiny.fr");
  CHECK(AddressAt("https://x.fr/?a=1!?", 0) == "https://x.fr/?a=1");
  CHECK(AddressAt("https://x.fr, et", 0) == "https://x.fr");
  CHECK(AddressAt("https://x.fr;:", 0) == "https://x.fr");
  // Une adresse de wiki garde sa « ) » : elle a sa « ( ».
  constexpr std::string_view kWiki = "https://fr.wikipedia.org/wiki/Ragnarok_(jeu)";
  CHECK(AddressAt(kWiki, 0) == kWiki);
  CHECK(AddressAt("https://x.fr/a_(b)).", 0) == "https://x.fr/a_(b))");
}

TEST_CASE("Adresse web : arrêt sur blanc, contrôle, « < » et « \" », pas sur l'UTF-8",
          "[core][web]") {
  CHECK(AddressAt("https://x.fr/a b", 0) == "https://x.fr/a");
  CHECK(AddressAt("https://x.fr/a<b>", 0) == "https://x.fr/a");
  CHECK(AddressAt("https://x.fr/a\"b", 0) == "https://x.fr/a");
  CHECK(AddressAt(std::string_view("https://x.fr/a\x01z", 15), 0) == "https://x.fr/a");
  CHECK(AddressAt("https://x.fr/a\tb", 0) == "https://x.fr/a");
  // « été » en UTF-8 : les octets ≥ 0x80 sont de l'adresse.
  constexpr std::string_view kAccented = "https://x.fr/\xC3\xA9t\xC3\xA9 suite";
  CHECK(AddressAt(kAccented, 0) == "https://x.fr/\xC3\xA9t\xC3\xA9");
}

TEST_CASE("Adresse web : « www. » s'ouvre en HTTPS, le reste tel quel", "[core][web]") {
  CHECK(OpenableWebAddress("www.a.fr") == "https://www.a.fr");
  CHECK(OpenableWebAddress("WWW.a.fr") == "https://WWW.a.fr");
  CHECK(OpenableWebAddress("http://a.fr") == "http://a.fr");
  CHECK(OpenableWebAddress("https://a.fr/x") == "https://a.fr/x");
  // Le schéma seul passe en minuscules : l'hôte et le chemin restent écrits.
  CHECK(OpenableWebAddress("HTTPS://A.fr/X") == "https://A.fr/X");
  CHECK(OpenableWebAddress("hTtP://a.fr/Y") == "http://a.fr/Y");
  CHECK(OpenableWebAddress("ftp://a.fr") == "ftp://a.fr");
}

TEST_CASE("Adresse web : les morceaux d'une adresse", "[core][web]") {
  const auto parts = ParseWebAddress("HTTPS://user:pw@CDN.Discordapp.com:8443/a/b.png?x=1#y");
  REQUIRE(parts.valid);
  CHECK(parts.scheme == "https");
  CHECK(parts.userinfo == "user:pw");
  CHECK(parts.host == "cdn.discordapp.com");
  CHECK(parts.port == "8443");
  CHECK(parts.path == "/a/b.png?x=1#y");

  const auto bare = ParseWebAddress("http://x.fr");
  REQUIRE(bare.valid);
  CHECK(bare.scheme == "http");
  CHECK(bare.path.empty());
  CHECK(bare.port.empty());

  const auto query_only = ParseWebAddress("https://x.fr?q=1");
  REQUIRE(query_only.valid);
  CHECK(query_only.host == "x.fr");
  CHECK(query_only.path == "?q=1");

  const auto bracketed = ParseWebAddress("https://[::1]:80/");
  REQUIRE(bracketed.valid);
  CHECK(bracketed.host == "[::1]");
  CHECK(bracketed.port == "80");

  CHECK_FALSE(ParseWebAddress("www.x.fr").valid);
  CHECK_FALSE(ParseWebAddress("ftp://x.fr").valid);
  CHECK_FALSE(ParseWebAddress("https:///chemin").valid);
  CHECK_FALSE(ParseWebAddress("https://x.fr:8a/").valid);
  // 🔴 Un « \ » dans l'autorité : l'hôte se lit autrement selon le lecteur.
  CHECK_FALSE(ParseWebAddress("https://cdn.discordapp.com\\@evil.tld/").valid);
  CHECK_FALSE(ParseWebAddress("https://%65vil.tld/").valid);
  // Des étiquettes vides, un tiret en bord d'étiquette, un point final.
  CHECK_FALSE(ParseWebAddress("https://./x").valid);
  CHECK_FALSE(ParseWebAddress("https://.com/x").valid);
  CHECK_FALSE(ParseWebAddress("https://..discordapp.com/x").valid);
  CHECK_FALSE(ParseWebAddress("https://a..discordapp.com/x").valid);
  CHECK_FALSE(ParseWebAddress("https://cdn.discordapp.com./x").valid);
  CHECK_FALSE(ParseWebAddress("https://-.discordapp.com/x").valid);
  CHECK_FALSE(ParseWebAddress("https://cdn-.discordapp.com/x").valid);
  CHECK(ParseWebAddress("https://cdn-1.discordapp.com/x").valid);
  // Un port au-delà de 65535.
  CHECK_FALSE(ParseWebAddress("https://x.fr:65536/").valid);
  CHECK_FALSE(ParseWebAddress("https://x.fr:99999999999/").valid);
  CHECK(ParseWebAddress("https://x.fr:65535/").valid);
  CHECK(ParseWebAddress("https://x.fr:/").valid);
}

TEST_CASE("Adresse web : ce qu'une adresse a de suspect", "[core][web]") {
  CHECK(WebAddressWarnings("https://moonlight-destiny.fr/x") == 0);
  CHECK(WebAddressWarnings("http://moonlight-destiny.fr") == ro::core::kWebWarnPlainHttp);
  CHECK(WebAddressWarnings("https://cdn.discordapp.com@evil.tld/") == ro::core::kWebWarnUserInfo);
  CHECK(WebAddressWarnings("https://@evil.tld/") == ro::core::kWebWarnUserInfo);
  // Illisible, mais son « @ » se dit quand même.
  CHECK(WebAddressWarnings("https://cdn.discordapp.com\\@evil.tld/") == ro::core::kWebWarnUserInfo);
  // Un « @ » dans le CHEMIN n'est pas une identité.
  CHECK(WebAddressWarnings("https://medium.com/@auteur") == 0);
  CHECK(WebAddressWarnings("https://xn--pple-43d.com/") == ro::core::kWebWarnPunycode);
  CHECK(WebAddressWarnings("https://www.XN--pple-43d.com/") == ro::core::kWebWarnPunycode);
  CHECK(WebAddressWarnings("https://\xD0\xB0pple.com/") == ro::core::kWebWarnNonAsciiHost);
  // Les drapeaux se cumulent.
  CHECK(WebAddressWarnings("http://a@xn--x.fr/") ==
        (ro::core::kWebWarnPlainHttp | ro::core::kWebWarnUserInfo | ro::core::kWebWarnPunycode));
}

TEST_CASE("Adresse web : une règle d'hôte, suffixe ou égalité", "[core][web]") {
  CHECK(HostMatchesRule("cdn.discordapp.com", ".discordapp.com"));
  CHECK(HostMatchesRule("CDN.discordapp.com", ".discordapp.com"));
  CHECK_FALSE(HostMatchesRule("discordapp.com", ".discordapp.com"));
  CHECK_FALSE(HostMatchesRule("discordapp.com.evil.io", ".discordapp.com"));
  CHECK_FALSE(HostMatchesRule("evildiscordapp.com", ".discordapp.com"));
  CHECK(HostMatchesRule("imgur.com", "imgur.com"));
  CHECK_FALSE(HostMatchesRule("i.imgur.com", "imgur.com"));
  // 🔴 Un « @ » dans l'hôte, ou rien : toujours refusé.
  CHECK_FALSE(HostMatchesRule("x@cdn.discordapp.com", ".discordapp.com"));
  CHECK_FALSE(HostMatchesRule("a@imgur.com", "a@imgur.com"));
  CHECK_FALSE(HostMatchesRule("", ".discordapp.com"));
  CHECK_FALSE(HostMatchesRule("imgur.com", ""));
}

TEST_CASE("Adresse web : la liste d'hôtes du joueur, nettoyée", "[core][web]") {
  // « .com », « . », « ..x.fr » et « localhost » ne sont pas des règles : elles
  // admettraient un domaine entier, ou rien de lisible.
  const auto hosts =
      SplitHostList(" A.com ;b.net;;a.com; ;x@y.fr;c.org/chemin;.D.fr;.com;.;..x.fr;localhost");
  REQUIRE(hosts.size() == 3);
  CHECK(hosts[0] == "a.com");
  CHECK(hosts[1] == "b.net");
  CHECK(hosts[2] == ".d.fr");
  CHECK(JoinHostList(hosts) == "a.com;b.net;.d.fr");
  CHECK(SplitHostList("").empty());
  CHECK(JoinHostList({}).empty());
}

TEST_CASE("Adresse web : une règle vaut pour deux étiquettes au moins", "[core][web]") {
  using ro::core::IsUsableHostRule;
  CHECK(IsUsableHostRule("imgur.com"));
  CHECK(IsUsableHostRule(".discordapp.com"));
  CHECK_FALSE(IsUsableHostRule(".com"));
  CHECK_FALSE(IsUsableHostRule("com"));
  CHECK_FALSE(IsUsableHostRule("."));
  CHECK_FALSE(IsUsableHostRule(""));
  CHECK_FALSE(IsUsableHostRule("..discordapp.com"));
  CHECK_FALSE(IsUsableHostRule("discordapp.com."));
  CHECK_FALSE(IsUsableHostRule("-x.com"));
  CHECK_FALSE(IsUsableHostRule("x@y.com"));
}

// L'ADRESSE QUE WINHTTP CONTACTERA, recoupée avec celle que la liste blanche a
// jugée. Aucun réseau : `WinHttpCrackUrl` découpe une chaîne, rien de plus.

#include "web/win_http_get.h"

#include <catch2/catch_test_macros.hpp>

using moonclient::CrackJudgedUrl;

TEST_CASE("WinHTTP : une adresse ordinaire se découpe sans son fragment", "[app][web]") {
  const auto target = CrackJudgedUrl("https://CDN.discordapp.com/a/b.png?x=1#frag");
  REQUIRE(target.has_value());
  CHECK(target->host == L"CDN.discordapp.com");
  CHECK(target->port == 443);
  CHECK(target->path_and_query == L"/a/b.png?x=1");
}

TEST_CASE("WinHTTP : deux lecteurs qui divergent, et rien ne part", "[app][web]") {
  // WinHTTP lit evil.tld ; la liste lit une adresse illisible.
  CHECK_FALSE(CrackJudgedUrl("https://cdn.discordapp.com\\@evil.tld/x").has_value());
  // WinHTTP coupe au premier « @ », la liste au dernier.
  CHECK_FALSE(CrackJudgedUrl("https://a@b@cdn.discordapp.com/x").has_value());
  // Un « @ » après « # » est dans le fragment, pour les deux : l'hôte est Discord.
  const auto fragment = CrackJudgedUrl("https://cdn.discordapp.com#@evil.tld/x");
  REQUIRE(fragment.has_value());
  CHECK(fragment->host == L"cdn.discordapp.com");
  CHECK(fragment->path_and_query == L"/");
}

TEST_CASE("WinHTTP : HTTPS seulement, sans casse", "[app][web]") {
  CHECK(CrackJudgedUrl("HTTPS://cdn.discordapp.com/x").has_value());
  CHECK_FALSE(CrackJudgedUrl("http://cdn.discordapp.com/x").has_value());
  CHECK_FALSE(CrackJudgedUrl("https://cdn.discordapp.com:65536/x").has_value());
}

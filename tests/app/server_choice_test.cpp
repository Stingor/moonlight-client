// LE CHOIX DU SERVEUR : la liste du sélecteur, la marque TEST et la
// présélection, sans réseau — la sonde n'y entre que par son verdict.

#include "game/server_choice.h"

#include <vector>

#include <catch2/catch_test_macros.hpp>

using moonclient::ComposeServerChoices;
using moonclient::OfferServers;
using moonclient::PreferredServer;
using moonclient::ServerAddress;
using moonclient::ServerChoice;
using moonclient::ServerEntry;

namespace {

const std::vector<ServerEntry> kProductionOnly = {
    {.name = "Moonlight-Destiny", .address = "moonlight-destiny.fr", .port = 16900}};

const ServerAddress kLocalTest{.host = "127.0.0.1", .port = 6900};

// Le site de la production, celui que vise un serveur qui n'en déclare pas.
constexpr const char* kSite = "https://site.example";

}  // namespace

TEST_CASE("Serveurs : un serveur TEST sans site déclaré n'offre pas la porte du site",
          "[app][servers]") {
  const std::vector<ServerEntry> listed = {
      kProductionOnly.front(),
      {.name = "Local", .address = "127.0.0.1", .port = 6900},
      {.name = "Local web",
       .address = "localhost",
       .port = 6901,
       .web_login = "https://127.0.0.1:8443"},
      {.name = "Miroir", .address = "miroir.fr", .port = 16900, .web_login = "https://miroir.fr"}};
  const std::vector<ServerChoice> choices = ComposeServerChoices(listed, kLocalTest, true, kSite);
  REQUIRE(choices.size() == 4);
  // La production garde le site par défaut.
  CHECK(choices[0].web_origin == kSite);
  // Un serveur TEST sans clé : pas de porte, le formulaire classique seul.
  CHECK(choices[1].web_origin.empty());
  // Un serveur TEST qui déclare son site la retrouve, sur ce site-là.
  CHECK(choices[2].web_origin == "https://127.0.0.1:8443");
  // Un serveur ordinaire qui déclare le sien l'emploie.
  CHECK(choices[3].web_origin == "https://miroir.fr");

  // Le serveur test ajouté par la sonde n'a pas de fiche, donc pas de site.
  const std::vector<ServerChoice> probed =
      ComposeServerChoices(kProductionOnly, kLocalTest, true, kSite);
  REQUIRE(probed.size() == 2);
  CHECK(probed[1].web_origin.empty());
}

TEST_CASE("Serveurs : le serveur test détecté s'ajoute en dernier, marqué", "[app][servers]") {
  const std::vector<ServerChoice> choices =
      ComposeServerChoices(kProductionOnly, kLocalTest, true, kSite);
  REQUIRE(choices.size() == 2);
  CHECK(choices[0].name == "Moonlight-Destiny");
  CHECK_FALSE(choices[0].test);
  CHECK_FALSE(choices[0].probed);
  CHECK(choices[1].test);
  CHECK(choices[1].probed);
  CHECK(choices[1].address.host == "127.0.0.1");
  CHECK(choices[1].address.port == 6900);
  CHECK(moonclient::OffersServerChoice(choices));
}

TEST_CASE("Serveurs : sans serveur test détecté, un serveur seul n'offre pas de choix",
          "[app][servers]") {
  const std::vector<ServerChoice> choices =
      ComposeServerChoices(kProductionOnly, kLocalTest, false, kSite);
  REQUIRE(choices.size() == 1);
  CHECK_FALSE(moonclient::OffersServerChoice(choices));
}

TEST_CASE("Serveurs : un serveur test déjà listé ne s'ajoute pas une seconde fois",
          "[app][servers]") {
  // ⚠ « localhost » ET « 127.0.0.1 » SONT LE MÊME SERVEUR : au même port, l'un
  // couvre l'autre.
  const std::vector<ServerEntry> listed = {kProductionOnly.front(),
                                           {.name = "Local", .address = "localhost", .port = 6900}};
  const std::vector<ServerChoice> choices = ComposeServerChoices(listed, kLocalTest, true, kSite);
  REQUIRE(choices.size() == 2);
  CHECK(choices[1].name == "Local");
  CHECK(choices[1].test);
  CHECK_FALSE(choices[1].probed);
}

TEST_CASE("Serveurs : une adresse locale ou celle du serveur test porte la marque TEST",
          "[app][servers]") {
  const ServerAddress remote_test{.host = "essai.moonlight.fr", .port = 7000};
  const std::vector<ServerEntry> listed = {
      kProductionOnly.front(),
      {.name = "Boucle", .address = "::1", .port = 5000},
      {.name = "Essai", .address = "ESSAI.moonlight.fr", .port = 7000},
      // Même hôte que le serveur test, autre port : un autre serveur.
      {.name = "Voisin", .address = "essai.moonlight.fr", .port = 7001}};
  const std::vector<ServerChoice> choices = ComposeServerChoices(listed, remote_test, true, kSite);
  REQUIRE(choices.size() == 4);
  CHECK_FALSE(choices[0].test);
  CHECK(choices[1].test);
  CHECK(choices[2].test);
  CHECK_FALSE(choices[3].test);
}

TEST_CASE("Serveurs : le serveur retenu est présélectionné, le premier à défaut",
          "[app][servers]") {
  const std::vector<ServerEntry> listed = {kProductionOnly.front(),
                                           {.name = "Autre", .address = "autre.fr", .port = 16900}};
  CHECK(OfferServers(listed, kLocalTest, false, "", kSite).chosen == 0);
  CHECK(OfferServers(listed, kLocalTest, false, "autre.fr:16900", kSite).chosen == 1);
  // Un serveur qui n'est plus dans le fichier, ou une clé illisible : le premier.
  CHECK(OfferServers(listed, kLocalTest, false, "parti.fr:16900", kSite).chosen == 0);
  CHECK(OfferServers(listed, kLocalTest, false, "n'importe quoi", kSite).chosen == 0);
  // Le serveur test retenu, toujours là.
  const auto with_test = OfferServers(listed, kLocalTest, true, "127.0.0.1:6900", kSite);
  CHECK(with_test.chosen == 2);
  CHECK(with_test.current().test);
}

TEST_CASE("Serveurs : le serveur test retenu qui ne répond plus ramène à la production",
          "[app][servers]") {
  CHECK(OfferServers(kProductionOnly, kLocalTest, false, "127.0.0.1:6900", kSite).chosen == 0);
  // 🔴 MÊME LISTÉ DANS LE FICHIER : on ne vise pas un serveur qu'on sait éteint.
  const std::vector<ServerEntry> listed = {kProductionOnly.front(),
                                           {.name = "Local", .address = "127.0.0.1", .port = 6900}};
  const auto offer = OfferServers(listed, kLocalTest, false, "localhost:6900", kSite);
  CHECK(offer.chosen == 0);
  CHECK_FALSE(offer.current().test);
}

TEST_CASE("Serveurs : la clé retenue est hôte:port, et se relit", "[app][servers]") {
  const ServerChoice probed = ComposeServerChoices(kProductionOnly, kLocalTest, true, kSite).back();
  CHECK(moonclient::ServerMemoryKey(probed.address) == "127.0.0.1:6900");
  CHECK(PreferredServer(ComposeServerChoices(kProductionOnly, kLocalTest, true, kSite),
                        moonclient::ServerMemoryKey(probed.address), kLocalTest, true) == 1);
}

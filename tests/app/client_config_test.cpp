// `moonclient.yml` : à qui ce client parle, et ce qu'il refuse de deviner.

#include "game/client_config.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

using moonclient::ParseClientConfig;

namespace {

constexpr const char* kShipped = R"(
servers:
  - name: Moonlight-Destiny
    address: moonlight-destiny.fr
    port: 16900
  - name: Local
    address: 127.0.0.1
    port: 6900
web: moonlight-destiny.fr:8888
archives:
  - moonlight.grf
  - data.grf
)";

// Le plus petit fichier accepté, à compléter par chaque essai.
constexpr const char* kMinimal = "servers:\n  - {address: h, port: 1}\narchives: [data.grf]\n";

}  // namespace

TEST_CASE("Configuration : les serveurs dans l'ordre du fichier, et le service web",
          "[app][config]") {
  const auto config = ParseClientConfig(kShipped);
  REQUIRE(config.has_value());
  REQUIRE(config->servers.size() == 2);
  CHECK(config->servers[0].name == "Moonlight-Destiny");
  CHECK(config->servers[0].address == "moonlight-destiny.fr");
  CHECK(config->servers[0].port == 16900);
  CHECK(config->servers[1].address == "127.0.0.1");
  CHECK(config->servers[1].port == 6900);
  CHECK(config->web == "moonlight-destiny.fr:8888");
}

TEST_CASE("Configuration : sans service web, l'adresse est vide", "[app][config]") {
  const auto config = ParseClientConfig(kMinimal);
  REQUIRE(config.has_value());
  CHECK(config->web.empty());
  // Sans nom, un serveur se nomme par son adresse : le journal dit lequel.
  CHECK(config->servers[0].name == "h");
}

TEST_CASE("Configuration : le fuseau du serveur, Paris faute d'en dire un", "[app][config]") {
  const auto absent = ParseClientConfig(kMinimal);
  REQUIRE(absent.has_value());
  CHECK(absent->server_time_zone == moonclient::kDefaultServerTimeZone);

  const auto declared =
      ParseClientConfig(std::string(kMinimal) + "server_time_zone: America/Montreal\n");
  REQUIRE(declared.has_value());
  CHECK(declared->server_time_zone == "America/Montreal");
}

TEST_CASE("Configuration : ce qui ne mène à aucun serveur est refusé, et dit pourquoi",
          "[app][config]") {
  const auto empty = ParseClientConfig("");
  REQUIRE_FALSE(empty.has_value());
  CHECK(empty.error().detail.find("aucun serveur") != std::string::npos);

  CHECK_FALSE(ParseClientConfig("servers: []\n").has_value());
  CHECK_FALSE(ParseClientConfig("servers:\n  - {port: 6900}\n").has_value());

  // L'erreur NOMME le serveur fautif : un fichier de dix serveurs se corrige
  // sans deviner lequel.
  const auto bad_port =
      ParseClientConfig("servers:\n  - {address: a, port: 1}\n  - {address: b, port: 70000}\n");
  REQUIRE_FALSE(bad_port.has_value());
  CHECK(bad_port.error().detail.find("n° 2") != std::string::npos);
  CHECK_FALSE(ParseClientConfig("servers:\n  - {address: a, port: zéro}\n").has_value());
  CHECK_FALSE(ParseClientConfig("servers:\n  - {address: a, port: 0}\n").has_value());

  // Du YAML cassé ne fait pas planter : il est refusé comme le reste.
  CHECK_FALSE(ParseClientConfig("servers: [ {address: a\n").has_value());
}

TEST_CASE("Configuration : les archives dans l'ordre de priorité du fichier", "[app][config]") {
  const auto config = ParseClientConfig(kShipped);
  REQUIRE(config.has_value());
  REQUIRE(config->archives.size() == 2);
  CHECK(config->archives[0] == "moonlight.grf");
  CHECK(config->archives[1] == "data.grf");
  // Un sous-dossier de l'installation est permis.
  CHECK(ParseClientConfig("servers:\n  - {address: h, port: 1}\narchives: [grf/extra.grf]\n")
            .has_value());
}

TEST_CASE("Configuration : sans archive, ou hors de l'installation, le fichier est refusé",
          "[app][config]") {
  const std::string servers = "servers:\n  - {address: h, port: 1}\n";
  const auto missing = ParseClientConfig(servers);
  REQUIRE_FALSE(missing.has_value());
  CHECK(missing.error().detail.find("archives") != std::string::npos);
  CHECK_FALSE(ParseClientConfig(servers + "archives: []\n").has_value());
  CHECK_FALSE(ParseClientConfig(servers + "archives: data.grf\n").has_value());

  const auto outside = ParseClientConfig(servers + "archives: [data.grf, ../autre/data.grf]\n");
  REQUIRE_FALSE(outside.has_value());
  CHECK(outside.error().detail.find("n° 2") != std::string::npos);
  CHECK_FALSE(ParseClientConfig(servers + "archives: ['C:/jeu/data.grf']\n").has_value());
  CHECK_FALSE(ParseClientConfig(servers + "archives: ['\\\\partage\\data.grf']\n").has_value());
  CHECK_FALSE(ParseClientConfig(servers + "archives: ['grf\\..\\..\\data.grf']\n").has_value());
}

TEST_CASE("Configuration : sans section « patcher », le client ne se patche pas",
          "[app][config][patcher]") {
  const auto config = ParseClientConfig(kShipped);
  REQUIRE(config.has_value());
  CHECK_FALSE(config->patcher.has_value());
}

TEST_CASE("Configuration : les serveurs de patchs dans l'ordre, le dossier fermé par une barre",
          "[app][config][patcher]") {
  const std::string text = std::string(kShipped) + R"(
patcher:
  mirrors:
    - name: Moon Patch Server
      list: https://moonlight-destiny.fr/client/patcher/plist.txt
      archives: https://moonlight-destiny.fr/client/patcher/data
    - list: https://miroir.example/plist.txt
      archives: https://miroir.example/data/
      manifest: https://miroir.example/manifest.php
      files: https://miroir.example/files
  default_grf: moonlight.grf
  require_hashes: true
)";
  const auto config = ParseClientConfig(text);
  REQUIRE(config.has_value());
  REQUIRE(config->patcher.has_value());
  const moonclient::PatcherConfig& patcher = *config->patcher;
  REQUIRE(patcher.mirrors.size() == 2);
  CHECK(patcher.mirrors[0].name == "Moon Patch Server");
  CHECK(patcher.mirrors[0].list_url == "https://moonlight-destiny.fr/client/patcher/plist.txt");
  CHECK(patcher.mirrors[0].archive_url == "https://moonlight-destiny.fr/client/patcher/data/");
  // Sans nom, un serveur de patchs se nomme par sa liste.
  CHECK(patcher.mirrors[1].name == "https://miroir.example/plist.txt");
  CHECK(patcher.mirrors[1].archive_url == "https://miroir.example/data/");
  // Le miroir de fichiers est facultatif.
  CHECK(patcher.mirrors[0].manifest_url.empty());
  CHECK(patcher.mirrors[0].files_url.empty());
  CHECK(patcher.mirrors[1].manifest_url == "https://miroir.example/manifest.php");
  CHECK(patcher.mirrors[1].files_url == "https://miroir.example/files/");
  CHECK(patcher.default_grf == "moonlight.grf");
  CHECK(patcher.require_hashes);
  // Sans la clé, une archive absente n'est pas recréée.
  CHECK_FALSE(patcher.create_grf);
}

TEST_CASE("Configuration : un serveur de patchs en clair ou incomplet est refusé",
          "[app][config][patcher]") {
  const std::string servers = kMinimal;
  const auto with = [&](const std::string& section) {
    return ParseClientConfig(servers + section);
  };

  const auto plain = with(
      "patcher:\n  mirrors:\n    - {list: http://a/plist.txt, archives: https://a/d/}\n"
      "  default_grf: g.grf\n");
  REQUIRE_FALSE(plain.has_value());
  CHECK(plain.error().detail.find("https://") != std::string::npos);

  CHECK_FALSE(with("patcher:\n  mirrors: []\n  default_grf: g.grf\n").has_value());
  CHECK_FALSE(with("patcher:\n  mirrors:\n    - {list: https://a/p.txt, archives: https://a/d/}\n")
                  .has_value());
  CHECK_FALSE(with("patcher:\n  mirrors:\n    - {list: https://a/p.txt, archives: https://a/d/}\n"
                   "  default_grf: g.grf\n  require_hashes: peut-être\n")
                  .has_value());
  CHECK_FALSE(with("patcher:\n  mirrors:\n    - {list: https://a/p.txt, archives: https://a/d/}\n"
                   "  default_grf: g.grf\n  create_grf: [oui]\n")
                  .has_value());
  // Un manifeste sans son dossier, ou en clair, est refusé.
  CHECK_FALSE(with("patcher:\n  mirrors:\n    - {list: https://a/p.txt, archives: https://a/d/,"
                   " manifest: https://a/m.php}\n  default_grf: g.grf\n")
                  .has_value());
  CHECK_FALSE(with("patcher:\n  mirrors:\n    - {list: https://a/p.txt, archives: https://a/d/,"
                   " manifest: http://a/m.php, files: https://a/f/}\n  default_grf: g.grf\n")
                  .has_value());
  // Par défaut, une empreinte absente n'est pas fatale.
  const auto lenient = with(
      "patcher:\n  mirrors:\n    - {list: https://a/p.txt, archives: https://a/d/}\n"
      "  default_grf: g.grf\n");
  REQUIRE(lenient.has_value());
  CHECK_FALSE(lenient->patcher->require_hashes);
}

TEST_CASE("Configuration : le serveur test, local par défaut ou tel que déclaré", "[app][config]") {
  const auto absent = ParseClientConfig(kMinimal);
  REQUIRE(absent.has_value());
  CHECK(absent->test_server.host == moonclient::kDefaultTestServerHost);
  CHECK(absent->test_server.port == moonclient::kDefaultTestServerPort);

  const auto declared =
      ParseClientConfig(std::string(kMinimal) + "test_server: essai.local:7000\n");
  REQUIRE(declared.has_value());
  CHECK(declared->test_server.host == "essai.local");
  CHECK(declared->test_server.port == 7000);

  // IPv6 : entre crochets, que la lecture retire.
  const auto bracketed = ParseClientConfig(std::string(kMinimal) + "test_server: \"[::1]:6900\"\n");
  REQUIRE(bracketed.has_value());
  CHECK(bracketed->test_server.host == "::1");
}

TEST_CASE("Configuration : un serveur test qui n'est pas hôte:port est refusé", "[app][config]") {
  for (const char* wrong : {"test_server: localhost\n", "test_server: :6900\n",
                            "test_server: h:0\n", "test_server: h:70000\n", "test_server: h:69x\n",
                            "test_server: \"::1:6900\"\n", "test_server: [h, 1]\n"}) {
    const auto config = ParseClientConfig(std::string(kMinimal) + wrong);
    REQUIRE_FALSE(config.has_value());
    CHECK(config.error().detail.find("test_server") != std::string::npos);
  }
}

TEST_CASE("Configuration : le site d'un serveur, une origine https sans chemin", "[app][config]") {
  const auto declared = ParseClientConfig(
      "servers:\n  - {address: h, port: 1, web_login: \"https://essai.local:8443/\"}\n"
      "  - {address: p, port: 2}\narchives: [data.grf]\n");
  REQUIRE(declared.has_value());
  // La barre finale se retire : une adresse se compose en y ajoutant un chemin.
  CHECK(declared->servers[0].web_login == "https://essai.local:8443");
  CHECK(declared->servers[1].web_login.empty());

  for (const char* wrong : {"http://essai.local", "https://", "https://h/api", "essai.local"}) {
    const auto config =
        ParseClientConfig(std::string("servers:\n  - {address: h, port: 1, web_login: \"") + wrong +
                          "\"}\narchives: [data.grf]\n");
    REQUIRE_FALSE(config.has_value());
    CHECK(config.error().detail.find("web_login") != std::string::npos);
  }
}

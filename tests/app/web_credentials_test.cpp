// CE QUE LA PORTE MOONLIGHT RETIENT, ET CE QU'ELLE REFUSE DE RETENIR.
//
// 🔴 LE DÉFAUT QU'ON REDOUTE ICI N'EST PAS UNE PANNE, C'EST UN SECRET ÉCRIT EN
// CLAIR. Un coffre qui refuse de sceller — système indisponible, politique de
// la machine — ne doit pas faire retomber le mot de passe dans le fichier :
// personne ne s'en plaindrait, tout marcherait, et le mot de passe du site
// serait lisible dans un fichier qu'on colle dans une conversation de
// dépannage.
//
// ⚠ LE COFFRE EST FEINT ICI, ET C'EST LE BUT. `SystemSecretStore` demande le
// coffre du système, donc un compte de session ; la POLITIQUE — quoi retenir, quoi
// refuser, quoi effacer — n'a besoin de rien de tout cela. C'est la coupure
// que `SecretStore` achète.

#include "web/web_credentials.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "support/temp_directory.h"

using moonclient::web::LoadWebCredentials;
using moonclient::web::SaveWebCredentials;
using moonclient::web::SecretStore;
using moonclient::web::WebCredentials;
using moonclient::web::WebCredentialsPath;

namespace {

// UN COFFRE FEINT : il scelle en retournant les octets.
//
// ⚠ RETOURNER PLUTÔT QUE RECOPIER : un scellé identique au clair passerait
// tous ces essais même si l'on oubliait d'appeler le coffre. Renversé, un
// secret lu sans passer par lui se verrait à l'endroit.
class MirrorVault final : public SecretStore {
 public:
  [[nodiscard]] std::string Seal(std::string_view plain) override {
    return std::string(plain.rbegin(), plain.rend());
  }
  [[nodiscard]] std::string Unseal(std::string_view sealed) override {
    return std::string(sealed.rbegin(), sealed.rend());
  }
};

// UN COFFRE QUI REFUSE DE SCELLER : le système n'a pas voulu.
class DeafVault final : public SecretStore {
 public:
  [[nodiscard]] std::string Seal(std::string_view) override { return {}; }
  [[nodiscard]] std::string Unseal(std::string_view sealed) override {
    return std::string(sealed.rbegin(), sealed.rend());
  }
};

// UN COFFRE D'UN AUTRE COMPTE WINDOWS : il scelle, mais n'ouvre pas le nôtre.
class StrangerVault final : public SecretStore {
 public:
  [[nodiscard]] std::string Seal(std::string_view plain) override {
    return std::string(plain.rbegin(), plain.rend());
  }
  [[nodiscard]] std::string Unseal(std::string_view) override { return {}; }
};

[[nodiscard]] std::string FileText(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    return {};
  }
  const std::uintmax_t weight = std::filesystem::file_size(path);
  std::string content(static_cast<std::size_t>(weight), '\0');
  std::ifstream reading(path, std::ios::binary);
  reading.read(content.data(), static_cast<std::streamsize>(weight));
  content.resize(static_cast<std::size_t>(reading.gcount()));
  return content;
}

}  // namespace

TEST_CASE("Compte Moonlight : un premier lancement ne retient rien", "[app][web][memoire]") {
  const ro::test_support::TempDirectory home;
  MirrorVault vault;
  const WebCredentials found = LoadWebCredentials(home.path(), vault);
  CHECK(found.member.empty());
  CHECK(found.password.empty());
  CHECK_FALSE(found.remember_member);
  CHECK_FALSE(found.remember_password);
}

TEST_CASE("Compte Moonlight : l'identifiant seul fait l'aller-retour", "[app][web][memoire]") {
  const ro::test_support::TempDirectory home;
  MirrorVault vault;
  CHECK(SaveWebCredentials(home.path(), vault,
                           WebCredentials{.member = "Stingor", .remember_member = true}));

  const WebCredentials found = LoadWebCredentials(home.path(), vault);
  CHECK(found.member == "Stingor");
  CHECK(found.remember_member);
  // ⚠ ET SURTOUT PAS LE MOT DE PASSE : une case en coche une, pas deux.
  CHECK(found.password.empty());
  CHECK_FALSE(found.remember_password);
}

TEST_CASE("Compte Moonlight : le mot de passe passe par le coffre", "[app][web][memoire]") {
  const ro::test_support::TempDirectory home;
  MirrorVault vault;
  CHECK(SaveWebCredentials(home.path(), vault,
                           WebCredentials{.member = "Stingor",
                                          .password = "azerty-du-site",
                                          .remember_member = true,
                                          .remember_password = true}));

  const WebCredentials found = LoadWebCredentials(home.path(), vault);
  CHECK(found.member == "Stingor");
  CHECK(found.password == "azerty-du-site");
  CHECK(found.remember_password);

  // 🔴 ET RIEN EN CLAIR SUR LE DISQUE. C'est l'assertion qui compte : le reste
  // de cet essai passerait tout aussi bien si le fichier portait le mot de
  // passe tel quel à côté de son scellé.
  const std::string written = FileText(WebCredentialsPath(home.path()));
  CHECK_FALSE(written.empty());
  CHECK(written.find("azerty-du-site") == std::string::npos);
}

TEST_CASE("Compte Moonlight : un coffre qui refuse n'écrit RIEN en clair", "[app][web][memoire]") {
  const ro::test_support::TempDirectory home;
  DeafVault deaf;
  CHECK(SaveWebCredentials(home.path(), deaf,
                           WebCredentials{.member = "Stingor",
                                          .password = "azerty-du-site",
                                          .remember_member = true,
                                          .remember_password = true}));

  // 🔴 L'IDENTIFIANT RESTE — il n'a jamais eu besoin d'être scellé —, LE
  // SECRET DISPARAÎT. Se rabattre sur le clair poserait sur le disque, sans le
  // dire, exactement ce que le joueur avait demandé de protéger.
  const std::string written = FileText(WebCredentialsPath(home.path()));
  CHECK(written.find("Stingor") != std::string::npos);
  CHECK(written.find("azerty-du-site") == std::string::npos);

  MirrorVault vault;
  const WebCredentials found = LoadWebCredentials(home.path(), vault);
  CHECK(found.member == "Stingor");
  CHECK(found.password.empty());
  CHECK_FALSE(found.remember_password);
}

TEST_CASE("Compte Moonlight : le secret suit l'identifiant", "[app][web][memoire]") {
  const ro::test_support::TempDirectory home;
  MirrorVault vault;
  // ⚠ SANS NOM À REMPLIR, un mot de passe retenu ne servirait personne et
  // resterait sur le disque pour rien.
  CHECK(SaveWebCredentials(home.path(), vault,
                           WebCredentials{.member = "Stingor",
                                          .password = "azerty-du-site",
                                          .remember_member = false,
                                          .remember_password = true}));
  CHECK_FALSE(std::filesystem::exists(WebCredentialsPath(home.path())));
}

TEST_CASE("Compte Moonlight : tout oublier efface le fichier", "[app][web][memoire]") {
  const ro::test_support::TempDirectory home;
  MirrorVault vault;
  CHECK(SaveWebCredentials(home.path(), vault,
                           WebCredentials{.member = "Stingor",
                                          .password = "azerty-du-site",
                                          .remember_member = true,
                                          .remember_password = true}));
  REQUIRE(std::filesystem::exists(WebCredentialsPath(home.path())));

  // ⚠ LE FICHIER DISPARAÎT, IL NE SE VIDE PAS : un fichier vide laisserait
  // croire qu'une mémoire existe et qu'elle est cassée.
  CHECK(SaveWebCredentials(home.path(), vault, WebCredentials{}));
  CHECK_FALSE(std::filesystem::exists(WebCredentialsPath(home.path())));
}

TEST_CASE("Compte Moonlight : un scellé d'un autre compte Windows s'oublie",
          "[app][web][memoire]") {
  const ro::test_support::TempDirectory home;
  StrangerVault stranger;
  CHECK(SaveWebCredentials(home.path(), stranger,
                           WebCredentials{.member = "Stingor",
                                          .password = "azerty-du-site",
                                          .remember_member = true,
                                          .remember_password = true}));

  // 🔴 CE N'EST PAS UNE PANNE : le fichier vient d'ailleurs, et cela vaut
  // « rien de retenu ». L'identifiant, lui, n'était pas scellé et survit.
  const WebCredentials found = LoadWebCredentials(home.path(), stranger);
  CHECK(found.member == "Stingor");
  CHECK(found.remember_member);
  CHECK(found.password.empty());
  // ⚠ ET LA CASE SE DÉCOCHE : cochée, l'écran promettrait une mémoire qui
  // n'est plus là, et le joueur chercherait pourquoi son champ reste vide.
  CHECK_FALSE(found.remember_password);
}

TEST_CASE("Compte Moonlight : un secret abîmé n'atteint pas le coffre", "[app][web][memoire]") {
  const ro::test_support::TempDirectory home;
  MirrorVault vault;
  // ⚠ UN HEXADÉCIMAL IMPAIR, ET UN CARACTÈRE QUI N'EN EST PAS : un fichier
  // tronqué ou retouché à la main. Le refuser ICI évite d'envoyer au système
  // des octets à moitié justes — il répondrait « je n'ouvre pas », et le
  // joueur lirait un message sur son compte Windows pour un fichier
  // simplement abîmé.
  home.WriteText("savedata/moonlight_login.yml", "login: Stingor\nsealed_secret: 0a1b2c3\n");
  const WebCredentials odd = LoadWebCredentials(home.path(), vault);
  CHECK(odd.member == "Stingor");
  CHECK(odd.password.empty());

  home.WriteText("savedata/moonlight_login.yml", "login: Stingor\nsealed_secret: 0a1b2czz\n");
  const WebCredentials wrong = LoadWebCredentials(home.path(), vault);
  CHECK(wrong.member == "Stingor");
  CHECK(wrong.password.empty());
}

TEST_CASE("Compte Moonlight : un fichier illisible vaut une mémoire vide", "[app][web][memoire]") {
  const ro::test_support::TempDirectory home;
  MirrorVault vault;
  // Ni un map, ni du YAML : un fichier qui a été remplacé par autre chose.
  home.WriteText("savedata/moonlight_login.yml", "[[[ pas du tout du YAML\n");
  const WebCredentials found = LoadWebCredentials(home.path(), vault);
  CHECK(found.member.empty());
  CHECK(found.password.empty());
  CHECK_FALSE(found.remember_member);
}

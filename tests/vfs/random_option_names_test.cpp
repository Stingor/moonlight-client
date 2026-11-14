// LES NOMS DES OPTIONS ALÉATOIRES : la table YAML lue, et un gabarit qui ne
// passe jamais à `printf`.

#include "ro/vfs/random_option_names.h"

#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::FillOptionTemplate;
using ro::vfs::ParseRandomOptionNames;
using ro::vfs::RandomOptionNames;
using ro::vfs::ReadRandomOptionNames;
using ro::vfs::Vfs;

namespace {

// La forme que le script écrit : un en-tête, une option nommée, une option dont
// le gabarit porte `%%` et des guillemets, une option sans libellé.
constexpr std::string_view kTable = R"yaml(# Les options aléatoires d'équipement
version: 1
mode: Prerenewal
options:
  1: {option: VAR_MAXHPAMOUNT, name: "MaxHP +%d"}
  13: {option: VAR_ATKPERCENT, name: "\"ATK\" +%d%%"}
  86: {option: VAR_SERVER_ONLY}
)yaml";

[[nodiscard]] RandomOptionNames Read(const TempDirectory& client) {
  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  auto names = ReadRandomOptionNames(*vfs);
  REQUIRE(names.has_value());
  return std::move(*names);
}

}  // namespace

TEST_CASE("Options aléatoires : le gabarit se trouve par l'identifiant du paquet",
          "[vfs][description]") {
  const auto names = ParseRandomOptionNames(kTable);
  REQUIRE(names.has_value());
  CHECK(names->Format(1, 250) == "MaxHP +250");
  CHECK(names->Format(13, 5) == "\"ATK\" +5%");
  // Connue du serveur sans libellé, ou inconnue : rien, l'appelant écrit le numéro.
  CHECK(names->Format(86, 1).empty());
  CHECK(names->Format(99, 1).empty());
  CHECK(names->templates.size() == 2);
}

TEST_CASE("Options aléatoires : la table se lit par le VFS", "[vfs][description]") {
  TempDirectory client;
  client.WriteText("data/yaml_files/random_options.yml", std::string(kTable));
  CHECK(Read(client).Format(1, 7) == "MaxHP +7");
}

TEST_CASE("Options aléatoires : absente, rien ne se nomme", "[vfs][description]") {
  TempDirectory client;
  client.WriteText("data/marqueur.txt", "");
  CHECK(Read(client).empty());
}

TEST_CASE("Options aléatoires : une table mal formée est refusée", "[vfs][description]") {
  CHECK_FALSE(ParseRandomOptionNames("version: 2\noptions: {}\n").has_value());
  CHECK_FALSE(ParseRandomOptionNames("version: 1\n").has_value());
  CHECK_FALSE(ParseRandomOptionNames("version: 1\noptions: [1, 2]\n").has_value());
  CHECK_FALSE(ParseRandomOptionNames("version: 1\noptions: {1: MaxHP}\n").has_value());
  CHECK_FALSE(ParseRandomOptionNames("version: 1\noptions: {1: {name: [a]}}\n").has_value());
  CHECK_FALSE(ParseRandomOptionNames("version: 1\noptions: {x: {name: a}}\n").has_value());
  CHECK_FALSE(ParseRandomOptionNames("[").has_value());
}

TEST_CASE("Options aléatoires : un gabarit ne remplit que %d et %%", "[vfs][description]") {
  CHECK(FillOptionTemplate("Délai -%d%%", 10) == "Délai -10%");
  // Une seconde valeur, ou une autre conversion : rien plutôt qu'un risque.
  CHECK(FillOptionTemplate("%d / %d", 1).empty());
  CHECK(FillOptionTemplate("%s", 1).empty());
  CHECK(FillOptionTemplate("fin %", 1).empty());
}

#include "ro/vfs/res_name_table.h"

#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/grf_builder.h"
#include "support/temp_directory.h"

using ro::test_support::BuildGrf;
using ro::test_support::Bytes;
using ro::test_support::TempDirectory;
using ro::vfs::ResNameTable;
using ro::vfs::Vfs;

namespace {

// Ce que la conversion tire de l'extrait du fichier livré que lisait l'ancien
// lecteur, à la lettre :
//
//   // pvp
//   pvp_n_1-1.gnd#prt_maze02.gnd#     … (et .gat, .rsw)
//   ordeal_1-3.gnd#ordeal_a00.gnd#    ordeal_1-3.gat#ordeal_a03.gat#  …
//   PVP_N_9-9.rsw#prt_maze02.rsw#     une clé mal casée
//   carte.rsw#premier.rsw#            puis carte.rsw#second.rsw#
//   seule.rsw#                        puis, à la ligne, cible.rsw#
//   유저인터페이스\map\a.bmp#유저인터페이스\map\b.bmp#
constexpr std::string_view kConverted =
    "version: 1\n"
    "files:\n"
    "  PVP_N_9-9.rsw: prt_maze02.rsw\n"
    "  carte.rsw: premier.rsw\n"
    "  ordeal_1-3.gat: ordeal_a03.gat\n"
    "  ordeal_1-3.gnd: ordeal_a00.gnd\n"
    "  ordeal_1-3.rsw: ordeal_a03.rsw\n"
    "  pvp_n_1-1.gat: prt_maze02.gat\n"
    "  pvp_n_1-1.gnd: prt_maze02.gnd\n"
    "  pvp_n_1-1.rsw: prt_maze02.rsw\n"
    "  seule.rsw: cible.rsw\n"
    "  유저인터페이스\\map\\a.bmp: 유저인터페이스\\map\\b.bmp\n";

[[nodiscard]] ResNameTable Parsed(std::string_view yaml_text) {
  auto table = ResNameTable::Parse(yaml_text);
  REQUIRE(table.has_value());
  return std::move(*table);
}

}  // namespace

TEST_CASE("une carte clonée lit les fichiers de son modèle", "[res_name_table]") {
  const ResNameTable table = Parsed(kConverted);

  REQUIRE(table.size() == 10);
  CHECK(table.MapFilePath("pvp_n_1-1.gat") == "data\\prt_maze02.gat");
  CHECK(table.MapFilePath("pvp_n_1-1.rsw") == "data\\prt_maze02.rsw");
  CHECK(table.MapFilePath("pvp_n_1-1.gnd") == "data\\prt_maze02.gnd");
}

TEST_CASE("chaque fichier d'une carte se traduit pour lui-même", "[res_name_table]") {
  const ResNameTable table = Parsed(kConverted);

  // 🔴 LE RELIEF ET LA SCÈNE NE VIENNENT PAS DE LA MÊME CARTE, et c'est le cas
  // de neuf cartes du fichier livré. Traduire le nom de la carte une seule fois
  // pour ses trois fichiers rendrait ici « ordeal_a03.gnd », qui n'existe pas.
  CHECK(table.CloneOf("ordeal_1-3.gnd") == "ordeal_a00.gnd");
  CHECK(table.CloneOf("ordeal_1-3.rsw") == "ordeal_a03.rsw");
  CHECK(table.CloneOf("ordeal_1-3.gat") == "ordeal_a03.gat");
}

TEST_CASE("un nom absent de la table reste lui-même", "[res_name_table]") {
  const ResNameTable table = Parsed(kConverted);

  CHECK(table.CloneOf("prontera.rsw") == "prontera.rsw");
  CHECK(table.MapFilePath("prontera.gat") == "data\\prontera.gat");
}

TEST_CASE("la casse du nom demandé est abaissée avant la recherche", "[res_name_table]") {
  const ResNameTable table = Parsed(kConverted);

  // Le client abaisse son tampon AVANT de traduire ; le nom rendu l'est donc
  // aussi, qu'il ait été traduit ou non.
  CHECK(table.CloneOf("PVP_N_1-1.RSW") == "prt_maze02.rsw");
  CHECK(table.CloneOf("Prontera.Rsw") == "prontera.rsw");
}

TEST_CASE("une clé écrite avec une majuscule ne se trouve jamais", "[res_name_table]") {
  // Seul le nom demandé s'abaisse ; la clé se compare telle que la table
  // l'écrit. Une entrée mal casée est donc lue, rangée, et sans effet.
  const ResNameTable table = Parsed(kConverted);

  CHECK(table.clones().contains("PVP_N_9-9.rsw"));
  CHECK(table.CloneOf("pvp_n_9-9.rsw") == "pvp_n_9-9.rsw");
  CHECK(table.CloneOf("PVP_N_9-9.rsw") == "pvp_n_9-9.rsw");
}

TEST_CASE("la conversion garde la première entrée et apparie une ligne seule", "[res_name_table]") {
  // Le fichier livré répète 97 de ses entrées, et le client garde la
  // première ; une ligne à un seul dièse s'apparie avec le jeton suivant.
  const ResNameTable table = Parsed(kConverted);

  CHECK(table.CloneOf("carte.rsw") == "premier.rsw");
  CHECK(table.CloneOf("seule.rsw") == "cible.rsw");
}

TEST_CASE("un nom coréen se range en CP949, comme l'installation l'écrit", "[res_name_table]") {
  const ResNameTable table = Parsed(kConverted);

  // « 유저인터페이스 » en CP949.
  const std::string folder = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";
  CHECK(table.CloneOf(folder + "\\map\\a.bmp") == folder + "\\map\\b.bmp");
}

TEST_CASE("une table mal formée est refusée", "[res_name_table]") {
  CHECK_FALSE(ResNameTable::Parse("version: 2\nfiles: {}\n").has_value());
  CHECK_FALSE(ResNameTable::Parse("version: 1\nfiles: [a]\n").has_value());
  CHECK_FALSE(ResNameTable::Parse("version: 1\n").has_value());
  CHECK_FALSE(ResNameTable::Parse("pas: [du yaml").has_value());
}

TEST_CASE("une installation sans table charge ses cartes en direct", "[res_name_table]") {
  const TempDirectory client;
  client.WriteFile("donnees.grf", BuildGrf({{"data\\prontera.rsw", Bytes("scène")}}));

  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"donnees.grf"}});
  REQUIRE(vfs.has_value());
  CHECK(vfs->res_names().size() == 0);
  CHECK(vfs->res_names().MapFilePath("prontera.rsw") == "data\\prontera.rsw");
}

TEST_CASE("la table de l'installation est lue au montage", "[res_name_table]") {
  const TempDirectory client;
  client.WriteText("data/yaml_files/res_names.yml", std::string(kConverted));
  client.WriteFile("donnees.grf", BuildGrf({{"data\\prt_maze02.gat", Bytes("grille")}}));

  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"donnees.grf"}});
  REQUIRE(vfs.has_value());
  REQUIRE(vfs->res_names().size() == 10);

  auto bytes = vfs->Read(vfs->res_names().MapFilePath("pvp_n_1-1.gat"));
  REQUIRE(bytes.has_value());
  CHECK(std::string(bytes->begin(), bytes->end()) == "grille");
}

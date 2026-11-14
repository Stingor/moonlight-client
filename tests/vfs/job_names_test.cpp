// LES NOMS DE MÉTIER : la table lue, et le nom qu'un identifiant désigne.

#include "ro/vfs/job_names.h"

#include <string>
#include <string_view>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::JobNames;
using ro::vfs::ParseJobNames;
using ro::vfs::ReadJobNames;
using ro::vfs::Vfs;

namespace {

// La forme que le script écrit : un en-tête, des noms entre guillemets, le nom
// du serveur en commentaire.
constexpr std::string_view kTable = R"yaml(# Le nom affiché de chaque métier
version: 1
jobs:
  0: "Novice"  # Novice
  7: "Knight"  # Knight
  4008: "Lord \"Knight\""  # Lord_Knight
  4332: "Rune Knight 2ND"  # Rune_Knight_2nd
)yaml";

void MakeClient(const TempDirectory& client) { client.WriteText("data/marqueur.txt", ""); }

[[nodiscard]] JobNames Read(const TempDirectory& client) {
  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  auto names = ReadJobNames(*vfs);
  REQUIRE(names.has_value());
  return std::move(*names);
}

}  // namespace

TEST_CASE("Noms de métiers : chaque identifiant reçoit son nom", "[vfs][metiers]") {
  const auto names = ParseJobNames(kTable);
  REQUIRE(names.has_value());
  CHECK(names->size() == 4);
  CHECK(names->Find(0) == "Novice");
  CHECK(names->Find(7) == "Knight");
  CHECK(names->Find(4008) == "Lord \"Knight\"");
  CHECK(names->Find(4332) == "Rune Knight 2ND");
}

TEST_CASE("Noms de métiers : ce que la table ignore n'a pas de nom", "[vfs][metiers]") {
  // ⚠ RIEN N'EST PAS UNE PANNE : la fiche écrit alors ses niveaux seuls.
  const auto names = ParseJobNames(kTable);
  REQUIRE(names.has_value());
  CHECK(names->Find(13).empty());
  CHECK(names->Find(4307).empty());
}

TEST_CASE("Noms de métiers : une table mal formée se refuse", "[vfs][metiers]") {
  CHECK_FALSE(ParseJobNames("- 1\n- 2\n").has_value());
  CHECK_FALSE(ParseJobNames("version: 2\njobs:\n  0: \"Novice\"\n").has_value());
  CHECK_FALSE(ParseJobNames("version: 1\n").has_value());
  CHECK_FALSE(ParseJobNames("version: 1\njobs: [Novice]\n").has_value());
  CHECK_FALSE(ParseJobNames("version: 1\njobs:\n  novice: \"Novice\"\n").has_value());
  CHECK_FALSE(ParseJobNames("version: 1\njobs:\n  0: [Novice]\n").has_value());
  CHECK_FALSE(ParseJobNames("version: 1\njobs: {0: \"Novice\"\n").has_value());
}

TEST_CASE("Noms de métiers : la table se lit à sa place dans l'installation", "[vfs][metiers]") {
  TempDirectory client;
  MakeClient(client);
  client.WriteText("data/yaml_files/job_names.yml", std::string(kTable));

  const JobNames names = Read(client);
  CHECK(names.size() == 4);
  CHECK(names.Find(4008) == "Lord \"Knight\"");
}

TEST_CASE("Noms de métiers : une installation sans la table se joue encore", "[vfs][metiers]") {
  TempDirectory client;
  MakeClient(client);

  const JobNames names = Read(client);
  CHECK(names.empty());
}

TEST_CASE("Noms de métiers : les tables Lua du client d'origine ne se lisent plus",
          "[vfs][metiers]") {
  // Le cap : plus aucune dépendance aux `.lub` du client d'origine. Une
  // installation qui les porte encore, sans la table convertie, n'a pas de noms.
  TempDirectory client;
  MakeClient(client);
  client.WriteText("data/luafiles514/lua files/datainfo/jobidentity.lub",
                   "JTtbl = { JT_NOVICE = 0 }\n");
  client.WriteText("data/luafiles514/lua files/datainfo/pcjobname.lub",
                   "PCJobNameTable = { [JOBID.JT_NOVICE] = \"Novice\" }\n");

  const JobNames names = Read(client);
  CHECK(names.empty());
}

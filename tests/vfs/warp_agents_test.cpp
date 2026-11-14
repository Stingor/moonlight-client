// LES WARP AGENT : la table lue, et la classe de la figure qu'une carte porte.

#include "ro/vfs/warp_agents.h"

#include <cstdint>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "support/temp_directory.h"

using ro::test_support::TempDirectory;
using ro::vfs::Vfs;
using ro::vfs::WarpAgents;

namespace {

constexpr std::uint32_t kAgentClass = 90;
constexpr std::uint32_t kWinterAgentClass = 714;

// La forme que le script écrit : la figure par son nom et par sa classe, le
// fichier du PNJ en commentaire.
constexpr std::string_view kTable = R"yaml(# Les Warp Agent du serveur
version: 1
agents:
  prontera: {sprite: "4_F_01", class: 90}  # moon/warp_agent.npc
  XMAS: {sprite: "4_F_06", class: 714}  # moon/warp_agent.npc
)yaml";

void MakeClient(const TempDirectory& client) { client.WriteText("data/marqueur.txt", ""); }

}  // namespace

TEST_CASE("Warp Agent : chaque carte reçoit la classe de sa figure", "[vfs][warp_agents]") {
  const auto agents = WarpAgents::Parse(kTable);
  REQUIRE(agents.has_value());
  CHECK(agents->ClassAt("prontera") == kAgentClass);
  // Le nom interne d'une carte se compare en minuscules.
  CHECK(agents->ClassAt("xmas") == kWinterAgentClass);
}

TEST_CASE("Warp Agent : une carte que la table ignore n'en a pas", "[vfs][warp_agents]") {
  const auto agents = WarpAgents::Parse(kTable);
  REQUIRE(agents.has_value());
  CHECK_FALSE(agents->ClassAt("geffen").has_value());
  CHECK_FALSE(agents->ClassAt("").has_value());
}

TEST_CASE("Warp Agent : une table mal formée se refuse", "[vfs][warp_agents]") {
  CHECK_FALSE(WarpAgents::Parse("- 1\n- 2\n").has_value());
  CHECK_FALSE(WarpAgents::Parse("version: 2\nagents:\n  prontera: {class: 90}\n").has_value());
  CHECK_FALSE(WarpAgents::Parse("version: 1\n").has_value());
  CHECK_FALSE(WarpAgents::Parse("version: 1\nagents: [prontera]\n").has_value());
  CHECK_FALSE(WarpAgents::Parse("version: 1\nagents:\n  prontera: 90\n").has_value());
  CHECK_FALSE(WarpAgents::Parse("version: 1\nagents:\n  prontera: {sprite: x}\n").has_value());
  CHECK_FALSE(WarpAgents::Parse("version: 1\nagents:\n  prontera: {class: -1}\n").has_value());
  CHECK_FALSE(WarpAgents::Parse("version: 1\nagents:\n  prontera: {class: 4_F_01}\n").has_value());
  CHECK_FALSE(WarpAgents::Parse("version: 1\nagents:\n  prontera: {class: [90]}\n").has_value());
  CHECK_FALSE(WarpAgents::Parse("version: 1\nagents: {prontera: {class: 90}\n").has_value());
}

TEST_CASE("Warp Agent : la table se lit à sa place dans l'installation", "[vfs][warp_agents]") {
  TempDirectory client;
  MakeClient(client);
  client.WriteText("data/yaml_files/warp_agents.yml", std::string(kTable));

  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  const auto agents = WarpAgents::Load(*vfs);
  REQUIRE(agents.has_value());
  CHECK(agents->ClassAt("prontera") == kAgentClass);
}

TEST_CASE("Warp Agent : une installation sans la table n'en montre aucun", "[vfs][warp_agents]") {
  TempDirectory client;
  MakeClient(client);

  auto vfs = Vfs::Mount({.root = client.path()});
  REQUIRE(vfs.has_value());
  CHECK_FALSE(WarpAgents::Load(*vfs).has_value());
}

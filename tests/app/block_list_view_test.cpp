// LA VUE DES JOUEURS BLOQUÉS : deux listes de chuchotements et celle de la
// voix, fondues en une ligne par nom.

#include "chat/block_list_view.h"

#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "chat/opening_commands.h"

using moonclient::BlockListFacts;
using moonclient::ComposeBlockList;

TEST_CASE("Joueurs bloqués : une ligne par nom, triée, les canaux réunis", "[app][block_list]") {
  const std::vector<std::string> whispers = {"Zoé", "Bob"};
  const std::vector<std::string> kept = {"Bob", "Alice"};
  const std::vector<std::string> voice = {"Carl", "Bob"};
  const ro::ui::BlockListView view = ComposeBlockList(
      BlockListFacts{.whispers = whispers, .kept_whispers = kept, .voice_blocked = voice});
  REQUIRE(view.players.size() == 4);
  CHECK(view.players[0].name == "Alice");
  CHECK(view.players[1].name == "Bob");
  CHECK(view.players[2].name == "Carl");
  CHECK(view.players[3].name == "Zoé");
  CHECK(view.players[1].whispers);
  CHECK(view.players[1].voice);
  CHECK(view.players[2].voice);
  CHECK_FALSE(view.players[2].whispers);
  CHECK(view.players[0].whispers);
  CHECK_FALSE(view.players[0].voice);
}

TEST_CASE("Joueurs bloqués : la casse fait deux joueurs, comme chez le serveur",
          "[app][block_list]") {
  const std::vector<std::string> whispers = {"bob", "Bob"};
  const ro::ui::BlockListView view = ComposeBlockList(BlockListFacts{.whispers = whispers});
  CHECK(view.players.size() == 2);
}

TEST_CASE("Joueurs bloqués : la voix ne coupe que qui elle entend", "[app][block_list]") {
  const std::vector<std::string> whispers = {"Proche", "Loin"};
  const std::vector<std::string> voice = {"Coupé"};
  BlockListFacts facts{.whispers = whispers, .voice_blocked = voice};
  SECTION("sans oreille, seule une voix déjà coupée se rétablit") {
    const ro::ui::BlockListView view = ComposeBlockList(facts);
    for (const ro::ui::BlockedPlayerView& player : view.players) {
      INFO(player.name);
      CHECK(player.voice_reachable == player.voice);
    }
  }
  SECTION("avec elle, celui qu'elle entend se coupe aussi") {
    facts.voice_hears = [](std::string_view name) { return name == "Proche"; };
    const ro::ui::BlockListView view = ComposeBlockList(facts);
    REQUIRE(view.players.size() == 3);
    CHECK(view.players[0].name == "Coupé");
    CHECK(view.players[0].voice_reachable);
    CHECK(view.players[1].name == "Loin");
    CHECK_FALSE(view.players[1].voice_reachable);
    CHECK(view.players[2].name == "Proche");
    CHECK(view.players[2].voice_reachable);
  }
}

TEST_CASE("Joueurs bloqués : les états de la partie passent tels quels", "[app][block_list]") {
  const ro::ui::BlockListView view = ComposeBlockList(BlockListFacts{.whispers_kept = true,
                                                                     .whispers_from_everyone = true,
                                                                     .connected = true,
                                                                     .voice_online = true});
  CHECK(view.players.empty());
  CHECK(view.whispers_kept);
  CHECK(view.whispers_from_everyone);
  CHECK(view.connected);
  CHECK(view.voice_online);
}

TEST_CASE("Joueurs bloqués : /blocage et ses alias, à la casse près", "[app][block_list]") {
  CHECK(moonclient::IsBlockListCommand("/blocage"));
  CHECK(moonclient::IsBlockListCommand("/BLOCAGES"));
  CHECK(moonclient::IsBlockListCommand("/blocklist"));
  CHECK_FALSE(moonclient::IsBlockListCommand("/blocage Bob"));
  CHECK_FALSE(moonclient::IsBlockListCommand("/bloc"));
}

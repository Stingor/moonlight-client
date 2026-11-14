#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_notice.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/banned_words.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

// LES ANNONCES FILTRÉES : la voix d'un nom ignoré se tait, et les mots
// interdits se masquent là où le client d'origine les masque.

using ro::net::ApplyBroadcastFrame;
using ro::net::BroadcastFilter;
using ro::net::FrameOutcome;
using ro::sim::BannedWords;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

// « micc » + le nom sur 24 octets + la couleur + le texte.
std::string Voice(std::string name, std::string_view text) {
  constexpr std::size_t kNameSize = 24;
  name.resize(kNameSize, '\0');
  return "micc" + name + "00FF00" + std::string(text);
}

FrameOutcome Broadcast(World& world, std::string message, const BroadcastFilter& filter) {
  ro::proto::ZcBroadcast announcement;
  announcement.message = std::move(message);
  return ApplyBroadcastFrame(FrameOf(Encode(announcement)), world, filter, 0);
}

}  // namespace

TEST_CASE("Annonce filtrée : la voix d'un nom ignoré se tait", "[net][notice]") {
  const BroadcastFilter filter{.ignores = [](std::string_view name) { return name == "Bob"; }};
  World world;
  CHECK(Broadcast(world, Voice("Bob", "Achetez !"), filter) == FrameOutcome::kIgnored);
  // ⚠ LA CASSE COMPTE : « bob » n'est pas « Bob ».
  CHECK(Broadcast(world, Voice("bob", "Bonjour."), filter) == FrameOutcome::kApplied);
  // Une annonce sans voix ne nomme personne, et passe.
  CHECK(Broadcast(world, "toolFF0000Bob", filter) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines()[0].text == "Bonjour.");
  CHECK(world.chat_lines()[1].text == "Bob");
}

TEST_CASE("Annonce filtrée : les mots interdits se masquent dans la voix et l'annonce colorée",
          "[net][notice]") {
  const BannedWords banned(std::vector<std::string>{"zut"});
  const BroadcastFilter filter{.banned = &banned};
  World world;
  CHECK(Broadcast(world, Voice("Alice", "Zut alors"), filter) == FrameOutcome::kApplied);
  // ⚠ SEULE LA VOIX SE MASQUE parmi les annonces à étiquette.
  CHECK(Broadcast(world, "toolFF0000zut", filter) == FrameOutcome::kApplied);
  CHECK(Broadcast(world, "zut", filter) == FrameOutcome::kApplied);

  ro::proto::ZcBroadcast2 colored;
  colored.message = "Alice#1234: zut";
  CHECK(ApplyBroadcastFrame(FrameOf(Encode(colored)), world, filter, 0) == FrameOutcome::kApplied);

  REQUIRE(world.chat_lines().size() == 4);
  CHECK(world.chat_lines()[0].text == "*** alors");
  CHECK(world.chat_lines()[1].text == "zut");
  CHECK(world.chat_lines()[2].text == "zut");
  CHECK(world.chat_lines()[3].text == "Alice: ***");
}

TEST_CASE("Annonce filtrée : toute autre trame n'est pas la sienne", "[net][notice]") {
  World world;
  ro::proto::ZcNotifyTime clock;
  CHECK(ApplyBroadcastFrame(FrameOf(Encode(clock)), world, BroadcastFilter{}, 0) ==
        FrameOutcome::kUnhandled);
}

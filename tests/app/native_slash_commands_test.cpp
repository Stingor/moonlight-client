// LES COMMANDES « / » QUE LE CLIENT DU JOUEUR CONNAÎT : aucune ne part en
// parole publique, et celle qui se tait le dit.

#include "chat/native_slash_commands.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/chat_target.h"
#include "ro/net/connection.h"
#include "ro/proto/manual_packets.h"

#include "support/loopback_server.h"

using moonclient::NativeKnowsSlashCommand;
using moonclient::NativeSlashCommandNames;
using ro::net::ChatTarget;
using ro::net::ReadChatIntent;
using ro::test::LoopbackServer;

namespace {

// Le premier octet d'une trame porte l'opcode, en petit-boutiste.
constexpr unsigned kOpcodeHighShift = 8;

// LA LIGNE PART-ELLE EN PAROLE PUBLIQUE ? C'est la décision de `SubmitChat` :
// une parole adressée part là où elle dit, une commande connue se tait, et
// seule une ligne en « / » inconnue part à voix haute.
[[nodiscard]] bool LeaksInPublic(std::string_view line) {
  if (ro::net::IsSpokenLine(line)) {
    return ReadChatIntent(line).target == ChatTarget::kPublic;
  }
  return !NativeKnowsSlashCommand(line);
}

struct Wire {
  LoopbackServer server;
  ro::net::Connection link;
  std::vector<std::vector<std::uint8_t>> sent;

  Wire() {
    REQUIRE(server.listening());
    auto opened = ro::net::Connection::Open("127.0.0.1", server.port());
    REQUIRE(opened.has_value());
    REQUIRE(server.Accept());
    link = std::move(*opened);
    link.set_send_observer([this](std::span<const std::uint8_t> frame) {
      sent.emplace_back(frame.begin(), frame.end());
    });
  }
};

[[nodiscard]] std::uint16_t OpcodeOf(const std::vector<std::uint8_t>& frame) {
  return static_cast<std::uint16_t>(frame[0] | (frame[1] << kOpcodeHighShift));
}

}  // namespace

TEST_CASE("Commandes connues : aucun nom ne part en public", "[app][chat][native_commands]") {
  const std::span<const std::string_view> names = NativeSlashCommandNames();
  REQUIRE_FALSE(names.empty());
  for (const std::string_view name : names) {
    INFO(name);
    CHECK(NativeKnowsSlashCommand(name));
    // ⚠ AVEC UN ARGUMENT AUSSI : c'est sous cette forme que « /deal Nom » ou
    // « /gc texte » fuyaient.
    CHECK_FALSE(LeaksInPublic(name));
    CHECK_FALSE(LeaksInPublic(std::string(name) + " Aurélie 42"));
  }
}

TEST_CASE("Commandes connues : rangées dans l'ordre des octets, sans doublon",
          "[app][chat][native_commands]") {
  const std::span<const std::string_view> names = NativeSlashCommandNames();
  CHECK(std::ranges::is_sorted(names));
  CHECK(std::ranges::adjacent_find(names) == names.end());
}

TEST_CASE("Commandes connues : celles qui partaient en public se taisent",
          "[app][chat][native_commands]") {
  for (const std::string_view line :
       {"/deal Bob", "/rjfo Bob", "/ryghks Bob", "/guild Lune", "/rlfem Lune", "/v 50",
        "/bv 50",    "/kill Bob", "/bb salut",   "/mb salut",   "/red rien",   "/b salut",
        "/lb salut", "/nb salut", "/nlb salut",  "/in Bob",     "/am",         "/cn",
        "/hw",       "/o",        "/pk",         "/sp",         "/x",          "/DEAL Bob"}) {
    INFO(line);
    CHECK(NativeKnowsSlashCommand(line));
    CHECK_FALSE(LeaksInPublic(line));
  }
}

TEST_CASE("Commandes inconnues : elles se disent, comme chez le client du joueur",
          "[app][chat][native_commands]") {
  CHECK_FALSE(NativeKnowsSlashCommand("/bonjour"));
  CHECK(LeaksInPublic("/bonjour tout le monde"));
  // Un nom connu n'est reconnu que pour lui-même, pas pour ce qui le prolonge.
  CHECK_FALSE(NativeKnowsSlashCommand("/dealer"));
  CHECK_FALSE(NativeKnowsSlashCommand("/gcx"));
}

TEST_CASE("Commandes du serveur : « @ » et « # » ne changent pas de chemin",
          "[app][chat][native_commands]") {
  CHECK(ro::net::IsSpokenLine("@warp prontera"));
  CHECK(ReadChatIntent("@warp prontera").target == ChatTarget::kPublic);
  CHECK(ReadChatIntent("@warp prontera").text == "@warp prontera");
  CHECK_FALSE(NativeKnowsSlashCommand("@warp prontera"));
  CHECK(ReadChatIntent("#Bob @heal").text == "#Bob @heal");
}

TEST_CASE("Commande tue : une ligne grise dit que rien n'est parti",
          "[app][chat][native_commands]") {
  const std::string notice = moonclient::SilencedCommandNotice("/deal Bob");
  CHECK(notice.ends_with(" /deal Bob"));
  CHECK(notice.starts_with("Commande non prise en charge"));
}

TEST_CASE("Guilde et clan : « /gc » et « /cl » partent là où ils disent",
          "[app][chat][native_commands]") {
  CHECK(ReadChatIntent("/gc bonjour").target == ChatTarget::kGuild);
  CHECK(ReadChatIntent("/gc bonjour").text == "bonjour");
  CHECK(ReadChatIntent("/guildchat  on y va").target == ChatTarget::kGuild);
  CHECK(ReadChatIntent("/guildchat  on y va").text == "on y va");
  CHECK(ReadChatIntent("/GC salut").target == ChatTarget::kGuild);
  CHECK(ReadChatIntent("/cl bonjour").target == ChatTarget::kClan);
  CHECK(ReadChatIntent("/clanchat bonjour").target == ChatTarget::kClan);
  CHECK(ReadChatIntent("/clanchat bonjour").text == "bonjour");
  // Seul, le mot vise sans rien dire : rien ne part.
  CHECK(ReadChatIntent("/gc").target == ChatTarget::kGuild);
  CHECK(ReadChatIntent("/gc").text.empty());
  // La barre armée ailleurs n'y change rien : la ligne a dit où elle va.
  CHECK(ReadChatIntent("/gc bonjour", ro::net::ChatArming{.mode = ChatTarget::kParty}).target ==
        ChatTarget::kGuild);

  // Sur le fil : l'opcode de la guilde, « Moi : texte ».
  Wire wire;
  REQUIRE(ro::net::SendChat(wire.link, "Moi", ReadChatIntent("/gc bonjour")).has_value());
  REQUIRE(wire.sent.size() == 1);
  CHECK(OpcodeOf(wire.sent[0]) == ro::proto::GuildChatRequest::kOpcode);
  const std::string body(
      wire.sent[0].begin() + static_cast<std::ptrdiff_t>(ro::proto::GuildChatRequest::kHeaderSize),
      wire.sent[0].end());
  CHECK(body.starts_with("Moi : bonjour"));
}

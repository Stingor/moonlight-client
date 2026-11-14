// Le type d'une ligne de conversation : la case de filtre où elle se range.

#include <cstddef>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

namespace {

using ro::sim::ChatKind;
using ro::sim::ChatType;
using ro::sim::DefaultChatType;
using ro::sim::World;

}  // namespace

TEST_CASE("Type de conversation : une ligne prend celui de son genre, sauf avis contraire",
          "[sim][chat_type]") {
  // ⚠ LE TYPE EST LA CASE DE FILTRE, LE GENRE L'ALLURE : un refus de
  // compétence est un message du serveur, mais il se range sous « échecs de
  // compétence ».
  World world;
  world.RecordChat(ChatKind::kSelf, 0, "bonjour", 1000);
  world.RecordChat(ChatKind::kServer, 0, "vitesse changee", 1000);
  world.RecordChat(ChatKind::kAnnouncement, 0, "maintenance", 1000);
  world.RecordChat(ChatKind::kServer, ChatType::kSkillFailure, 0, "refusee", 1000);
  REQUIRE(world.chat_lines().size() == 4);
  CHECK(world.chat_lines()[0].type == ChatType::kPublicChat);
  CHECK(world.chat_lines()[1].type == ChatType::kPublic);
  CHECK(world.chat_lines()[2].type == ChatType::kBroadcast);
  CHECK(world.chat_lines()[3].type == ChatType::kSkillFailure);
  CHECK(world.chat_lines()[3].kind == ChatKind::kServer);
}

TEST_CASE("Type de conversation : chaque genre a le sien par défaut", "[sim][chat_type]") {
  STATIC_REQUIRE(DefaultChatType(ChatKind::kPublic) == ChatType::kPublicChat);
  STATIC_REQUIRE(DefaultChatType(ChatKind::kSelf) == ChatType::kPublicChat);
  STATIC_REQUIRE(DefaultChatType(ChatKind::kServer) == ChatType::kPublic);
  STATIC_REQUIRE(DefaultChatType(ChatKind::kChannel) == ChatType::kPublic);
  STATIC_REQUIRE(DefaultChatType(ChatKind::kAnnouncement) == ChatType::kBroadcast);
  STATIC_REQUIRE(DefaultChatType(ChatKind::kWhisper) == ChatType::kWhisper);
  STATIC_REQUIRE(DefaultChatType(ChatKind::kParty) == ChatType::kPartyChat);
  STATIC_REQUIRE(DefaultChatType(ChatKind::kGuild) == ChatType::kGuildChat);
  STATIC_REQUIRE(DefaultChatType(ChatKind::kClan) == ChatType::kClanChat);
  STATIC_REQUIRE(DefaultChatType(ChatKind::kItem) == ChatType::kItemGetDrop);
  STATIC_REQUIRE(DefaultChatType(ChatKind::kBattle) == ChatType::kBattle);
  STATIC_REQUIRE(DefaultChatType(ChatKind::kPartyBattle) == ChatType::kPartyBattle);
  STATIC_REQUIRE(DefaultChatType(ChatKind::kExp) == ChatType::kExperience);
  STATIC_REQUIRE(DefaultChatType(ChatKind::kCall) == ChatType::kCall);
  // Les vingt-cinq cases du client, plus celle des annonces, celle du relai
  // Discord et celle de l'armement et du lancer de ses skills.
  STATIC_REQUIRE(static_cast<std::size_t>(ChatType::kDiscord) == 26);
  STATIC_REQUIRE(static_cast<std::size_t>(ChatType::kSkillUse) == 27);
  STATIC_REQUIRE(static_cast<std::size_t>(ChatType::kSkillUse) + 1 == ro::sim::kChatTypeCount);
}

// Les joueurs en ligne du staff sur le fil : la demande telle qu'elle part, une
// entrée telle que le serveur l'écrit, et l'assemblage des trames d'une réponse.

#include "ro/net/staff_roster.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"

#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::StaffActivity;
using ro::net::StaffRosterAssembly;
using ro::net::StaffRosterClient;
using ro::net::StaffRosterPage;
using ro::net::StaffRosterStatus;
using ro::test::LoopbackServer;
using ro::test_support::ReadU16;

namespace {

constexpr std::uint16_t kRequest = 0x1234;
constexpr std::uint8_t kServed = 0;
constexpr std::uint8_t kDenied = 1;
constexpr std::uint32_t kNever = 0xFFFFFFFFU;

[[nodiscard]] std::size_t RankOf(StaffActivity activity) {
  return static_cast<std::size_t>(activity);
}

// Un chasseur à l'œuvre depuis six heures, sur une machine partagée.
[[nodiscard]] ro::proto::MoonlightStaffRosterEntry Hunter() {
  ro::proto::MoonlightStaffRosterEntry entry;
  entry.account_id = 2000123;
  entry.char_id = 150042;
  // « é » dans la page de codes du jeu.
  entry.name = std::string("Ysold") + static_cast<char>(0xE9);
  entry.guild = "Cendre";
  entry.party = "Veilleurs";
  entry.map = "gl_cas02";
  entry.instance_id = 4;
  entry.x = 88;
  entry.y = 204;
  entry.job = 4012;
  entry.base_level = 96;
  entry.job_level = 64;
  entry.group_level = 0;
  // Assis, absent déclaré, en prison.
  entry.state = 0x0040 | 0x0100 | 0x0200;
  entry.online_s = 22800;
  entry.character_s = 120;
  entry.idle_s = 1;
  entry.walk_age_s = 30;
  entry.fight_age_s = 1;
  entry.item_age_s = 12;
  entry.talk_age_s = 21600;
  entry.gesture_age_s = kNever;
  entry.npc_age_s = kNever;
  entry.command_age_s = 600;
  entry.client = 2;
  entry.client_flags = 0x01;
  entry.patch_index = 41;
  entry.commit = "906828f0f";
  entry.machine_group = 2;
  entry.account_group = 1;
  entry.cheat_clock = 1;
  entry.cheat_cadence = 3;
  entry.cheat_client = 2;
  entry.mute_minutes = 34;
  return entry;
}

[[nodiscard]] std::vector<std::uint8_t> Reply(
    std::uint8_t status, std::vector<ro::proto::MoonlightStaffRosterEntry> entries,
    std::uint8_t page = 0, std::uint8_t pages = 1) {
  ro::proto::ZcMoonlightStaffRoster packet;
  packet.request_id = kRequest;
  packet.status = status;
  packet.page = page;
  packet.pages = pages;
  packet.entries = std::move(entries);
  return ro::proto::Encode(packet);
}

[[nodiscard]] StaffRosterPage PageOf(std::uint8_t page, std::uint8_t pages, std::string name,
                                     std::uint16_t request_id = kRequest) {
  StaffRosterPage built;
  built.request_id = request_id;
  built.status = StaffRosterStatus::kServed;
  built.page = page;
  built.pages = pages;
  built.players.emplace_back().name = std::move(name);
  return built;
}

}  // namespace

TEST_CASE("Joueurs en ligne : la demande porte sa longueur puis son numéro",
          "[net][staff_roster]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  auto link = ro::net::Connection::Open("127.0.0.1", server.port());
  REQUIRE(link.has_value());
  REQUIRE(server.Accept());
  REQUIRE(ro::net::RequestStaffRoster(*link, kRequest).has_value());
  constexpr std::size_t kSize = ro::proto::CzMoonlightReqStaffRoster::kWireSize;
  const std::vector<std::uint8_t> sent = server.Receive(kSize);
  REQUIRE(sent.size() == kSize);
  CHECK(ReadU16(sent, 0) == 0x0F67);
  CHECK(ReadU16(sent, 2) == kSize);
  CHECK(ReadU16(sent, 4) == kRequest);
}

TEST_CASE("Joueurs en ligne : une entrée se lit champ par champ", "[net][staff_roster]") {
  const auto page = ro::net::ReadStaffRoster(ro::test_support::FrameOf(Reply(kServed, {Hunter()})));
  REQUIRE(page.has_value());
  CHECK(page->request_id == kRequest);
  CHECK(page->status == StaffRosterStatus::kServed);
  CHECK(page->page == 0);
  CHECK(page->pages == 1);
  REQUIRE(page->players.size() == 1);
  const ro::net::StaffRosterPlayer& player = page->players.front();
  CHECK(player.account_id == 2000123);
  CHECK(player.char_id == 150042);
  CHECK(player.name == "Ysoldé");
  CHECK(player.guild == "Cendre");
  CHECK(player.party == "Veilleurs");
  CHECK(player.map == "gl_cas02");
  CHECK(player.instance_id == 4);
  CHECK(player.x == 88);
  CHECK(player.y == 204);
  CHECK(player.job == 4012);
  CHECK(player.base_level == 96);
  CHECK(player.job_level == 64);
  CHECK(player.state == ro::net::StaffRosterState{.sitting = true, .away = true, .jailed = true});
  CHECK(player.online_s == 22800);
  CHECK(player.character_s == 120);
  CHECK(player.idle_s == 1);
  CHECK(player.action_age_s[RankOf(StaffActivity::kWalk)] == 30U);
  CHECK(player.action_age_s[RankOf(StaffActivity::kFight)] == 1U);
  CHECK(player.action_age_s[RankOf(StaffActivity::kItem)] == 12U);
  CHECK(player.action_age_s[RankOf(StaffActivity::kTalk)] == 21600U);
  CHECK_FALSE(player.action_age_s[RankOf(StaffActivity::kGesture)].has_value());
  CHECK_FALSE(player.action_age_s[RankOf(StaffActivity::kNpc)].has_value());
  CHECK(player.action_age_s[RankOf(StaffActivity::kCommand)] == 600U);
  CHECK(player.client == StaffRosterClient::kMoonclient);
  CHECK(player.tree_modified);
  CHECK(player.patch_index == 41);
  CHECK(player.commit == "906828f0f");
  CHECK(player.machine_group == 2);
  CHECK(player.account_group == 1);
  CHECK(player.cheat_clock == 1);
  CHECK(player.cheat_cadence == 3);
  CHECK(player.cheat_client == 2);
  CHECK(player.mute_minutes == 34);
}

TEST_CASE("Joueurs en ligne : chaque bit d'état a son drapeau", "[net][staff_roster]") {
  ro::proto::MoonlightStaffRosterEntry entry = Hunter();
  entry.state = 0x0001 | 0x0002 | 0x0004 | 0x0008 | 0x0010 | 0x0020 | 0x0080 | 0x0400 | 0x0800;
  const auto page = ro::net::DecodeStaffRoster(Reply(kServed, {entry}));
  REQUIRE(page.has_value());
  REQUIRE(page->players.size() == 1);
  CHECK(page->players.front().state == ro::net::StaffRosterState{.vending = true,
                                                                 .buying = true,
                                                                 .offline_shop = true,
                                                                 .chat_room = true,
                                                                 .trading = true,
                                                                 .dead = true,
                                                                 .hidden = true,
                                                                 .npc_dialog = true,
                                                                 .storage_open = true});
}

TEST_CASE("Joueurs en ligne : un refus et un statut inconnu ne portent aucune liste",
          "[net][staff_roster]") {
  CHECK(ro::net::DecodeStaffRoster(Reply(kServed, {}))->status == StaffRosterStatus::kServed);
  const auto denied = ro::net::DecodeStaffRoster(Reply(kDenied, {Hunter()}));
  REQUIRE(denied.has_value());
  CHECK(denied->status == StaffRosterStatus::kDenied);
  CHECK(denied->players.empty());
  constexpr std::uint8_t kUnknownStatus = 9;
  const auto unknown = ro::net::DecodeStaffRoster(Reply(kUnknownStatus, {Hunter()}));
  REQUIRE(unknown.has_value());
  CHECK(unknown->status == StaffRosterStatus::kUnknown);
  CHECK(unknown->players.empty());
}

TEST_CASE("Joueurs en ligne : chaque client se nomme, un inconnu reste un autre",
          "[net][staff_roster]") {
  const auto client_of = [](std::uint8_t wire) {
    ro::proto::MoonlightStaffRosterEntry entry = Hunter();
    entry.client = wire;
    return ro::net::DecodeStaffRoster(Reply(kServed, {entry}))->players.front().client;
  };
  CHECK(client_of(0) == StaffRosterClient::kNone);
  CHECK(client_of(1) == StaffRosterClient::kOriginal);
  CHECK(client_of(2) == StaffRosterClient::kMoonclient);
  constexpr std::uint8_t kUnknownClient = 7;
  CHECK(client_of(kUnknownClient) == StaffRosterClient::kOther);
}

TEST_CASE("Joueurs en ligne : une trame qui ne se découpe pas n'est pas une réponse",
          "[net][staff_roster]") {
  std::vector<std::uint8_t> bytes = Reply(kServed, {Hunter()});
  bytes.pop_back();
  CHECK_FALSE(ro::net::DecodeStaffRoster(bytes).has_value());
  CHECK_FALSE(ro::net::ReadStaffRoster(ro::net::Frame{.opcode = 0x0F3F, .bytes = {}}).has_value());
}

TEST_CASE("Assemblage : les trames d'une réponse se réunissent dans l'ordre",
          "[net][staff_roster]") {
  StaffRosterAssembly assembly;
  CHECK_FALSE(assembly.waiting());
  assembly.Expect(kRequest);
  CHECK(assembly.waiting());
  CHECK_FALSE(assembly.Take(PageOf(0, 3, "Aldric")).has_value());
  CHECK_FALSE(assembly.Take(PageOf(1, 3, "Mirelle")).has_value());
  const std::optional<ro::net::StaffRosterAnswer> answer = assembly.Take(PageOf(2, 3, "Tobias"));
  REQUIRE(answer.has_value());
  CHECK(answer->status == StaffRosterStatus::kServed);
  REQUIRE(answer->players.size() == 3);
  CHECK(answer->players[0].name == "Aldric");
  CHECK(answer->players[1].name == "Mirelle");
  CHECK(answer->players[2].name == "Tobias");
  // La réponse rendue, plus rien n'est attendu.
  CHECK_FALSE(assembly.waiting());
  CHECK_FALSE(assembly.Take(PageOf(0, 1, "Corvin")).has_value());
}

TEST_CASE("Assemblage : la trame d'une autre demande est jetée sans rien défaire",
          "[net][staff_roster]") {
  StaffRosterAssembly assembly;
  assembly.Expect(kRequest);
  CHECK_FALSE(assembly.Take(PageOf(0, 2, "Aldric")).has_value());
  constexpr std::uint16_t kOlderRequest = 0x1233;
  CHECK_FALSE(assembly.Take(PageOf(1, 2, "Intrus", kOlderRequest)).has_value());
  const std::optional<ro::net::StaffRosterAnswer> answer = assembly.Take(PageOf(1, 2, "Mirelle"));
  REQUIRE(answer.has_value());
  REQUIRE(answer->players.size() == 2);
  CHECK(answer->players[1].name == "Mirelle");
}

TEST_CASE("Assemblage : une trame hors de son rang jette ce qui était amassé",
          "[net][staff_roster]") {
  StaffRosterAssembly assembly;
  assembly.Expect(kRequest);
  CHECK_FALSE(assembly.Take(PageOf(0, 3, "Aldric")).has_value());
  CHECK_FALSE(assembly.Take(PageOf(2, 3, "Tobias")).has_value());
  CHECK_FALSE(assembly.waiting());
  // La suite ne ressuscite pas une réponse trouée.
  CHECK_FALSE(assembly.Take(PageOf(1, 3, "Mirelle")).has_value());
}

TEST_CASE("Assemblage : une trame sans total, ou d'un autre total, ne vaut rien",
          "[net][staff_roster]") {
  StaffRosterAssembly assembly;
  assembly.Expect(kRequest);
  CHECK_FALSE(assembly.Take(PageOf(0, 0, "Aldric")).has_value());
  CHECK_FALSE(assembly.waiting());

  assembly.Expect(kRequest);
  CHECK_FALSE(assembly.Take(PageOf(0, 3, "Aldric")).has_value());
  CHECK_FALSE(assembly.Take(PageOf(1, 2, "Mirelle")).has_value());
  CHECK_FALSE(assembly.waiting());
}

TEST_CASE("Assemblage : une nouvelle attente oublie la réponse en cours", "[net][staff_roster]") {
  StaffRosterAssembly assembly;
  assembly.Expect(kRequest);
  CHECK_FALSE(assembly.Take(PageOf(0, 2, "Aldric")).has_value());
  constexpr std::uint16_t kNewerRequest = 0x1235;
  assembly.Expect(kNewerRequest);
  const std::optional<ro::net::StaffRosterAnswer> answer =
      assembly.Take(PageOf(0, 1, "Mirelle", kNewerRequest));
  REQUIRE(answer.has_value());
  REQUIRE(answer->players.size() == 1);
  CHECK(answer->players.front().name == "Mirelle");
}

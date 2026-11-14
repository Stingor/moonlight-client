// LE DÉPART ET L'EXPULSION D'UN MEMBRE, écrits dans la conversation.
//
// 🔴 CE BANC PORTE CE QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · le nom se relève dans la liste des membres AVANT que le membre n'en
//     sorte — le serveur ne nomme que le personnage ;
//   · le motif se masque sur sa ligne entière, le nom non ;
//   · notre propre départ n'écrit que son constat, en rouge, et le personnage
//     joué perd sa guilde ;
//   · un expulsé entre dans la liste des expulsions.

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/guild.h"
#include "ro/proto/guild_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/banned_words.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/guild.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ApplyGuildFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::net::SayGuildDepartures;
using ro::sim::BannedWords;
using ro::sim::ChatLine;
using ro::sim::GuildBook;
using ro::sim::World;
using ro::test_support::FrameOf;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

constexpr std::uint32_t kMe = 150001;
constexpr std::uint32_t kOther = 150002;
constexpr std::uint32_t kStranger = 150003;
constexpr std::uint32_t kMyAccount = 2000001;
constexpr std::int32_t kGuildId = 42;
constexpr std::uint32_t kNowMs = 42'000;
constexpr std::uint32_t kYellow = 0xFFFF00;
constexpr std::uint32_t kRed = 0xFF0000;
constexpr std::size_t kReasonWidth = 40;

void PutText(std::vector<std::uint8_t>& into, const std::string& text) {
  for (std::size_t index = 0; index < kReasonWidth; ++index) {
    into.push_back(index < text.size() ? static_cast<std::uint8_t>(text[index]) : 0);
  }
}

[[nodiscard]] GuildBook Belonging() {
  GuildBook guild;
  ro::proto::ZcUpdateGdid belonging;
  belonging.guildId = kGuildId;
  belonging.guildName = "Moonlight";
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(belonging)), guild, kMe) ==
          FrameOutcome::kApplied);
  ro::proto::ZcMembermgrInfo members;
  for (const std::uint32_t id : {kMe, kOther}) {
    ro::proto::GuildMemberInfo member;
    member.AID = id + 1000;
    member.GID = id;
    member.char_name = id == kMe ? "Stingor" : "Bob";
    members.guildMemberInfo.push_back(member);
  }
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(members)), guild, kMe) ==
          FrameOutcome::kApplied);
  return guild;
}

[[nodiscard]] Frame Left(std::uint32_t char_id, const std::string& reason) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::kGuildMemberLeftOpcode);
  PutU32(bytes, char_id);
  PutText(bytes, reason);
  return Frame{.opcode = ro::proto::kGuildMemberLeftOpcode, .bytes = bytes};
}

// ⚠ L'EXPULSION RANGE SES DEUX CHAMPS À L'ENVERS DU DÉPART : le motif d'abord.
[[nodiscard]] Frame Expelled(std::uint32_t char_id, const std::string& reason) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::kGuildMemberExpelledOpcode);
  PutText(bytes, reason);
  PutU32(bytes, char_id);
  return Frame{.opcode = ro::proto::kGuildMemberExpelledOpcode, .bytes = bytes};
}

void CheckNews(const ChatLine& line, const std::string& text) {
  CHECK(line.text == text);
  CHECK(line.color == kYellow);
  CHECK(line.kind == ro::sim::ChatKind::kServer);
  CHECK(line.type == ro::sim::ChatType::kGuildChat);
  CHECK(line.said_ms == kNowMs);
}

}  // namespace

TEST_CASE("Départ de guilde : un autre membre, son nom puis son motif masqué", "[net][guild]") {
  GuildBook guild = Belonging();
  World world;
  const BannedWords banned(std::vector<std::string>{"brocoli"});
  REQUIRE(ApplyGuildFrame(Left(kOther, "trop de brocoli"), guild, kMe) == FrameOutcome::kApplied);
  // LE MEMBRE EST DÉJÀ SORTI de la liste : son nom a été relevé avant.
  CHECK(guild.FindMember(kOther) == nullptr);
  CHECK(world.chat_lines().empty());

  CHECK_FALSE(SayGuildDepartures(guild, world, &banned, kNowMs));
  REQUIRE(world.chat_lines().size() == 2);
  CheckNews(world.chat_lines()[0], "Bob a quitté la guilde.");
  CheckNews(world.chat_lines()[1], "Motif du départ : trop de *******");
  CHECK(guild.in_guild());
  // ⚠ UN DÉPART NE S'ÉCRIT QU'UNE FOIS.
  CHECK_FALSE(SayGuildDepartures(guild, world, &banned, kNowMs));
  CHECK(world.chat_lines().size() == 2);
}

TEST_CASE("Départ de guilde : un expulsé s'écrit et entre aux expulsions", "[net][guild]") {
  GuildBook guild = Belonging();
  World world;
  REQUIRE(ApplyGuildFrame(Expelled(kOther, "trop bavard"), guild, kMe) == FrameOutcome::kApplied);
  REQUIRE(guild.bans().size() == 1);
  CHECK(guild.bans()[0].char_id == static_cast<std::int32_t>(kOther));
  CHECK(guild.bans()[0].name == "Bob");
  CHECK(guild.bans()[0].reason == "trop bavard");

  CHECK_FALSE(SayGuildDepartures(guild, world, nullptr, kNowMs));
  REQUIRE(world.chat_lines().size() == 2);
  CheckNews(world.chat_lines()[0], "Bob a été expulsé de la guilde.");
  CheckNews(world.chat_lines()[1], "Motif de l'expulsion : trop bavard");
}

TEST_CASE("Départ de guilde : un inconnu de la liste est « Sans nom »", "[net][guild]") {
  GuildBook guild = Belonging();
  World world;
  REQUIRE(ApplyGuildFrame(Left(kStranger, ""), guild, kMe) == FrameOutcome::kApplied);
  CHECK_FALSE(SayGuildDepartures(guild, world, nullptr, kNowMs));
  REQUIRE(world.chat_lines().size() == 2);
  CheckNews(world.chat_lines()[0], "Sans nom a quitté la guilde.");
  CheckNews(world.chat_lines()[1], "Motif du départ : ");
}

TEST_CASE("Départ de guilde : le nôtre, le seul constat en rouge, et plus de guilde",
          "[net][guild]") {
  for (const bool expelled : {false, true}) {
    GuildBook guild = Belonging();
    World world;
    world.set_player_id(kMyAccount);
    ro::sim::Actor player;
    player.id = kMyAccount;
    player.guild_id = kGuildId;
    world.Spawn(player);

    const Frame frame = expelled ? Expelled(kMe, "au revoir") : Left(kMe, "au revoir");
    REQUIRE(ApplyGuildFrame(frame, guild, kMe) == FrameOutcome::kApplied);
    CHECK_FALSE(guild.in_guild());
    // ⚠ NOTRE EXPULSION N'ENTRE PAS AUX EXPULSIONS : la liste part avec la guilde.
    CHECK(guild.bans().empty());

    CHECK(SayGuildDepartures(guild, world, nullptr, kNowMs));
    REQUIRE(world.chat_lines().size() == 1);
    const ChatLine& line = world.chat_lines()[0];
    CHECK(line.text ==
          (expelled ? "Vous avez été expulsé de la guilde." : "Vous avez quitté la guilde."));
    CHECK(line.color == kRed);
    CHECK(line.type == ro::sim::ChatType::kGuildChat);
    REQUIRE(world.Find(kMyAccount) != nullptr);
    CHECK(world.Find(kMyAccount)->guild_id == 0);
  }
}

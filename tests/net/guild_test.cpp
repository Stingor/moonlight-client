// LA GUILDE, décodée — et ce qu'on demande, sur le fil.
//
// 🔴 CE BANC PORTE TROIS CHOSES QU'AUCUNE SESSION NE MONTRERAIT VITE :
//
//   · le départ ou l'expulsion d'un membre est rediffusé à TOUTE la guilde :
//     c'est le numéro du personnage qui dit si c'est nous qui partons ;
//   · les noms des postes arrivent à part de la liste des membres, et ils
//     survivent au rafraîchissement suivant ;
//   · la fenêtre se redemande dans un ORDRE, membres d'abord et postes ensuite.

#include "ro/net/guild.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/guild_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/bank.h"
#include "ro/sim/guild.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ApplyGuildFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::GuildBook;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint32_t kMe = 150001;
constexpr std::uint32_t kOther = 150002;

void PutText(std::vector<std::uint8_t>& into, const std::string& text, std::size_t width) {
  for (std::size_t index = 0; index < width; ++index) {
    into.push_back(index < text.size() ? static_cast<std::uint8_t>(text[index]) : 0);
  }
}

[[nodiscard]] GuildBook Belonging() {
  GuildBook guild;
  ro::proto::ZcUpdateGdid packet;
  packet.guildId = 42;
  packet.emblemVersion = 3;
  packet.mode = ro::sim::kGuildMayInvite;
  packet.isMaster = 1;
  packet.guildName = "Moonlight";
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(packet)), guild, kMe) ==
          FrameOutcome::kApplied);
  return guild;
}

[[nodiscard]] Frame Members() {
  ro::proto::ZcMembermgrInfo packet;
  for (const std::uint32_t id : {kMe, kOther}) {
    ro::proto::GuildMemberInfo member;
    member.AID = id + 1000;
    member.GID = id;
    member.char_name = id == kMe ? "Stingor" : "Bob";
    member.positionID = id == kMe ? 0 : 3;
    member.currentState = 1;
    packet.guildMemberInfo.push_back(member);
  }
  return FrameOf(ro::proto::Encode(packet));
}

// ⚠ SANS STRUCTURE CHEZ LE SERVEUR : une entrée de vingt-huit octets par poste.
[[nodiscard]] Frame PositionNames() {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::kGuildPositionNamesOpcode);
  PutU16(bytes, 0);
  PutU32(bytes, 3);
  PutText(bytes, "Handyman", 24);
  bytes[2] = static_cast<std::uint8_t>(bytes.size());
  return Frame{.opcode = ro::proto::kGuildPositionNamesOpcode, .bytes = bytes};
}

[[nodiscard]] Frame MemberLeft(std::uint32_t char_id) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::kGuildMemberLeftOpcode);
  PutU32(bytes, char_id);
  PutText(bytes, "au revoir", 40);
  return Frame{.opcode = ro::proto::kGuildMemberLeftOpcode, .bytes = bytes};
}

// ⚠ L'EXPULSION RANGE SES DEUX CHAMPS À L'ENVERS DU DÉPART : le motif d'abord.
[[nodiscard]] Frame MemberExpelled(std::uint32_t char_id) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, ro::proto::kGuildMemberExpelledOpcode);
  PutText(bytes, "trop bavard", 40);
  PutU32(bytes, char_id);
  return Frame{.opcode = ro::proto::kGuildMemberExpelledOpcode, .bytes = bytes};
}

}  // namespace

TEST_CASE("Guilde : l'appartenance pose la guilde et la version d'emblème", "[net][guild]") {
  const GuildBook guild = Belonging();
  REQUIRE(guild.in_guild());
  CHECK(guild.belonging()->guild_id == 42);
  CHECK(guild.belonging()->master);
  CHECK(guild.belonging()->name == "Moonlight");
  CHECK(guild.EmblemVersionOf(42) == 3);
}

TEST_CASE("Guilde : le départ d'un autre retire un membre, le nôtre vide tout", "[net][guild]") {
  GuildBook guild = Belonging();
  REQUIRE(ApplyGuildFrame(Members(), guild, kMe) == FrameOutcome::kApplied);
  REQUIRE(guild.members().size() == 2);

  REQUIRE(ApplyGuildFrame(MemberLeft(kOther), guild, kMe) == FrameOutcome::kApplied);
  CHECK(guild.in_guild());
  CHECK(guild.members().size() == 1);

  REQUIRE(ApplyGuildFrame(MemberExpelled(kMe), guild, kMe) == FrameOutcome::kApplied);
  CHECK_FALSE(guild.in_guild());
  CHECK(guild.members().empty());
  // ⚠ LA VERSION D'EMBLÈME RESTE : elle vaut pour une guilde qu'on voit encore.
  CHECK(guild.EmblemVersionOf(42) == 3);
}

TEST_CASE("Guilde : les noms de poste survivent à la liste des membres", "[net][guild]") {
  GuildBook guild = Belonging();
  REQUIRE(ApplyGuildFrame(PositionNames(), guild, kMe) == FrameOutcome::kApplied);
  REQUIRE(ApplyGuildFrame(Members(), guild, kMe) == FrameOutcome::kApplied);
  CHECK(guild.PositionLabel(3) == "Handyman");
  const auto* bob = guild.FindMember(kOther);
  REQUIRE(bob != nullptr);
  CHECK(bob->position == 3);
  CHECK(bob->name == "Bob");
}

[[nodiscard]] Frame Header(std::int32_t guild_id, std::uint32_t master_char_id) {
  ro::proto::ZcGuildInfo packet;
  packet.GDID = guild_id;
  packet.emblemVersion = 5;
  packet.masterGID = static_cast<std::int32_t>(master_char_id);
  packet.guildname = "Moonlight";
  packet.masterName = "Bob";
  return FrameOf(ro::proto::Encode(packet));
}

TEST_CASE("Guilde : à l'adhésion, l'en-tête SEUL fait entrer dans la guilde", "[net][guild]") {
  // 🔴 LE SERVEUR N'ENVOIE PAS L'APPARTENANCE À QUI VIENT D'ACCEPTER : son
  // poste n'est pas encore connu quand il accuse l'ajout. L'en-tête de la
  // guilde est le seul témoin, et sans lui l'onglet restait « sans guilde »
  // jusqu'à la reconnexion suivante.
  GuildBook guild;
  REQUIRE(ApplyGuildFrame(Header(42, kOther), guild, kMe) == FrameOutcome::kApplied);
  REQUIRE(guild.in_guild());
  CHECK(guild.belonging()->guild_id == 42);
  CHECK(guild.belonging()->name == "Moonlight");
  CHECK_FALSE(guild.belonging()->master);
  CHECK(guild.EmblemVersionOf(42) == 5);
  CHECK_FALSE(guild.IsMaster(kMe));
}

TEST_CASE("Guilde : l'en-tête ne réécrit pas une appartenance connue", "[net][guild]") {
  // ⚠ IL REVIENT À CHAQUE RAFRAÎCHISSEMENT, et il n'a ni le masque de droits
  // ni le drapeau de maître : réécrire les effacerait toutes les trente
  // secondes.
  GuildBook guild = Belonging();
  REQUIRE(ApplyGuildFrame(Header(42, kOther), guild, kMe) == FrameOutcome::kApplied);
  CHECK(guild.belonging()->master);
  CHECK(guild.belonging()->rights == ro::sim::kGuildMayInvite);
}

TEST_CASE("Guilde : à l'entrée en jeu, l'appartenance qui suit l'en-tête complète sans vider",
          "[net][guild]") {
  // L'ORDRE DU SERVEUR À LA CONNEXION : l'en-tête d'abord, l'appartenance
  // ensuite. La seconde précise la première ; elle ne doit rien effacer.
  GuildBook guild;
  REQUIRE(ApplyGuildFrame(Header(42, kMe), guild, kMe) == FrameOutcome::kApplied);
  REQUIRE(ApplyGuildFrame(Members(), guild, kMe) == FrameOutcome::kApplied);
  ro::proto::ZcUpdateGdid packet;
  packet.guildId = 42;
  packet.mode = ro::sim::kGuildMayExpel;
  packet.isMaster = 1;
  packet.guildName = "Moonlight";
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(packet)), guild, kMe) ==
          FrameOutcome::kApplied);
  CHECK(guild.belonging()->master);
  CHECK(guild.belonging()->rights == ro::sim::kGuildMayExpel);
  CHECK(guild.members().size() == 2);
}

TEST_CASE("Guilde : un en-tête sans guilde ne pose rien, une autre guilde remplace",
          "[net][guild]") {
  GuildBook none;
  REQUIRE(ApplyGuildFrame(Header(0, kOther), none, kMe) == FrameOutcome::kApplied);
  CHECK_FALSE(none.in_guild());

  GuildBook moved = Belonging();
  REQUIRE(ApplyGuildFrame(Members(), moved, kMe) == FrameOutcome::kApplied);
  REQUIRE(ApplyGuildFrame(Header(77, kOther), moved, kMe) == FrameOutcome::kApplied);
  REQUIRE(moved.in_guild());
  CHECK(moved.belonging()->guild_id == 77);
  // Les membres de l'ancienne guilde n'ont rien à faire dans la nouvelle.
  CHECK_FALSE(moved.members_known());
}

template <typename Packet>
[[nodiscard]] Frame Standing(std::uint32_t guild_id, std::int16_t version, std::int16_t job) {
  Packet packet;
  packet.AID = 2'000'001;
  packet.GUID = guild_id;
  packet.GEmblemVer = version;
  packet.job = job;
  packet.name = "Bob";
  return FrameOf(ro::proto::Encode(packet));
}

// ⚠ LES TROIS APPARITIONS : au repos, qui surgit, qui marche. Un joueur qui entre
// en vue en marchant n'apprendrait rien si l'une manquait.
TEMPLATE_TEST_CASE("Guilde : une apparition apprend la version d'emblème de sa guilde",
                   "[net][guild]", ro::proto::PacketIdleUnit, ro::proto::PacketSpawnUnit,
                   ro::proto::PacketUnitWalking) {
  // 🔴 LE SEUL ENDROIT OÙ L'ON APPREND CELLE DES AUTRES GUILDES : sans elle,
  // aucune étiquette ne saurait quel emblème demander.
  GuildBook guild;
  REQUIRE(ApplyGuildFrame(Standing<TestType>(77, 3, 0), guild, kMe) == FrameOutcome::kApplied);
  CHECK(guild.EmblemVersionOf(77) == 3);
  // ⚠ UNE VERSION NULLE D'UN JOUEUR NE L'EFFACE PAS.
  CHECK(ApplyGuildFrame(Standing<TestType>(77, 0, 0), guild, kMe) == FrameOutcome::kIgnored);
  CHECK(guild.EmblemVersionOf(77) == 3);
  // Sans guilde, rien.
  CHECK(ApplyGuildFrame(Standing<TestType>(0, 5, 0), guild, kMe) == FrameOutcome::kIgnored);
}

TEST_CASE("Guilde : les deux drapeaux inscrivent leur version même nulle", "[net][guild]") {
  constexpr std::int16_t kGuildFlagJob = 722;
  constexpr std::int16_t kGuildSkillFlagJob = 20269;
  for (const std::int16_t job : {kGuildFlagJob, kGuildSkillFlagJob}) {
    GuildBook guild;
    guild.TellEmblemVersion(77, 3);
    REQUIRE(ApplyGuildFrame(Standing<ro::proto::PacketIdleUnit>(77, 0, job), guild, kMe) ==
            FrameOutcome::kApplied);
    CHECK(guild.EmblemVersionOf(77) == 0);
  }
}

TEST_CASE("Guilde : la rediffusion d'un emblème change sa version", "[net][guild]") {
  GuildBook guild;
  ro::proto::ZcChangeGuild packet;
  packet.guild_id = 77;
  packet.emblem_id = 9;
  packet.AID = 2000001;
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(packet)), guild, kMe) ==
          FrameOutcome::kApplied);
  CHECK(guild.EmblemVersionOf(77) == 9);
  CHECK_FALSE(guild.in_guild());
}

TEST_CASE("Guilde : une invitation reçue attend sa réponse", "[net][guild]") {
  GuildBook guild;
  ro::proto::ZcReqJoinGuild packet;
  packet.guild_id = 5;
  packet.guild_name = "Les Porings";
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(packet)), guild, kMe) ==
          FrameOutcome::kApplied);
  REQUIRE(guild.invitation().has_value());
  CHECK(guild.invitation()->guild_name == "Les Porings");
}

TEST_CASE("Guilde : une trame étrangère n'est pas à elle", "[net][guild]") {
  GuildBook guild;
  const Frame other{.opcode = 0x0001, .bytes = {0x01, 0x00}};
  CHECK(ApplyGuildFrame(other, guild, kMe) == FrameOutcome::kUnhandled);
  Frame cut = MemberLeft(kOther);
  cut.bytes.pop_back();
  CHECK(ApplyGuildFrame(cut, guild, kMe) == FrameOutcome::kUndecodable);
}

TEST_CASE("Guilde : la fenêtre se redemande membres d'abord, postes ensuite", "[net][guild]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::RequestGuildWindow(link).has_value());
  constexpr std::size_t kSentBytes =
      ro::proto::GuildMenuRequest::kWireSize * 5 + ro::proto::GuildMenuInterfaceRequest::kWireSize;
  const std::vector<std::uint8_t> sent = server.Receive(kSentBytes);
  REQUIRE(sent.size() == kSentBytes);
  const std::vector<std::uint32_t> parts = {ReadU32(sent, 2), ReadU32(sent, 8), ReadU32(sent, 14),
                                            ReadU32(sent, 20), ReadU32(sent, 26)};
  CHECK(parts == std::vector<std::uint32_t>{1, 2, 3, 4, 0});
  // PUIS LES ONGLETS : le serveur ne les pousse qu'au maître.
  CHECK((sent[30] | (sent[31] << 8)) == ro::proto::GuildMenuInterfaceRequest::kOpcode);
}

TEST_CASE("Guilde : les onglets que le serveur ouvre se retiennent", "[net][guild]") {
  GuildBook guild = Belonging();
  // Rien de dit : tout s'ouvre.
  CHECK_FALSE(guild.menu_flags().has_value());
  CHECK(guild.MenuOpens(ro::sim::kGuildMenuNotice));

  // Un simple membre : tout sauf l'annonce.
  ro::proto::ZcAckGuildMenuinterface member;
  member.menuFlag = 0x57;
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(member)), guild, kMe) ==
          FrameOutcome::kApplied);
  CHECK(guild.MenuOpens(ro::sim::kGuildMenuMembers));
  CHECK(guild.MenuOpens(ro::sim::kGuildMenuPositions));
  CHECK(guild.MenuOpens(ro::sim::kGuildMenuBans));
  CHECK_FALSE(guild.MenuOpens(ro::sim::kGuildMenuNotice));

  // Le maître : tout.
  ro::proto::ZcAckGuildMenuinterface master;
  master.menuFlag = 0xD7;
  REQUIRE(ApplyGuildFrame(FrameOf(ro::proto::Encode(master)), guild, kMe) ==
          FrameOutcome::kApplied);
  CHECK(guild.MenuOpens(ro::sim::kGuildMenuNotice));

  // Quitter la guilde les oublie.
  guild.Leave();
  CHECK_FALSE(guild.menu_flags().has_value());
}

TEST_CASE("Guilde : un poste part sur quarante octets, droits bornés", "[net][guild]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  const std::vector<ro::net::GuildPositionEdit> edits = {
      {.position = 2, .rights = 0xFFFF, .pay_rate = 90, .name = "Garde"}};
  REQUIRE(ro::net::RequestGuildPositions(link, edits).has_value());
  const std::vector<std::uint8_t> sent = server.Receive(44);
  REQUIRE(sent.size() == 44);
  CHECK((sent[0] | (sent[1] << 8)) == ro::proto::GuildPositionsRequest::kOpcode);
  CHECK((sent[2] | (sent[3] << 8)) == 44);
  CHECK(ReadU32(sent, 4) == 2);
  CHECK(ReadU32(sent, 8) == static_cast<std::uint32_t>(ro::sim::kGuildRightsMask));
  CHECK(ReadU32(sent, 16) == static_cast<std::uint32_t>(ro::sim::kGuildPayRateMost));
  CHECK(sent[20] == 'G');
}

TEST_CASE("Guilde : la dissolution nomme la guilde, en 1252", "[net][guild]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::RequestGuildDisband(link, "Étoile").has_value());
  const std::vector<std::uint8_t> sent =
      server.Receive(ro::proto::CzReqDisorganizeGuild::kWireSize);
  REQUIRE(sent.size() == ro::proto::CzReqDisorganizeGuild::kWireSize);
  CHECK((sent[0] | (sent[1] << 8)) == ro::proto::CzReqDisorganizeGuild::kOpcode);
  // Le nom part dans l'encodage de la base, que le serveur compare.
  CHECK(sent[2] == 0xC9);
  CHECK(sent[3] == 't');
  CHECK(sent[8] == 0);
}

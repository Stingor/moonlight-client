// LE GROUPE DU JOUEUR, tel que le serveur le décrit : qui en est, et qui est
// connecté. Les lignes de combat de la conversation en dépendent.

#include "ro/net/party.h"

#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/social_text.h"
#include "ro/net/target_info.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/social_packets.h"
#include "ro/sim/party_roster.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"
#include "support/wire_bytes.h"

using ro::net::ApplyPartyFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::PartyRoster;
using ro::test::Connect;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint32_t kSelf = 2'000'001;
constexpr std::uint32_t kFriend = 2'000'002;
constexpr std::uint32_t kSleeper = 2'000'003;

Frame ListOf() {
  ro::proto::ZcGroupList packet;
  packet.partyName = "Groupe";
  ro::proto::ZcGroupListSub self;
  self.AID = kSelf;
  self.playerName = "Moi";
  ro::proto::ZcGroupListSub mate;
  mate.AID = kFriend;
  mate.playerName = "Ami";
  ro::proto::ZcGroupListSub asleep;
  asleep.AID = kSleeper;
  asleep.playerName = "Dormeur";
  asleep.offline = 1;
  packet.members = {self, mate, asleep};
  return Frame{.opcode = ro::proto::ZcGroupList::kOpcode, .bytes = ro::proto::Encode(packet)};
}

Frame Leaving(std::uint32_t account_id) {
  ro::proto::ZcDeleteMemberFromGroup packet;
  packet.AID = static_cast<std::int32_t>(account_id);
  return Frame{.opcode = ro::proto::ZcDeleteMemberFromGroup::kOpcode,
               .bytes = ro::proto::Encode(packet)};
}

}  // namespace

TEST_CASE("Groupe : la liste se pose, un membre déconnecté n'y compte pas", "[net][party]") {
  PartyRoster party;
  REQUIRE(ApplyPartyFrame(ListOf(), party, kSelf) == FrameOutcome::kApplied);
  CHECK(party.members().size() == 3);
  CHECK(party.HasOnlineMember(kFriend));
  CHECK_FALSE(party.HasOnlineMember(kSleeper));
  REQUIRE(party.Find(kFriend) != nullptr);
  CHECK(party.Find(kFriend)->name == "Ami");

  // Un membre qui se reconnecte est redécrit, et compte.
  ro::proto::ZcAddMemberToGroup back;
  back.AID = kSleeper;
  back.playerName = "Dormeur";
  back.offline = 0;
  CHECK(ApplyPartyFrame(Frame{.opcode = ro::proto::ZcAddMemberToGroup::kOpcode,
                              .bytes = ro::proto::Encode(back)},
                        party, kSelf) == FrameOutcome::kApplied);
  CHECK(party.HasOnlineMember(kSleeper));
  CHECK(party.members().size() == 3);
}

TEST_CASE("Groupe : un départ retire le membre, le nôtre vide la liste", "[net][party]") {
  PartyRoster party;
  REQUIRE(ApplyPartyFrame(ListOf(), party, kSelf) == FrameOutcome::kApplied);
  CHECK(ApplyPartyFrame(Leaving(kFriend), party, kSelf) == FrameOutcome::kApplied);
  CHECK_FALSE(party.HasOnlineMember(kFriend));
  CHECK(party.members().size() == 2);
  CHECK(ApplyPartyFrame(Leaving(kSelf), party, kSelf) == FrameOutcome::kApplied);
  CHECK(party.members().empty());
  // Ce que le traducteur ne connaît pas, il le laisse.
  CHECK(ApplyPartyFrame(Frame{.opcode = 0x0001, .bytes = {}}, party, kSelf) ==
        FrameOutcome::kUnhandled);
}

namespace {

using ro::test_support::FrameOf;

constexpr std::uint32_t kNow = 10'000;

// Un groupe dont le joueur est le chef, à Prontera avec son ami.
PartyRoster Led() {
  ro::proto::ZcGroupList packet;
  packet.partyName = "Les Porings";
  ro::proto::ZcGroupListSub self;
  self.AID = kSelf;
  self.GID = 150001;
  self.playerName = "Moi";
  self.mapName = "prontera.gat";
  self.leader = 0;
  self.class_ = 4008;
  self.baseLevel = 99;
  ro::proto::ZcGroupListSub mate;
  mate.AID = kFriend;
  mate.GID = 150002;
  mate.playerName = "Ami";
  mate.mapName = "prontera.gat";
  mate.leader = 1;
  packet.members = {self, mate};
  PartyRoster party;
  REQUIRE(ApplyPartyFrame(FrameOf(ro::proto::Encode(packet)), party, kSelf, kNow) ==
          FrameOutcome::kApplied);
  return party;
}

Frame TargetAnswer(std::uint32_t gid, std::uint8_t status, std::uint8_t known) {
  ro::proto::ZcMoonlightTargetInfo packet;
  packet.gid = gid;
  packet.status = status;
  packet.known = known;
  packet.hp = 400;
  packet.maxhp = 800;
  packet.sp = 70;
  packet.maxsp = 140;
  return FrameOf(ro::proto::Encode(packet));
}

}  // namespace

TEST_CASE("Groupe : la liste porte le nom, le chef, la carte, le job et le niveau",
          "[net][party]") {
  const PartyRoster party = Led();
  CHECK(party.name() == "Les Porings");
  CHECK(party.AmILeader(kSelf));
  CHECK_FALSE(party.IsLeader(kFriend));
  const ro::sim::PartyMember* self = party.Find(kSelf);
  REQUIRE(self != nullptr);
  CHECK(self->map == "prontera.gat");
  CHECK(self->job == 4008);
  CHECK(self->base_level == 99);
  CHECK(self->char_id == 150001);
  CHECK(party.FindByName("Ami") != nullptr);
}

TEST_CASE("Groupe : la vie est inconnue jusqu'à ce qu'on la dise", "[net][party]") {
  PartyRoster party = Led();
  CHECK_FALSE(party.Find(kFriend)->health.has_value());
  ro::proto::ZcNotifyHpToGroupm health;
  health.AID = kFriend;
  health.hp = 0;
  health.maxhp = 900;
  CHECK(ApplyPartyFrame(FrameOf(ro::proto::Encode(health)), party, kSelf, kNow) ==
        FrameOutcome::kApplied);
  REQUIRE(party.Find(kFriend)->health.has_value());
  CHECK(party.Find(kFriend)->health->current == 0);
  CHECK(party.Find(kFriend)->health->max == 900);

  ro::proto::ZcNotifyPositionToGroupm moved;
  moved.AID = static_cast<std::int32_t>(kFriend);
  moved.xPos = 150;
  moved.yPos = 180;
  CHECK(ApplyPartyFrame(FrameOf(ro::proto::Encode(moved)), party, kSelf, kNow) ==
        FrameOutcome::kApplied);
  REQUIRE(party.Find(kFriend)->position.has_value());
  CHECK(party.Find(kFriend)->position->x == 150);
  moved.xPos = -1;
  moved.yPos = -1;
  CHECK(ApplyPartyFrame(FrameOf(ro::proto::Encode(moved)), party, kSelf, kNow) ==
        FrameOutcome::kApplied);
  CHECK_FALSE(party.Find(kFriend)->position.has_value());
}

TEST_CASE("Groupe : le PS se lit sur la réponse, s'oublie sur un refus", "[net][party]") {
  PartyRoster party = Led();
  const Frame answer = TargetAnswer(kFriend, ro::net::kTargetAnswerOk,
                                    ro::net::kTargetKnowsMagic | ro::net::kTargetKnowsHealth);
  // Sans horloge, la réponse n'est pas au groupe.
  CHECK(ApplyPartyFrame(answer, party, kSelf) == FrameOutcome::kUnhandled);
  CHECK(ApplyPartyFrame(answer, party, kSelf, kNow) == FrameOutcome::kApplied);
  REQUIRE(party.FreshMagic(kFriend, kNow).has_value());
  CHECK(party.FreshMagic(kFriend, kNow)->current == 70);
  CHECK(party.Find(kFriend)->health->max == 800);
  CHECK_FALSE(party.FreshMagic(kFriend, kNow + ro::sim::kPartyMagicStaleMs + 1).has_value());

  const Frame out_of_sight = TargetAnswer(kFriend, 1, 0);
  CHECK(ApplyPartyFrame(out_of_sight, party, kSelf, kNow) == FrameOutcome::kApplied);
  CHECK_FALSE(party.Find(kFriend)->magic.has_value());
  // Une entité hors du groupe n'est pas à lui.
  CHECK(ApplyPartyFrame(TargetAnswer(42, 0, ro::net::kTargetKnowsMagic), party, kSelf, kNow) ==
        FrameOutcome::kUnhandled);
}

TEST_CASE("Groupe : des PV en pourcentage ne deviennent pas une vie", "[net][party]") {
  PartyRoster party = Led();
  const Frame answer = TargetAnswer(kFriend, ro::net::kTargetAnswerOk,
                                    ro::net::kTargetKnowsMagic | ro::net::kTargetKnowsHealth |
                                        ro::net::kTargetKnowsHealthPercent);
  CHECK(ApplyPartyFrame(answer, party, kSelf, kNow) == FrameOutcome::kApplied);
  CHECK(party.FreshMagic(kFriend, kNow).has_value());
  CHECK_FALSE(party.Find(kFriend)->health.has_value());
}

TEST_CASE("Groupe : le chef change, et le nouveau est nommé", "[net][party]") {
  PartyRoster party = Led();
  ro::proto::PacketPartyLeaderChanged packet;
  packet.prev_leader_aid = kSelf;
  packet.new_leader_aid = kFriend;
  CHECK(ApplyPartyFrame(FrameOf(ro::proto::Encode(packet)), party, kSelf, kNow) ==
        FrameOutcome::kApplied);
  CHECK(party.IsLeader(kFriend));
  CHECK_FALSE(party.AmILeader(kSelf));
  const std::vector<ro::sim::PartyNotice> notices = party.TakeNotices();
  REQUIRE(notices.size() == 1);
  CHECK(ro::net::PartyNoticeLine(notices[0]).text == "Le chef de groupe est [Ami].");
}

TEST_CASE("Groupe : un refus de départ ne retire personne", "[net][party]") {
  PartyRoster party = Led();
  ro::proto::ZcDeleteMemberFromGroup refused;
  refused.AID = static_cast<std::int32_t>(kSelf);
  refused.result = 2;
  CHECK(ApplyPartyFrame(FrameOf(ro::proto::Encode(refused)), party, kSelf, kNow) ==
        FrameOutcome::kApplied);
  CHECK(party.members().size() == 2);
  std::vector<ro::sim::PartyNotice> notices = party.TakeNotices();
  REQUIRE(notices.size() == 1);
  CHECK(notices[0].kind == ro::sim::PartyNoticeKind::kLeaveRefused);

  ro::proto::ZcDeleteMemberFromGroup expelled;
  expelled.AID = static_cast<std::int32_t>(kSelf);
  expelled.result = 1;
  CHECK(ApplyPartyFrame(FrameOf(ro::proto::Encode(expelled)), party, kSelf, kNow) ==
        FrameOutcome::kApplied);
  CHECK_FALSE(party.in_party());
  CHECK(party.name().empty());
  notices = party.TakeNotices();
  REQUIRE(notices.size() == 1);
  CHECK(ro::net::PartyNoticeLine(notices[0]).text == "Vous avez quitté le groupe.");
}

TEST_CASE("Groupe : les règles s'annoncent au changement, et un refus garde l'EXP",
          "[net][party]") {
  PartyRoster party = Led();
  const std::vector<std::uint8_t> each = {0xD8, 0x07, 0, 0, 0, 0, 0, 0};
  CHECK(ApplyPartyFrame(FrameOf(each), party, kSelf, kNow) == FrameOutcome::kApplied);
  CHECK(party.TakeNotices().empty());
  const std::vector<std::uint8_t> shared = {0xD8, 0x07, 1, 0, 0, 0, 1, 1};
  CHECK(ApplyPartyFrame(FrameOf(shared), party, kSelf, kNow) == FrameOutcome::kApplied);
  std::vector<ro::sim::PartyNotice> notices = party.TakeNotices();
  REQUIRE(notices.size() == 1);
  CHECK(ro::net::PartyNoticeLine(notices[0]).text ==
        "Groupe - Expérience : Partage égal | Ramassage : Partage de groupe | Partage : Partagé");

  const std::vector<std::uint8_t> refused = {0xD8, 0x07, 2, 0, 0, 0, 0, 0};
  CHECK(ApplyPartyFrame(FrameOf(refused), party, kSelf, kNow) == FrameOutcome::kApplied);
  CHECK(party.options()->exp_shared);
  CHECK_FALSE(party.options()->loot_shared);
  notices = party.TakeNotices();
  REQUIRE(notices.size() == 1);
  CHECK(notices[0].kind == ro::sim::PartyNoticeKind::kExpShareRefused);
}

TEST_CASE("Groupe : invitation reçue, réponse de l'invité, création", "[net][party]") {
  PartyRoster party;
  ro::proto::ZcPartyJoinReq invited;
  invited.GRID = 77;
  invited.groupName = "Chasseurs";
  CHECK(ApplyPartyFrame(FrameOf(ro::proto::Encode(invited)), party, kSelf, kNow) ==
        FrameOutcome::kApplied);
  REQUIRE(party.invitation().has_value());
  CHECK(party.invitation()->party_id == 77);
  CHECK(party.invitation()->party_name == "Chasseurs");

  ro::proto::ZcPartyJoinReqAck answer;
  answer.characterName = "Bob";
  answer.result = 1;
  CHECK(ApplyPartyFrame(FrameOf(ro::proto::Encode(answer)), party, kSelf, kNow) ==
        FrameOutcome::kApplied);
  answer.result = 6;
  CHECK(ApplyPartyFrame(FrameOf(ro::proto::Encode(answer)), party, kSelf, kNow) ==
        FrameOutcome::kIgnored);
  ro::proto::ZcAckMakeGroup made;
  made.result = 1;
  CHECK(ApplyPartyFrame(FrameOf(ro::proto::Encode(made)), party, kSelf, kNow) ==
        FrameOutcome::kApplied);
  const std::vector<ro::sim::PartyNotice> notices = party.TakeNotices();
  REQUIRE(notices.size() == 2);
  const ro::net::SocialLine refused = ro::net::PartyNoticeLine(notices[0]);
  CHECK(refused.text == "Bob a refusé votre invitation de groupe.");
  CHECK(refused.tone == ro::net::SocialTone::kRefusal);
  CHECK(ro::net::PartyNoticeLine(notices[1]).text == "Ce nom de groupe existe déjà.");
}

TEST_CASE("Groupe : l'état du partage et le refus des invitations", "[net][party]") {
  PartyRoster party = Led();
  std::vector<std::uint8_t> share;
  PutU16(share, 0x0F35);
  PutU16(share, 13);
  PutU16(share, 15);
  PutU16(share, 1);
  PutU32(share, kFriend);
  share.push_back(ro::sim::kShareBusy);
  CHECK(ApplyPartyFrame(FrameOf(share), party, kSelf, kNow) == FrameOutcome::kApplied);
  CHECK(party.share_idle_s() == 15);
  CHECK(party.ShareFlagsFor(kFriend, kSelf) == ro::sim::kShareBusy);

  ro::proto::ZcPartyConfig config;
  config.denyPartyInvites = 1;
  CHECK(ApplyPartyFrame(FrameOf(ro::proto::Encode(config)), party, kSelf, kNow) ==
        FrameOutcome::kApplied);
  CHECK(party.refuses_invitations() == true);
}

TEST_CASE("Groupe : chaque écho du réglage d'invitation se dit", "[net][party]") {
  ro::sim::PartyRoster party;
  ro::proto::ZcPartyConfig config;
  config.denyPartyInvites = 1;
  REQUIRE(ApplyPartyFrame(FrameOf(ro::proto::Encode(config)), party, kSelf, kNow) ==
          FrameOutcome::kApplied);
  config.denyPartyInvites = 0;
  REQUIRE(ApplyPartyFrame(FrameOf(ro::proto::Encode(config)), party, kSelf, kNow) ==
          FrameOutcome::kApplied);
  const std::vector<ro::sim::PartyNotice> notices = party.TakeNotices();
  REQUIRE(notices.size() == 2);
  CHECK(notices[0].kind == ro::sim::PartyNoticeKind::kOwnInvitationsRefused);
  CHECK(notices[1].kind == ro::sim::PartyNoticeKind::kOwnInvitationsAllowed);
  CHECK_FALSE(ro::net::PartyNoticeLine(notices[0]).text.empty());
  CHECK(party.refuses_invitations() == false);
}

TEST_CASE("Groupe : chaque demande part aux octets près", "[net][party]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);

  REQUIRE(ro::net::RequestPartyCreation(link, "Équipe", true, false).has_value());
  std::vector<std::uint8_t> sent = server.Receive(28);
  REQUIRE(sent.size() == 28);
  CHECK(sent[0] == 0xE8);
  CHECK(sent[1] == 0x01);
  // Le nom part en 1252.
  CHECK(sent[2] == 0xC9);
  CHECK(sent[3] == 'q');
  CHECK(sent[26] == 1);
  CHECK(sent[27] == 0);

  REQUIRE(ro::net::RequestPartyInvite(link, "Bob").has_value());
  sent = server.Receive(26);
  REQUIRE(sent.size() == 26);
  CHECK(sent[0] == 0xC4);
  CHECK(sent[1] == 0x02);
  CHECK(sent[2] == 'B');

  REQUIRE(ro::net::AnswerPartyInvitation(link, 77, true).has_value());
  sent = server.Receive(7);
  REQUIRE(sent.size() == 7);
  CHECK(sent[0] == 0xC7);
  CHECK(ReadU32(sent, 2) == 77);
  CHECK(sent[6] == 1);

  REQUIRE(ro::net::RequestPartyLeave(link).has_value());
  sent = server.Receive(2);
  CHECK(sent == std::vector<std::uint8_t>{0x00, 0x01});

  REQUIRE(ro::net::RequestPartyExpel(link, kFriend, "Ami").has_value());
  sent = server.Receive(30);
  REQUIRE(sent.size() == 30);
  CHECK(sent[0] == 0x03);
  CHECK(sent[1] == 0x01);
  CHECK(ReadU32(sent, 2) == kFriend);
  CHECK(sent[6] == 'A');

  REQUIRE(ro::net::RequestPartyLeader(link, kFriend).has_value());
  sent = server.Receive(6);
  REQUIRE(sent.size() == 6);
  CHECK(sent[0] == 0xDA);
  CHECK(sent[1] == 0x07);
  CHECK(ReadU32(sent, 2) == kFriend);

  REQUIRE(ro::net::RequestPartyOptions(
              link, ro::sim::PartyOptions{.exp_shared = true, .loot_shared = true})
              .has_value());
  sent = server.Receive(8);
  CHECK(sent == std::vector<std::uint8_t>{0xD7, 0x07, 1, 0, 0, 0, 0, 1});

  REQUIRE(ro::net::RequestPartyInviteRefusal(link, true).has_value());
  sent = server.Receive(3);
  CHECK(sent == std::vector<std::uint8_t>{0xC8, 0x02, 1});
}

// LES CLASSEMENTS ET LES AVIS D'ÉTOILE : ce qui se lit du fil, ce qui s'écrit
// dans la conversation, et le classement qui attend ses noms.

#include "world/rank_notices.h"

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/net/rank_notices.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

#include "chat/rank_command.h"
#include "support/loopback_link.h"
#include "support/loopback_server.h"

using moonclient::RankingLines;
using moonclient::RankNoticeLine;
using moonclient::RankNotices;
using moonclient::StarNoticeOutcomeOf;
using ro::net::RankKind;
using ro::net::StarNoticeCode;
using ro::test::Connect;

namespace {

constexpr std::uint32_t kPlayerId = 150'001;
constexpr std::uint32_t kNowMs = 10'000;
constexpr std::uint32_t kPoring = 1002;
constexpr std::uint8_t kHalfDone = 50;
constexpr std::uint32_t kStarNoticeEffect = 543;
constexpr std::size_t kStarNoticeSize = 32;
constexpr std::size_t kSubjectOffset = 26;
constexpr std::size_t kStarOffset = 30;
constexpr std::size_t kCodeOffset = 31;
constexpr std::size_t kNameRequestBytes = 6;
constexpr int kSilenceMs = 150;
constexpr std::uint32_t kAnaId = 77;
constexpr std::uint32_t kBobId = 78;
constexpr std::uint32_t kOneSecondMs = 1000;
// L'en-tête, dix rangs, un trait, ses points, un trait.
constexpr std::size_t kRankingLineCount = 14;

// Un avis d'étoile tel que `clif_starskill` l'écrit.
ro::net::Frame StarNoticeFrame(std::string_view name, std::uint32_t subject_id, std::uint8_t star,
                               std::uint8_t code) {
  ro::net::Frame frame;
  frame.opcode = 0x020E;
  frame.bytes.assign(kStarNoticeSize, 0);
  frame.bytes[0] = 0x0E;
  frame.bytes[1] = 0x02;
  for (std::size_t letter = 0; letter < name.size(); ++letter) {
    frame.bytes[2 + letter] = static_cast<std::uint8_t>(name[letter]);
  }
  for (std::size_t octet = 0; octet < sizeof(subject_id); ++octet) {
    frame.bytes[kSubjectOffset + octet] = static_cast<std::uint8_t>(subject_id >> (8 * octet));
  }
  frame.bytes[kStarOffset] = star;
  frame.bytes[kCodeOffset] = code;
  return frame;
}

ro::sim::World WorldWithPlayer() {
  ro::sim::World world;
  ro::sim::Actor player;
  player.id = kPlayerId;
  player.kind = ro::sim::ActorKind::kPlayer;
  player.name = "Sting";
  world.Spawn(player);
  world.set_player_id(kPlayerId);
  return world;
}

ro::net::Frame RankingFrame(std::array<std::uint32_t, ro::net::kRankingSize> ids) {
  ro::proto::ZcAckRanking2 packet;
  packet.type = static_cast<std::uint16_t>(RankKind::kTaekwon);
  packet.CIDs = ids;
  packet.points[0] = 120;
  packet.mypoints = 7;
  return ro::net::Frame{.opcode = ro::proto::ZcAckRanking2::kOpcode,
                        .bytes = ro::proto::Encode(packet)};
}

ro::net::Frame NameFrame(std::uint32_t character_id, std::string name) {
  ro::proto::ZcAckReqnameBygid packet;
  packet.CID = character_id;
  packet.name = std::move(name);
  return ro::net::Frame{.opcode = ro::proto::ZcAckReqnameBygid::kOpcode,
                        .bytes = ro::proto::Encode(packet)};
}

bool ChatSays(const ro::sim::World& world, std::string_view part) {
  for (const ro::sim::ChatLine& line : world.chat_lines()) {
    if (line.text.find(part) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("un avis d'étoile se lit champ par champ", "[app][rank]") {
  const std::optional<ro::net::StarNotice> notice =
      ro::net::ReadStarNotice(StarNoticeFrame("Poring", kPoring, kHalfDone, 20));
  REQUIRE(notice.has_value());
  CHECK(notice->name == "Poring");
  CHECK(notice->subject_id == kPoring);
  CHECK(notice->star == kHalfDone);
  CHECK(notice->code == StarNoticeCode::kTaekwonMission);
}

TEST_CASE("la cible d'une mission Taekwon est un lien vers sa fiche", "[app][rank]") {
  const ro::net::StarNotice notice{.name = "Poring",
                                   .subject_id = kPoring,
                                   .star = kHalfDone,
                                   .code = StarNoticeCode::kTaekwonMission};
  const auto outcome = StarNoticeOutcomeOf(notice, "Sting");
  REQUIRE(outcome.has_value());
  CHECK(outcome->line.text.find("<MOBL>1002:0:Poring</MOBL>") != std::string::npos);
  CHECK(outcome->line.text.find("50 / 100") != std::string::npos);
  CHECK(outcome->effect_id == kStarNoticeEffect);
  CHECK(outcome->sound.empty());
}

TEST_CASE("le rappel d'un lieu de Feeling nomme le joueur et sonne", "[app][rank]") {
  const ro::net::StarNotice notice{
      .name = "prontera", .subject_id = 0, .star = 1, .code = StarNoticeCode::kPlaceRecalled};
  const auto outcome = StarNoticeOutcomeOf(notice, "Sting");
  REQUIRE(outcome.has_value());
  CHECK(outcome->line.text.find("Luna Space") != std::string::npos);
  CHECK(outcome->line.text.find("Sting") < outcome->line.text.find("prontera"));
  CHECK(outcome->effect_id == kStarNoticeEffect);
  CHECK_FALSE(outcome->sound.empty());
  // Une étoile hors des trois n'a pas de phrase.
  CHECK_FALSE(StarNoticeOutcomeOf(
                  ro::net::StarNotice{.star = 3, .code = StarNoticeCode::kPlaceRecalled}, "Sting")
                  .has_value());
}

TEST_CASE("un classement écrit ses dix rangs, un inconnu sans nom", "[app][rank]") {
  ro::net::RankingList list;
  list.kind = RankKind::kTaekwon;
  list.character_ids[0] = kAnaId;
  list.points[0] = 120;
  list.own_points = 7;
  std::array<std::string, ro::net::kRankingSize> names;
  names[0] = "Ana";
  const std::vector<RankNoticeLine> lines = RankingLines(list, names);
  REQUIRE(lines.size() == kRankingLineCount);
  CHECK(lines[0].text.find("TAEKWON RANK") != std::string::npos);
  CHECK(lines[1].text == "[  1 ] Ana  :   120 POINT");
  CHECK(lines[2].text == "[  2 ] Sans nom  :     0 POINT");
  CHECK(lines[12].text == "MY POINT    :     7 POINT ");
  CHECK(lines[0].color != lines[1].color);
}

TEST_CASE("le classement attend ses noms, puis s'écrit", "[app][rank]") {
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  ro::sim::World world = WorldWithPlayer();
  RankNotices notices;

  std::array<std::uint32_t, ro::net::kRankingSize> ids = {};
  ids[0] = kAnaId;
  ids[1] = kBobId;
  CHECK(notices.ApplyFrame(RankingFrame(ids), world, link, kNowMs) ==
        ro::net::FrameOutcome::kApplied);
  // Deux noms demandés, un par personnage.
  CHECK(server.Receive(kNameRequestBytes).size() == kNameRequestBytes);
  CHECK(server.Receive(kNameRequestBytes).size() == kNameRequestBytes);
  CHECK(server.Receive(kNameRequestBytes, kSilenceMs).empty());
  CHECK_FALSE(ChatSays(world, "TAEKWON RANK"));

  // Un seul nom au bout d'une seconde : le client dit qu'il cherche.
  static_cast<void>(notices.ApplyFrame(NameFrame(kAnaId, "Ana"), world, link, kNowMs));
  notices.Advance(world, kNowMs + kOneSecondMs);
  CHECK_FALSE(ChatSays(world, "TAEKWON RANK"));
  CHECK(ChatSays(world, "Récupération du classement"));

  // Le second arrive : le classement s'écrit à la seconde suivante.
  static_cast<void>(notices.ApplyFrame(NameFrame(kBobId, "Bob"), world, link, kNowMs));
  notices.Advance(world, kNowMs + 2 * kOneSecondMs);
  CHECK(ChatSays(world, "TAEKWON RANK"));
  CHECK(ChatSays(world, "] Bob  :"));
}

TEST_CASE("un avis d'étoile pose son effet sur le joueur", "[app][rank]") {
  ro::sim::World world = WorldWithPlayer();
  ro::net::Connection link;
  RankNotices notices;
  CHECK(notices.ApplyFrame(StarNoticeFrame("prontera", 0, 0, 1), world, link, kNowMs) ==
        ro::net::FrameOutcome::kApplied);
  CHECK(ChatSays(world, "Solar Space"));
  const ro::sim::Actor* player = world.Find(kPlayerId);
  REQUIRE(player != nullptr);
  REQUIRE(player->effects.size() == 1);
  CHECK(player->effects.front().id == kStarNoticeEffect);
  CHECK(notices.TakeSounds().size() == 1);
}

TEST_CASE("les trois commandes de classement", "[app][rank]") {
  CHECK(moonclient::ReadRankCommand("/taekwon") == RankKind::kTaekwon);
  CHECK(moonclient::ReadRankCommand("/Blacksmith") == RankKind::kBlacksmith);
  CHECK(moonclient::ReadRankCommand("/alchemist") == RankKind::kAlchemist);
  CHECK_FALSE(moonclient::ReadRankCommand("/taekwon 3").has_value());
}

TEST_CASE("la mission Taekwon compte les monstres que le joueur achève", "[app][rank]") {
  // Le serveur annonce la mission à cinquante ; deux Poring meurent, l'un
  // achevé par le joueur, l'autre par un inconnu : un seul compte.
  constexpr std::uint32_t kOwnKill = 5001;
  constexpr std::uint32_t kOtherKill = 5002;
  constexpr std::uint32_t kStranger = 150'002;
  ro::sim::World world = WorldWithPlayer();
  moonclient::TaekwonMission mission;
  mission.Start(kPoring, "Poring", kHalfDone);
  for (const std::uint32_t id : {kOwnKill, kOtherKill}) {
    ro::sim::Actor poring;
    poring.id = id;
    poring.kind = ro::sim::ActorKind::kMonster;
    poring.look.job = static_cast<std::uint16_t>(kPoring);
    world.Spawn(poring);
  }
  REQUIRE(world.RecordStrike(ro::sim::Strike{
      .attacker = kPlayerId, .target = kOwnKill, .started_ms = kNowMs, .damage = 9}));
  REQUIRE(world.RecordStrike(ro::sim::Strike{
      .attacker = kStranger, .target = kOtherKill, .started_ms = kNowMs, .damage = 9}));
  REQUIRE(world.SetPosture(kOwnKill, ro::sim::Posture::kDead, kNowMs));
  REQUIRE(world.SetPosture(kOtherKill, ro::sim::Posture::kDead, kNowMs));
  mission.Advance(world, kNowMs);
  CHECK(mission.count() == kHalfDone + 1U);
  CHECK(ChatSays(world, "51 / 100"));
  // Une mort ne se compte qu'une fois.
  mission.Advance(world, kNowMs + kOneSecondMs);
  CHECK(mission.count() == kHalfDone + 1U);
}

TEST_CASE("Feeling demande avant de désigner, et seul OK répond", "[app][rank]") {
  constexpr std::uint8_t kMoon = 1;
  constexpr std::size_t kAgreementBytes = 3;
  ro::test::LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  ro::sim::World world = WorldWithPlayer();
  RankNotices notices;
  moonclient::StarPlaceBox box;
  const ro::net::Frame question{.opcode = 0x0253, .bytes = {0x53, 0x02, kMoon}};

  // La question ouvre la boîte ; Annuler la ferme sans un octet.
  CHECK(notices.ApplyFrame(question, world, link, kNowMs) == ro::net::FrameOutcome::kApplied);
  notices.AdvanceStarPlace(box, link);
  CHECK(box.ask);
  CHECK(box.question.find("désigner cette carte") != std::string::npos);
  box.answer = false;
  notices.AdvanceStarPlace(box, link);
  CHECK(box.question.empty());
  CHECK(server.Receive(kAgreementBytes, kSilenceMs).empty());

  // Reposée, OK renvoie le sujet de la question.
  box.ask = false;
  static_cast<void>(notices.ApplyFrame(question, world, link, kNowMs));
  notices.AdvanceStarPlace(box, link);
  CHECK(box.ask);
  box.answer = true;
  notices.AdvanceStarPlace(box, link);
  CHECK(server.Receive(kAgreementBytes) == std::vector<std::uint8_t>{0x54, 0x02, kMoon});
}

TEST_CASE("la question du sort coûteux a sa propre phrase", "[app][rank]") {
  CHECK(std::string_view(moonclient::StarPlaceQuestion(ro::net::kCostlySpellSubject))
            .find("Black Gemstone") != std::string_view::npos);
  CHECK(std::string_view(moonclient::StarPlaceQuestion(0)) !=
        moonclient::StarPlaceQuestion(ro::net::kCostlySpellSubject));
}

// LES TRAMES DU MERCENAIRE : ce qui le fait paraître, ce qui le change, ce qui
// le fait partir, et l'ordre que le client lui envoie.

#include "ro/net/mercenary_frames.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/connection.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/mercenary_packets.h"
#include "ro/sim/mercenary.h"

#include "support/loopback_link.h"
#include "support/loopback_server.h"
#include "support/net_frames.h"

using ro::net::ApplyMercenaryFrame;
using ro::net::FrameOutcome;
using ro::net::MercenaryClock;
using ro::test::Connect;
using ro::test::LoopbackServer;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kMercenaryId = 110000042;
constexpr std::int64_t kNowUnix = 1'800'000'000;
constexpr MercenaryClock kClock{.now_ms = 1000, .now_unix_s = kNowUnix};

ro::net::Frame InfoFrame() {
  ro::proto::MercenaryInfo info;
  info.actor_id = kMercenaryId;
  info.name = "Archer Mercenary";
  info.level = 3;
  info.hp = 500;
  info.max_hp = 600;
  info.amotion_ms = 700;
  info.expire_unix_s = static_cast<std::uint32_t>(kNowUnix + 600);
  info.faith = 10;
  return FrameOf(ro::proto::EncodeMercenaryInfo(info));
}

ro::net::Frame MessageFrame(std::uint16_t message_id) {
  ro::proto::ZcMsg packet;
  packet.msgId = message_id;
  return FrameOf(ro::proto::Encode(packet));
}

ro::net::Frame VanishFrame(std::uint32_t id, std::uint8_t type) {
  ro::proto::ZcNotifyVanish packet;
  packet.gid = id;
  packet.type = type;
  return FrameOf(ro::proto::Encode(packet));
}

}  // namespace

TEST_CASE("MercenaryFrames : la fiche le fait paraître, avec son contrat", "[mercenaryframes]") {
  ro::sim::MercenaryBook book;
  CHECK(ApplyMercenaryFrame(InfoFrame(), book, kClock) == FrameOutcome::kApplied);
  REQUIRE(book.present());
  CHECK(book.profile().actor_id == kMercenaryId);
  CHECK(book.profile().name == "Archer Mercenary");
  CHECK(book.profile().level == 3);
  CHECK(book.contract_left_s(kClock.now_ms) == 600);
}

TEST_CASE("MercenaryFrames : une valeur isolée change la fiche, et rien sans elle",
          "[mercenaryframes]") {
  ro::sim::MercenaryBook book;
  const ro::net::Frame hp = FrameOf(
      ro::proto::EncodeMercenaryStatus({.variable = ro::proto::kMercenaryVarHp, .value = 123}));
  CHECK(ApplyMercenaryFrame(hp, book, kClock) == FrameOutcome::kIgnored);
  REQUIRE(ApplyMercenaryFrame(InfoFrame(), book, kClock) == FrameOutcome::kApplied);
  CHECK(ApplyMercenaryFrame(hp, book, kClock) == FrameOutcome::kApplied);
  CHECK(book.profile().hp == 123);
  CHECK(ApplyMercenaryFrame(FrameOf(ro::proto::EncodeMercenaryStatus(
                                {.variable = ro::proto::kMercenaryVarAmotion, .value = 600})),
                            book, kClock) == FrameOutcome::kApplied);
  CHECK(book.profile().amotion_ms == 600);
  // Une variable que ce paquet ne porte pas ne touche à rien.
  CHECK(
      ApplyMercenaryFrame(FrameOf(ro::proto::EncodeMercenaryStatus({.variable = 9999, .value = 1})),
                          book, kClock) == FrameOutcome::kIgnored);
}

TEST_CASE("MercenaryFrames : la liste des compétences", "[mercenaryframes]") {
  ro::sim::MercenaryBook book;
  ro::proto::MercenarySkillList list;
  list.skills.push_back({.skill_id = 8223, .targeting = 4, .level = 1, .name = "MER_QUICKEN"});
  CHECK(ApplyMercenaryFrame(FrameOf(ro::proto::EncodeMercenarySkillList(list)), book, kClock) ==
        FrameOutcome::kApplied);
  const ro::sim::MercenarySkill* quicken = book.FindSkill(8223);
  REQUIRE(quicken != nullptr);
  CHECK(quicken->targeting == 4);
  CHECK(quicken->name == "MER_QUICKEN");
}

TEST_CASE("MercenaryFrames : la fin du contrat et la mort le font partir", "[mercenaryframes]") {
  ro::sim::MercenaryBook book;
  REQUIRE(ApplyMercenaryFrame(InfoFrame(), book, kClock) == FrameOutcome::kApplied);
  // Un message qui ne le regarde pas.
  CHECK(ApplyMercenaryFrame(MessageFrame(1), book, kClock) == FrameOutcome::kUnhandled);
  CHECK(book.present());
  CHECK(ApplyMercenaryFrame(MessageFrame(1266), book, kClock) == FrameOutcome::kApplied);
  CHECK_FALSE(book.present());
  CHECK(book.TakeFarewell() == ro::sim::MercenaryFarewell::kContractEnded);

  REQUIRE(ApplyMercenaryFrame(InfoFrame(), book, kClock) == FrameOutcome::kApplied);
  CHECK(ApplyMercenaryFrame(MessageFrame(1267), book, kClock) == FrameOutcome::kApplied);
  CHECK_FALSE(book.present());
  CHECK(book.TakeFarewell() == ro::sim::MercenaryFarewell::kDied);
}

TEST_CASE("MercenaryFrames : la disparition de son acteur mort l'efface, sans la prendre au monde",
          "[mercenaryframes]") {
  ro::sim::MercenaryBook book;
  REQUIRE(ApplyMercenaryFrame(InfoFrame(), book, kClock) == FrameOutcome::kApplied);
  // Hors de vue : il reste sous contrat.
  CHECK(ApplyMercenaryFrame(VanishFrame(kMercenaryId, 0), book, kClock) ==
        FrameOutcome::kUnhandled);
  CHECK(book.present());
  // Un autre acteur mort.
  CHECK(ApplyMercenaryFrame(VanishFrame(kMercenaryId + 1, 1), book, kClock) ==
        FrameOutcome::kUnhandled);
  CHECK(book.present());
  CHECK(ApplyMercenaryFrame(VanishFrame(kMercenaryId, 1), book, kClock) ==
        FrameOutcome::kUnhandled);
  CHECK_FALSE(book.present());
}

TEST_CASE("MercenaryFrames : la fiche et le renvoi partent par la commande du mercenaire",
          "[mercenaryframes]") {
  LoopbackServer server;
  REQUIRE(server.listening());
  ro::net::Connection link = Connect(server);
  REQUIRE(ro::net::RequestMercenaryInfo(link).has_value());
  REQUIRE(ro::net::RequestMercenaryDismiss(link).has_value());
  for (const std::uint8_t expected :
       {ro::proto::kMercenaryCommandInfo, ro::proto::kMercenaryCommandDismiss}) {
    const std::vector<std::uint8_t> sent = server.Receive(ro::proto::MercenaryCommand::kWireSize);
    REQUIRE(sent.size() == ro::proto::MercenaryCommand::kWireSize);
    CHECK(sent[0] == 0x9F);
    CHECK(sent[1] == 0x02);
    CHECK(sent[2] == expected);
  }
}

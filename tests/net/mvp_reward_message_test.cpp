// Ce que la discussion dit au MVP : l'objet qu'il reçoit, ou la récompense que
// son poids lui fait perdre.

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/net/server_message.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::core::Tr;
using ro::net::ApplyServerMessageFrame;
using ro::net::FrameOutcome;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kNowMs = 4242;
// Les couleurs à l'endroit (RGB) : le client les range à l'envers.
constexpr std::uint32_t kCyan = 0x00FFFF;
constexpr std::uint32_t kRed = 0xFF0000;

constexpr std::uint32_t kOldCardAlbum = 616;
constexpr std::uint32_t kUnknownItem = 999'999;

ro::net::Frame MvpItem(std::uint32_t item_id) {
  ro::proto::ZcMvpGettingItem packet;
  packet.itemId = item_id;
  return FrameOf(ro::proto::Encode(packet));
}

std::string NameOf(std::uint32_t item_id) {
  return item_id == kOldCardAlbum ? "Old Card Album" : std::string{};
}

std::string RewardLine(const std::string& name) {
  std::string line = Tr("Félicitations ! Vous êtes le MVP ! Votre récompense : %s");
  line.replace(line.find("%s"), 2, name);
  return line;
}

const ro::sim::ChatLine& OnlyLine(const ro::sim::World& world) {
  REQUIRE(world.chat_lines().size() == 1);
  return world.chat_lines().front();
}

}  // namespace

TEST_CASE("MVP : les deux paquets de récompense se décodent", "[net][server_message]") {
  ro::proto::ZcMvpGettingItem item;
  REQUIRE(ro::proto::Decode(ro::proto::Encode(ro::proto::ZcMvpGettingItem{kOldCardAlbum}), item));
  CHECK(item.itemId == kOldCardAlbum);
  CHECK(ro::proto::ZcMvpGettingItem::kWireSize == 6);

  ro::proto::ZcThrowMvpitem thrown;
  CHECK(ro::proto::Decode(ro::proto::Encode(ro::proto::ZcThrowMvpitem{}), thrown));
  CHECK(ro::proto::ZcThrowMvpitem::kWireSize == 2);
}

TEST_CASE("MVP : l'objet reçu s'écrit en cyan, son nom à la suite du message",
          "[net][server_message]") {
  ro::sim::World world;
  CHECK(ApplyServerMessageFrame(MvpItem(kOldCardAlbum), world, nullptr, kNowMs, NameOf) ==
        FrameOutcome::kApplied);
  const ro::sim::ChatLine& line = OnlyLine(world);
  CHECK(line.text == RewardLine("Old Card Album"));
  CHECK(line.color == kCyan);
  CHECK(line.kind == ro::sim::ChatKind::kServer);
  CHECK(line.type == ro::sim::ChatType::kPublic);
  CHECK(line.said_ms == kNowMs);
}

TEST_CASE("MVP : un objet inconnu se nomme d'une espace", "[net][server_message]") {
  ro::sim::World world;
  REQUIRE(ApplyServerMessageFrame(MvpItem(kUnknownItem), world, nullptr, kNowMs, NameOf) ==
          FrameOutcome::kApplied);
  CHECK(OnlyLine(world).text == RewardLine(" "));

  ro::sim::World without_table;
  REQUIRE(ApplyServerMessageFrame(MvpItem(kOldCardAlbum), without_table, nullptr, kNowMs) ==
          FrameOutcome::kApplied);
  CHECK(OnlyLine(without_table).text == RewardLine(" "));
}

TEST_CASE("MVP : la récompense perdue au poids s'écrit en rouge", "[net][server_message]") {
  ro::sim::World world;
  CHECK(ApplyServerMessageFrame(FrameOf(ro::proto::Encode(ro::proto::ZcThrowMvpitem{})), world,
                                nullptr, kNowMs) == FrameOutcome::kApplied);
  const ro::sim::ChatLine& line = OnlyLine(world);
  CHECK(line.text == Tr("Vous êtes le MVP, mais vous ne pouvez pas prendre la récompense : vous "
                        "dépassez votre limite de poids."));
  CHECK(line.color == kRed);
  CHECK(line.type == ro::sim::ChatType::kPublic);
}

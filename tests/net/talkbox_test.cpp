// LE MESSAGE D'UNE TALKIE BOX : le serveur le dit au nom de l'unité au sol, et
// il se pose sur elle.

#include "ro/net/talkbox.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kBoxId = 110042;
constexpr std::uint32_t kTalkieBoxUnit = 153;
constexpr std::uint32_t kSaidAtMs = 7000;

ro::proto::ZcTalkboxChatcontents Said(std::uint32_t unit_id, const char* text) {
  ro::proto::ZcTalkboxChatcontents packet;
  packet.aid = unit_id;
  packet.message = text;
  return packet;
}

}  // namespace

TEST_CASE("Talkie Box : le message déclenché se pose sur l'unité qui le dit", "[net][talkbox]") {
  ro::sim::World world;
  ro::sim::SkillUnit box;
  box.id = kBoxId;
  box.unit_id = kTalkieBoxUnit;
  box.cell = ro::sim::CellPosition{40, 50, 0};
  world.PlaceSkillUnit(box);

  // 🔴 LA TRAME PASSE PAR L'AIGUILLAGE ORDINAIRE : sans son traducteur, elle
  // n'arrivait nulle part, et rien ne paraissait au-dessus du piège.
  CHECK(ApplyFrame(FrameOf(Encode(Said(kBoxId, "Coucou"))), world, kSaidAtMs) ==
        FrameOutcome::kApplied);
  const ro::sim::SkillUnit& told = world.skill_units().at(kBoxId);
  CHECK(told.said == "Coucou");
  CHECK(told.said_since_ms == kSaidAtMs);
}

TEST_CASE("Talkie Box : une boîte hors de vue ne fait rien paraître", "[net][talkbox]") {
  ro::sim::World world;
  CHECK(ro::net::ApplyTalkboxFrame(FrameOf(Encode(Said(kBoxId, "Coucou"))), world, kSaidAtMs) ==
        FrameOutcome::kIgnored);
  CHECK(world.skill_units().empty());
}

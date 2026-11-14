// LA LISTE DE DEVOTION, du fil au monde : le paquet que le serveur écrit à la
// main, sa traduction, et ce que le monde en retient.

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/manual_packets.h"
#include "ro/proto/packet_lengths.h"
#include "ro/sim/actor.h"
#include "ro/sim/devotion.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kCrusader = 2'000'001;
constexpr std::uint32_t kFirstDevotee = 2'000'002;
constexpr std::uint32_t kSecondDevotee = 2'000'003;
constexpr std::int16_t kRangeCells = 7;

// 0x01CF : le croisé, cinq protégés, la portée — vingt-huit octets.
std::vector<std::uint8_t> DevotionFrame(std::uint32_t devoter,
                                        const std::array<std::uint32_t, 5>& devotees) {
  std::vector<std::uint8_t> frame = {0xCF, 0x01};
  const auto put32 = [&frame](std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
      frame.push_back(static_cast<std::uint8_t>(value >> shift));
    }
  };
  put32(devoter);
  for (const std::uint32_t devotee : devotees) {
    put32(devotee);
  }
  frame.push_back(static_cast<std::uint8_t>(kRangeCells));
  frame.push_back(0x00);
  return frame;
}

ro::sim::World WorldWith(std::uint32_t id) {
  ro::sim::World world;
  ro::sim::Actor actor;
  actor.id = id;
  actor.kind = ro::sim::ActorKind::kPlayer;
  world.Spawn(actor);
  return world;
}

}  // namespace

TEST_CASE("Devotion : le paquet tient sur vingt-huit octets, portée comprise", "[net][devotion]") {
  // La longueur est celle que le serveur déclare pour cet opcode.
  const std::optional<std::int16_t> declared =
      ro::proto::PacketLength(ro::proto::DevotionListNotice::kOpcode);
  REQUIRE(declared.has_value());
  CHECK(static_cast<std::size_t>(*declared) == ro::proto::DevotionListNotice::kWireSize);

  const std::vector<std::uint8_t> frame =
      DevotionFrame(kCrusader, {0, kFirstDevotee, 0, 0, kSecondDevotee});
  ro::proto::DevotionListNotice notice;
  REQUIRE(ro::proto::Decode(frame, notice));
  CHECK(notice.devoter_id == kCrusader);
  // ⚠ UNE CASE VIDE PEUT PRÉCÉDER UNE PLEINE : le serveur ne tasse pas.
  CHECK(notice.devotee_ids == std::array<std::uint32_t, 5>{0, kFirstDevotee, 0, 0, kSecondDevotee});
  CHECK(notice.range == kRangeCells);

  std::vector<std::uint8_t> short_frame = frame;
  short_frame.pop_back();
  CHECK_FALSE(ro::proto::Decode(short_frame, notice));
  std::vector<std::uint8_t> other = frame;
  other[0] = 0xD0;
  CHECK_FALSE(ro::proto::Decode(other, notice));
}

TEST_CASE("Devotion : chaque liste s'inscrit sur le croisé, et se COMPTE", "[net][devotion]") {
  ro::sim::World world = WorldWith(kCrusader);
  const std::vector<std::uint8_t> frame = DevotionFrame(kCrusader, {kFirstDevotee, 0, 0, 0, 0});

  REQUIRE(ApplyFrame(FrameOf(frame), world, 1000) == FrameOutcome::kApplied);
  const ro::sim::Actor* crusader = world.Find(kCrusader);
  REQUIRE(crusader != nullptr);
  CHECK(crusader->devotion.devotee_ids[0] == kFirstDevotee);
  CHECK(crusader->devotion.told_serial == 1);

  // 🔴 LA MÊME LISTE REDITE EST UNE ANNONCE DE PLUS : le client retend alors
  // les traits, et le monde doit le laisser voir.
  REQUIRE(ApplyFrame(FrameOf(frame), world, 1100) == FrameOutcome::kApplied);
  CHECK(world.Find(kCrusader)->devotion.told_serial == 2);

  // LA LISTE VIDE défait tout.
  REQUIRE(ApplyFrame(FrameOf(DevotionFrame(kCrusader, {})), world, 1200) == FrameOutcome::kApplied);
  CHECK(world.Find(kCrusader)->devotion.devotee_ids == std::array<std::uint32_t, 5>{});
  CHECK(world.Find(kCrusader)->devotion.told_serial == 3);
}

TEST_CASE("Devotion : un croisé hors de vue n'est pas inventé", "[net][devotion]") {
  ro::sim::World world;
  CHECK(ApplyFrame(FrameOf(DevotionFrame(kCrusader, {kFirstDevotee, 0, 0, 0, 0})), world, 1000) ==
        FrameOutcome::kIgnored);
  CHECK(world.actors().empty());
}

TEST_CASE("Devotion : la réannonce du croisé garde ses protégés, la carte suivante non",
          "[sim][devotion]") {
  ro::sim::World world = WorldWith(kCrusader);
  world.set_player_id(kCrusader);
  REQUIRE(world.TellDevotion(kCrusader, {kFirstDevotee, 0, 0, 0, 0}));

  // UNE RÉANNONCE — le serveur en envoie une à chaque mise en marche — ne
  // défait aucun trait : le numéro perdu passerait pour une annonce neuve.
  ro::sim::Actor reannounced;
  reannounced.id = kCrusader;
  reannounced.kind = ro::sim::ActorKind::kPlayer;
  world.Spawn(reannounced);
  CHECK(world.Find(kCrusader)->devotion.devotee_ids[0] == kFirstDevotee);
  CHECK(world.Find(kCrusader)->devotion.told_serial == 1);

  // LE CHANGEMENT DE CARTE, lui, détruit les traits avec l'acteur.
  world.EnterMap("prontera", 150, 150);
  REQUIRE(world.Find(kCrusader) != nullptr);
  CHECK(world.Find(kCrusader)->devotion.devotee_ids == std::array<std::uint32_t, 5>{});
  CHECK(world.Find(kCrusader)->devotion.told_serial == 0);
}

TEST_CASE("Devotion : un croisé qui revient en vue reçoit un numéro d'annonce neuf",
          "[sim][devotion]") {
  ro::sim::World world = WorldWith(kCrusader);
  REQUIRE(world.TellDevotion(kCrusader, {kFirstDevotee, 0, 0, 0, 0}));
  const std::uint32_t before = world.Find(kCrusader)->devotion.told_serial;

  // 🔴 IL SORT ET REVIENT : c'est un acteur NEUF, et le serveur lui renvoie sa
  // liste à l'entrée en vue. Son numéro ne doit pas retomber sur l'ancien, sans
  // quoi la liste neuve passerait pour déjà tendue.
  REQUIRE(world.Remove(kCrusader));
  ro::sim::Actor back;
  back.id = kCrusader;
  back.kind = ro::sim::ActorKind::kPlayer;
  world.Spawn(back);
  CHECK(world.Find(kCrusader)->devotion.told_serial == 0);
  REQUIRE(world.TellDevotion(kCrusader, {kFirstDevotee, kSecondDevotee, 0, 0, 0}));
  CHECK(world.Find(kCrusader)->devotion.told_serial != before);
}

// LA GUILDE D'UN ACTEUR QUI EN CHANGE SOUS NOS YEUX.
//
// 🔴 CE BANC GARDE UN ÉTAT QUI RESTERAIT FAUX SANS BRUIT : un joueur qui entre
// dans une guilde — ou la quitte — en restant immobile garderait l'ancienne,
// et son étiquette l'ancien emblème, jusqu'à ce qu'il fasse un pas.

#include "ro/net/actor_guild.h"

#include <cstdint>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

using ro::net::ApplyActorGuildFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::NameCard;
using ro::sim::World;

namespace {

constexpr std::uint32_t kPlayer = 2'000'123;
constexpr std::uint32_t kStranger = 2'000'999;
constexpr std::uint32_t kOldGuild = 7;

[[nodiscard]] World WithGuildmate() {
  World world;
  Actor actor;
  actor.id = kPlayer;
  actor.kind = ActorKind::kPlayer;
  actor.name = "Bob";
  actor.guild_id = kOldGuild;
  actor.health = 40;
  actor.max_health = 100;
  world.Spawn(std::move(actor));
  static_cast<void>(world.TellActorName(kPlayer, "Bob", NameCard{.banner = "Lune"}));
  return world;
}

[[nodiscard]] Frame ChangeGuild(std::uint32_t aid, std::int32_t guild_id) {
  ro::proto::ZcChangeGuild packet;
  packet.guild_id = guild_id;
  packet.emblem_id = 4;
  packet.AID = aid;
  return Frame{.opcode = ro::proto::ZcChangeGuild::kOpcode, .bytes = ro::proto::Encode(packet)};
}

}  // namespace

TEST_CASE("Guilde d'un acteur : le changement se pose, le reste de l'acteur demeure",
          "[net][guild]") {
  World world = WithGuildmate();
  REQUIRE(ApplyActorGuildFrame(ChangeGuild(kPlayer, 9), world) == FrameOutcome::kApplied);
  const Actor* bob = world.Find(kPlayer);
  REQUIRE(bob != nullptr);
  CHECK(bob->guild_id == 9);
  // ⚠ UNE RÉANNONCE, PAS UNE RÉAPPARITION : ce que d'autres paquets avaient dit
  // reste — la vie, et la carte de nom, que rien ne redirait.
  CHECK(bob->health == 40);
  REQUIRE(bob->card.has_value());
  CHECK(bob->card->banner == "Lune");
}

TEST_CASE("Guilde d'un acteur : zéro est un départ", "[net][guild]") {
  World world = WithGuildmate();
  REQUIRE(ApplyActorGuildFrame(ChangeGuild(kPlayer, 0), world) == FrameOutcome::kApplied);
  CHECK(world.Find(kPlayer)->guild_id == 0);
}

TEST_CASE("Guilde d'un acteur : un inconnu ne s'invente pas, une autre trame ne se lit pas",
          "[net][guild]") {
  World world = WithGuildmate();
  CHECK(ApplyActorGuildFrame(ChangeGuild(kStranger, 9), world) == FrameOutcome::kIgnored);
  CHECK(world.Find(kStranger) == nullptr);
  const Frame other{.opcode = 0x0080, .bytes = {0x80, 0x00}};
  CHECK(ApplyActorGuildFrame(other, world) == FrameOutcome::kUnhandled);
}

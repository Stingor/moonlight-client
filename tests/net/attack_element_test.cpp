// L'ÉLÉMENT D'ATTAQUE D'UN JOUEUR — `ZC_MOONLIGHT_ATTACK_ELEMENT 0x0F4A`, et ce que
// le monde en garde.

#include "ro/sim/attack_element.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::Element;
using ro::test_support::FrameOf;
using ro::test_support::IdleUnit;

namespace {

constexpr std::uint32_t kArcherId = 2'000'147;
constexpr std::uint32_t kSelfId = 2'000'001;
constexpr std::uint32_t kNowMs = 5000;
constexpr std::uint8_t kWireFire = 3;
constexpr std::uint8_t kWireUndead = 9;
constexpr std::uint8_t kWireNone = 0xFF;
constexpr std::uint8_t kWireBeyondList = 10;

FrameOutcome TellElement(ro::sim::World& world, std::uint32_t player_id, std::uint8_t wire) {
  ro::proto::ZcMoonlightAttackElement told;
  told.GID = player_id;
  told.element = wire;
  return ApplyFrame(FrameOf(Encode(told)), world, kNowMs);
}

Element ElementOf(const ro::sim::World& world, std::uint32_t player_id) {
  const ro::sim::Actor* actor = world.Find(player_id);
  return actor != nullptr ? actor->attack_element : Element::kNone;
}

void Appear(ro::sim::World& world, std::uint32_t player_id) {
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(player_id, {100, 100}))), world, kNowMs) ==
          FrameOutcome::kApplied);
}

}  // namespace

TEST_CASE("Élément d'attaque : la trame tient en neuf octets", "[net][attack_element]") {
  // 4a 0f | 09 00 | joueur | élément
  const std::vector<std::uint8_t> bytes = {0x4A, 0x0F, 0x09, 0x00,     0x13,
                                           0x85, 0x1E, 0x00, kWireFire};
  REQUIRE(bytes.size() == ro::proto::ZcMoonlightAttackElement::kWireSize);
  ro::proto::ZcMoonlightAttackElement packet;
  REQUIRE(ro::proto::Decode(bytes, packet));
  CHECK(packet.GID == kArcherId);
  CHECK(packet.element == kWireFire);
}

TEST_CASE("Élément d'attaque : l'ordre du serveur, et aucun hors de la liste",
          "[net][attack_element]") {
  STATIC_CHECK(ro::sim::ElementOfWire(0) == Element::kNeutral);
  STATIC_CHECK(ro::sim::ElementOfWire(kWireFire) == Element::kFire);
  STATIC_CHECK(ro::sim::ElementOfWire(kWireUndead) == Element::kUndead);
  STATIC_CHECK(ro::sim::ElementOfWire(kWireBeyondList) == Element::kNone);
  STATIC_CHECK(ro::sim::ElementOfWire(kWireNone) == Element::kNone);
}

TEST_CASE("Élément d'attaque : la trame le pose sur le joueur, « aucun » l'efface",
          "[net][attack_element]") {
  ro::sim::World world;
  Appear(world, kArcherId);
  // 🔴 SANS PAQUET, AUCUN : rien ne se déduit de ce que le joueur porte.
  CHECK(ElementOf(world, kArcherId) == Element::kNone);

  REQUIRE(TellElement(world, kArcherId, kWireFire) == FrameOutcome::kApplied);
  CHECK(ElementOf(world, kArcherId) == Element::kFire);
  // Le serveur le retire quand le joueur passe sous la règle du combat.
  REQUIRE(TellElement(world, kArcherId, kWireNone) == FrameOutcome::kApplied);
  CHECK(ElementOf(world, kArcherId) == Element::kNone);
}

TEST_CASE("Élément d'attaque : avant l'apparition, la trame est ignorée", "[net][attack_element]") {
  ro::sim::World world;
  CHECK(TellElement(world, kArcherId, kWireFire) == FrameOutcome::kIgnored);
  Appear(world, kArcherId);
  CHECK(ElementOf(world, kArcherId) == Element::kNone);
}

TEST_CASE("Élément d'attaque : une réannonce le garde, une sortie de vue l'emporte",
          "[net][attack_element]") {
  ro::sim::World world;
  Appear(world, kArcherId);
  REQUIRE(TellElement(world, kArcherId, kWireFire) == FrameOutcome::kApplied);
  // La réannonce de marche ne le redit pas.
  Appear(world, kArcherId);
  CHECK(ElementOf(world, kArcherId) == Element::kFire);
  // Sorti de vue puis revenu : c'est au serveur de le redire.
  REQUIRE(world.Remove(kArcherId));
  Appear(world, kArcherId);
  CHECK(ElementOf(world, kArcherId) == Element::kNone);
}

TEST_CASE("Élément d'attaque : l'entrée en carte l'efface, le nôtre compris",
          "[net][attack_element]") {
  ro::sim::World world;
  world.set_player_id(kSelfId);
  Appear(world, kSelfId);
  Appear(world, kArcherId);
  REQUIRE(TellElement(world, kSelfId, kWireUndead) == FrameOutcome::kApplied);
  REQUIRE(TellElement(world, kArcherId, kWireFire) == FrameOutcome::kApplied);

  world.EnterMap("pvp_y_1-1", 50, 50);
  // 🔴 LE NÔTRE TRAVERSE, SON ÉLÉMENT NON : la carte d'arrivée peut le cacher.
  REQUIRE(world.Find(kSelfId) != nullptr);
  CHECK(ElementOf(world, kSelfId) == Element::kNone);
  CHECK(world.Find(kArcherId) == nullptr);
}

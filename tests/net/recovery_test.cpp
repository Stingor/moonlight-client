// LA VIE ET LA MAGIE RENDUES AU JOUEUR : ce que `ZC_RECOVERY` fait du monde,
// le chiffre, lui, est essayé côté application (tests/app/recovery_number_test.cpp).
//
// C'est la trame du tic d'Increase HP Recovery, de celui d'Increase SP Recovery
// et de la magie que Soul Drain arrache : chacune montre un chiffre et un effet
// sur le joueur, et la magie n'a pas d'autre porteur que celle-ci.

#include "ro/net/recovery.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/map_packets.h"
#include "ro/sim/actor.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::World;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kMyId = 2'000'001;
constexpr std::uint16_t kHpKind = 5;
constexpr std::uint16_t kSpKind = 7;
constexpr std::uint16_t kApKind = 232;

std::vector<std::uint8_t> RecoveryFrame(std::uint16_t kind, std::int32_t amount) {
  ro::proto::ZcRecovery packet;
  packet.type = kind;
  packet.amount = amount;
  return ro::proto::Encode(packet);
}

World WorldWithMe() {
  World world;
  ro::sim::Actor me;
  me.id = kMyId;
  world.Spawn(me);
  world.set_player_id(kMyId);
  return world;
}

const ro::sim::PlayedEffect* EffectOf(const World& world, std::uint32_t effect_id) {
  const ro::sim::Actor* actor = world.Find(kMyId);
  if (actor == nullptr) {
    return nullptr;
  }
  const auto found = std::ranges::find(actor->effects, effect_id, &ro::sim::PlayedEffect::id);
  return found == actor->effects.end() ? nullptr : &*found;
}

}  // namespace

TEST_CASE("Récupération : la vie rendue pose son effet et son montant sur le joueur",
          "[net][recovery]") {
  World world = WorldWithMe();
  REQUIRE(world.SetHealth(kMyId, 400, 1000));

  CHECK(ApplyFrame(FrameOf(RecoveryFrame(kHpKind, 35)), world, 10'000) == FrameOutcome::kApplied);

  const ro::sim::PlayedEffect* effect = EffectOf(world, ro::net::kHpRecoveryEffect);
  REQUIRE(effect != nullptr);
  CHECK(effect->since_ms == 10'000);
  REQUIRE(effect->value.has_value());
  CHECK(*effect->value == 35);
  // Et la vie monte d'autant, sans attendre le paquet de paramètre.
  CHECK(world.Find(kMyId)->health == 435);
}

TEST_CASE("Récupération : la magie rendue — le tic d'Increase SP Recovery, Soul Drain",
          "[net][recovery]") {
  World world = WorldWithMe();
  REQUIRE(world.SetPlayerParameter(static_cast<std::uint16_t>(ro::sim::ServerParameter::kSp),
                                   120) == ro::sim::ParameterOutcome::kStored);

  CHECK(ApplyFrame(FrameOf(RecoveryFrame(kSpKind, 57)), world, 20'000) == FrameOutcome::kApplied);

  const ro::sim::PlayedEffect* effect = EffectOf(world, ro::net::kSpRecoveryEffect);
  REQUIRE(effect != nullptr);
  REQUIRE(effect->value.has_value());
  CHECK(*effect->value == 57);
  // 🔴 LE SEUL PORTEUR de la nouvelle magie : le serveur n'en dit rien d'autre.
  CHECK(world.player_status().sp == 177);
  CHECK(EffectOf(world, ro::net::kHpRecoveryEffect) == nullptr);
}

TEST_CASE("Récupération : une jauge encore inconnue ne s'invente pas", "[net][recovery]") {
  World world = WorldWithMe();

  CHECK(ApplyFrame(FrameOf(RecoveryFrame(kSpKind, 10)), world, 1000) == FrameOutcome::kApplied);
  CHECK(ApplyFrame(FrameOf(RecoveryFrame(kHpKind, 10)), world, 1000) == FrameOutcome::kApplied);

  CHECK(world.player_status().sp == ro::sim::kUnknownStat);
  CHECK(world.Find(kMyId)->health == ro::sim::kUnknownHealth);
  // Le chiffre et l'effet paraissent quand même : ils ne dépendent pas de la jauge.
  CHECK(EffectOf(world, ro::net::kSpRecoveryEffect) != nullptr);
  CHECK(EffectOf(world, ro::net::kHpRecoveryEffect) != nullptr);
}

TEST_CASE("Récupération : ni l'AP ni un joueur absent ne montrent rien", "[net][recovery]") {
  World world = WorldWithMe();
  CHECK(ApplyFrame(FrameOf(RecoveryFrame(kApKind, 5)), world, 1000) == FrameOutcome::kIgnored);
  CHECK(world.Find(kMyId)->effects.empty());

  World empty;
  CHECK(ApplyFrame(FrameOf(RecoveryFrame(kHpKind, 5)), empty, 1000) == FrameOutcome::kIgnored);
}

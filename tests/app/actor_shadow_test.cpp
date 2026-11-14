// Si l'ombre d'un acteur se peint, d'après sa famille et son état.
//
// 🔴 UN JOUEUR ASSIS N'A PAS D'OMBRE. Elle restait sous lui, et le corps assis,
// plus bas que le disque, paraissait léviter au-dessus.

#include "world/actor_shadow.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using moonclient::CastsShadow;
using moonclient::ShadowFacts;
using ro::render::Motion;
using ro::sim::ActorKind;

namespace {

constexpr std::uint32_t kKnight = 7;
constexpr std::uint32_t kStarGladiator = 4047;
// Les cinq corps du vol de l'union.
constexpr std::uint32_t kStarGladiatorUnion = 4048;
constexpr std::uint32_t kBabyStarGladiatorUnion = 4238;
constexpr std::uint32_t kStarEmperorUnion = 4243;
constexpr std::uint32_t kBabyStarEmperorUnion = 4244;
constexpr std::uint32_t kSkyEmperorUnion = 4316;
constexpr std::uint32_t kPoring = 1002;
constexpr std::uint8_t kOpaque = 255;
constexpr std::uint8_t kHalfAlpha = 128;
constexpr std::uint8_t kAlmostOpaque = 254;

[[nodiscard]] ShadowFacts Player(Motion motion, std::uint32_t shown_job = kKnight) {
  return ShadowFacts{.kind = ActorKind::kPlayer, .motion = motion, .shown_job = shown_job};
}

[[nodiscard]] ShadowFacts Creature(ActorKind kind, Motion motion, std::uint8_t alpha) {
  return ShadowFacts{.kind = kind, .motion = motion, .shown_job = kPoring, .body_alpha = alpha};
}

}  // namespace

TEST_CASE("Ombre : un joueur la perd assis ou mort, et la garde partout ailleurs",
          "[app][shadow]") {
  CHECK_FALSE(CastsShadow(Player(Motion::kSit)));
  CHECK_FALSE(CastsShadow(Player(Motion::kDie)));
  for (const Motion motion : {Motion::kIdle, Motion::kWalk, Motion::kPickup, Motion::kReady,
                              Motion::kAttack, Motion::kCast, Motion::kHurt}) {
    CHECK(CastsShadow(Player(motion)));
  }
  // Un métier qui ne vole pas n'y échappe pas, même celui dont l'union vole.
  CHECK_FALSE(CastsShadow(Player(Motion::kSit, kStarGladiator)));
}

TEST_CASE("Ombre : un joueur ne la perd pas en devenant translucide", "[app][shadow]") {
  ShadowFacts faded = Player(Motion::kIdle);
  faded.body_alpha = kHalfAlpha;
  CHECK(CastsShadow(faded));
}

TEST_CASE("Ombre : le vol de l'union la garde assis, pas mort", "[app][shadow]") {
  for (const std::uint32_t body : {kStarGladiatorUnion, kBabyStarGladiatorUnion, kStarEmperorUnion,
                                   kBabyStarEmperorUnion, kSkyEmperorUnion}) {
    CHECK(CastsShadow(Player(Motion::kSit, body)));
    CHECK_FALSE(CastsShadow(Player(Motion::kDie, body)));
  }
}

TEST_CASE("Ombre : une créature ou un PNJ ne la porte que pleinement opaque", "[app][shadow]") {
  for (const ActorKind kind : {ActorKind::kMonster, ActorKind::kNonPlayer}) {
    CHECK(CastsShadow(Creature(kind, Motion::kIdle, kOpaque)));
    // Son geste ne compte pas : une créature qui meurt la garde, et un PNJ
    // vêtu d'un métier la garde assis.
    CHECK(CastsShadow(Creature(kind, Motion::kDie, kOpaque)));
    CHECK(CastsShadow(Creature(kind, Motion::kSit, kOpaque)));
    // Le moindre fondu la retire.
    CHECK_FALSE(CastsShadow(Creature(kind, Motion::kIdle, kAlmostOpaque)));
    CHECK_FALSE(CastsShadow(Creature(kind, Motion::kDie, kHalfAlpha)));
  }
}

TEST_CASE("Ombre : un objet au sol la porte, ce qui sort de vue la perd", "[app][shadow]") {
  CHECK(CastsShadow(ShadowFacts{.kind = ActorKind::kItem}));
  ShadowFacts leaving = Player(Motion::kIdle);
  leaving.leaving_sight = true;
  CHECK_FALSE(CastsShadow(leaving));
  ShadowFacts leaving_monster = Creature(ActorKind::kMonster, Motion::kIdle, kOpaque);
  leaving_monster.leaving_sight = true;
  CHECK_FALSE(CastsShadow(leaving_monster));
  CHECK_FALSE(CastsShadow(ShadowFacts{.kind = ActorKind::kSkillUnit}));
}

// UN EFFET FINI QUITTE LA LISTE DE SON ACTEUR, et le plafond n'est plus qu'un
// garde-fou : ces essais tiennent la règle du monde jusqu'à `app`, qui seule
// connaît la durée d'un effet.

#include "world/actor_effect_finish.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_duration.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

namespace {

constexpr std::uint32_t kBearerId = 150'404;
constexpr std::uint32_t kBornMs = 1'000;
constexpr auto kTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);

// Un effet qui ne touche pas son porteur et dont l'instance vit quatre-vingt-dix
// tics.
constexpr std::uint32_t kBoundedEffect = 305;
constexpr std::uint32_t kBoundedLifeTicks = 90;
// Un effet dont l'instance ne meurt jamais d'elle-même.
constexpr std::uint32_t kEndlessEffect = 301;
// Un effet qui fait trembler le corps : sa trace se relit dans la liste.
constexpr std::uint32_t kBodyMarkingEffect = 426;
// Une valeur que l'effet porte, comme un rang ou un nombre de sphères.
constexpr std::int32_t kToldValue = 3;
// Des numéros dont aucune durée n'est relevée : la liste les garde jusqu'au
// retrait, et c'est le plafond seul qui les borne.
constexpr std::uint32_t kFirstUnmeasuredEffect = 900'000;
// Bien au-delà de toute vie bornée relevée.
constexpr std::uint32_t kMuchLaterTicks = 1'000'000;

std::uint32_t AtTick(std::uint32_t tick) { return kBornMs + tick * kTickMs; }

struct Bench {
  ro::sim::World world;

  Bench() {
    ro::sim::Actor bearer;
    bearer.id = kBearerId;
    bearer.kind = ro::sim::ActorKind::kPlayer;
    world.Spawn(bearer);
  }

  [[nodiscard]] bool Carries(std::uint32_t effect_id) const {
    const ro::sim::Actor* bearer = world.Find(kBearerId);
    return std::ranges::find(bearer->effects, effect_id, &ro::sim::PlayedEffect::id) !=
           bearer->effects.end();
  }

  [[nodiscard]] std::size_t Count() const { return world.Find(kBearerId)->effects.size(); }
};

}  // namespace

TEST_CASE("Effets finis : les tables tiennent les faits que ces essais supposent",
          "[app][effectfinish]") {
  REQUIRE(ro::render::ActorEffectChangeFor(kBoundedEffect) == nullptr);
  REQUIRE(ro::render::EffectDurationIsKnown(kBoundedEffect));
  REQUIRE(ro::render::EffectDurationTicks(kBoundedEffect) ==
          static_cast<std::int32_t>(kBoundedLifeTicks));
  REQUIRE(ro::render::ActorEffectChangeFor(kEndlessEffect) == nullptr);
  REQUIRE(ro::render::EffectDurationTicks(kEndlessEffect) >= ro::render::kEffectLifeForever);
  REQUIRE(ro::render::ActorEffectChangeFor(kBodyMarkingEffect) != nullptr);
  REQUIRE_FALSE(ro::render::EffectDurationIsKnown(kFirstUnmeasuredEffect));
}

TEST_CASE("Effets finis : un effet à vie bornée quitte la liste le tic où son instance meurt",
          "[app][effectfinish]") {
  Bench bench;
  REQUIRE(bench.world.RecordEffect(kBearerId, kBoundedEffect, kBornMs));

  CHECK(moonclient::ForgetFinishedEffects(bench.world, AtTick(kBoundedLifeTicks - 1)) == 0);
  CHECK(bench.Carries(kBoundedEffect));

  CHECK(moonclient::ForgetFinishedEffects(bench.world, AtTick(kBoundedLifeTicks)) == 1);
  CHECK_FALSE(bench.Carries(kBoundedEffect));
  // ⚠ ÔTÉ SANS ÊTRE RETIRÉ : rien ne joue plus pour lui, et le journal des
  // retraits ne doit pas couper une instance reposée depuis.
  CHECK(bench.world.Find(kBearerId)->removed_effects_total == 0);
}

TEST_CASE("Effets finis : rejoué, un effet repart de zéro et vit de nouveau toute sa vie",
          "[app][effectfinish]") {
  Bench bench;
  REQUIRE(bench.world.RecordEffect(kBearerId, kBoundedEffect, kBornMs));
  const std::uint32_t replayed_ms = AtTick(kBoundedLifeTicks / 2);
  REQUIRE(bench.world.RecordEffect(kBearerId, kBoundedEffect, replayed_ms));

  CHECK(moonclient::ForgetFinishedEffects(bench.world, AtTick(kBoundedLifeTicks)) == 0);
  CHECK(bench.Carries(kBoundedEffect));
  CHECK(moonclient::ForgetFinishedEffects(bench.world, replayed_ms + kBoundedLifeTicks * kTickMs) ==
        1);
}

TEST_CASE("Effets finis : un effet en boucle ou d'office ne finit que sur retrait",
          "[app][effectfinish]") {
  Bench bench;
  REQUIRE(bench.world.RecordEffect(kBearerId, kEndlessEffect, kBornMs));
  REQUIRE(bench.world.RecordEffect(kBearerId, kFirstUnmeasuredEffect, kBornMs));
  REQUIRE(bench.world.RecordLastingEffect(kBearerId, kBoundedEffect, kBornMs));

  CHECK(moonclient::ForgetFinishedEffects(bench.world, AtTick(kMuchLaterTicks)) == 0);
  CHECK(bench.Carries(kEndlessEffect));
  CHECK(bench.Carries(kFirstUnmeasuredEffect));
  CHECK(bench.Carries(kBoundedEffect));

  REQUIRE(bench.world.RemoveEffect(kBearerId, kEndlessEffect));
  CHECK_FALSE(bench.Carries(kEndlessEffect));
}

TEST_CASE("Effets finis : ce que la liste relit après l'instance y reste", "[app][effectfinish]") {
  Bench bench;
  // La trace sur le corps, la valeur qu'un autre relit, et l'échéance que le
  // monde tient lui-même.
  REQUIRE(bench.world.RecordEffect(kBearerId, kBodyMarkingEffect, kBornMs));
  REQUIRE(bench.world.RecordEffect(kBearerId, kBoundedEffect, kBornMs, kToldValue));

  CHECK(moonclient::ForgetFinishedEffects(bench.world, AtTick(kMuchLaterTicks)) == 0);
  CHECK(bench.Carries(kBodyMarkingEffect));
  CHECK(bench.Carries(kBoundedEffect));

  ro::sim::PlayedEffect timed{.id = kBoundedEffect, .since_ms = kBornMs};
  timed.ends_ms = AtTick(kMuchLaterTicks);
  CHECK_FALSE(moonclient::EffectFinishedAt(timed, AtTick(kBoundedLifeTicks)));
}

TEST_CASE("Effets finis : un effet daté de l'avenir n'a pas commencé", "[app][effectfinish]") {
  const ro::sim::PlayedEffect ahead{.id = kBoundedEffect, .since_ms = AtTick(kBoundedLifeTicks)};
  CHECK_FALSE(moonclient::EffectFinishedAt(ahead, kBornMs));
}

TEST_CASE("Effets finis : le plafond tient toujours ce qui ne finit pas", "[app][effectfinish]") {
  Bench bench;
  for (std::uint32_t rank = 0; rank <= ro::sim::kEffectsKeptPerActor; ++rank) {
    REQUIRE(bench.world.RecordEffect(kBearerId, kFirstUnmeasuredEffect + rank, kBornMs));
  }
  CHECK(bench.Count() == ro::sim::kEffectsKeptPerActor);
  // Le plus ancien est parti, le plus récent est là.
  CHECK_FALSE(bench.Carries(kFirstUnmeasuredEffect));
  CHECK(bench.Carries(kFirstUnmeasuredEffect +
                      static_cast<std::uint32_t>(ro::sim::kEffectsKeptPerActor)));

  CHECK(moonclient::ForgetFinishedEffects(bench.world, AtTick(kMuchLaterTicks)) == 0);
  CHECK(bench.Count() == ro::sim::kEffectsKeptPerActor);
}

TEST_CASE("Effets finis : un effet fini laisse sa place sous le plafond", "[app][effectfinish]") {
  Bench bench;
  REQUIRE(bench.world.RecordEffect(kBearerId, kBoundedEffect, kBornMs));
  for (std::uint32_t rank = 1; rank < ro::sim::kEffectsKeptPerActor; ++rank) {
    REQUIRE(bench.world.RecordEffect(kBearerId, kFirstUnmeasuredEffect + rank, kBornMs));
  }
  REQUIRE(bench.Count() == ro::sim::kEffectsKeptPerActor);

  CHECK(moonclient::ForgetFinishedEffects(bench.world, AtTick(kBoundedLifeTicks)) == 1);
  // La place libérée reçoit le suivant sans chasser personne.
  REQUIRE(bench.world.RecordEffect(kBearerId, kFirstUnmeasuredEffect, AtTick(kBoundedLifeTicks)));
  CHECK(bench.Count() == ro::sim::kEffectsKeptPerActor);
  CHECK(bench.Carries(kFirstUnmeasuredEffect + 1));
}

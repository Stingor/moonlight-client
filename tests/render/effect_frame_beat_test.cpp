#include "ro/render/effect_frame_beat.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pool.h"

using ro::render::EffectNodePool;
using ro::render::kEffectTickMs;
using ro::render::PooledEffectNode;

TEST_CASE("les instances qui battent a chaque image sont celles du client",
          "[render][effectnode]") {
  CHECK(ro::render::EffectInstanceBeatsEveryFrame(37));
  CHECK(ro::render::EffectInstanceBeatsEveryFrame(312));
  CHECK(ro::render::EffectInstanceBeatsEveryFrame(530));
  CHECK(ro::render::EffectInstanceBeatsEveryFrame(1122));
  CHECK_FALSE(ro::render::EffectInstanceBeatsEveryFrame(36));
  CHECK_FALSE(ro::render::EffectInstanceBeatsEveryFrame(1123));
}

TEST_CASE("Troupe : les noeuds d'Increase Agility battent a chaque image", "[render][effectnode]") {
  // A cent vingt-cinq images par seconde, une image sur deux ne doit aucun tic :
  // eux battent quand meme, les autres attendent.
  EffectNodePool pool;
  PooledEffectNode pooled;
  pooled.node.life_ticks = 1000;
  pool.SowFor(ro::render::kIncreaseAgilityEffect);
  pool.Add(pooled);
  pool.SowFor(0);
  pool.Add(pooled);
  constexpr int kFrames = 40;
  for (int frame = 0; frame < kFrames; ++frame) {
    CHECK(pool.Advance(8) <= 1);
  }
  REQUIRE(pool.size() == 2);
  CHECK(pool.nodes()[0].node.age_ticks == kFrames);
  CHECK(pool.nodes()[1].node.age_ticks == kFrames / 2);
}

TEST_CASE("Troupe : l'instance emporte ses noeuds en mourant", "[render][effectnode]") {
  // UN NOEUD SEME A L'AGE t D'UNE INSTANCE DE VIE L reçoit L - 1 - t mises a
  // jour, quelle que soit la sienne : l'instance meurt a la mise a jour ou son
  // age atteint sa vie, et le noeud qui perd son proprietaire meurt avec.
  const std::int32_t life = ro::render::EffectNodeLifeFor(ro::render::kIncreaseAgilityEffect);
  constexpr std::int32_t kSowTick = 10;
  EffectNodePool pool;
  PooledEffectNode pooled;
  pooled.node.life_ticks = 100000;
  pool.SowFor(ro::render::kIncreaseAgilityEffect);
  pool.SowAtEffectTick(kSowTick);
  pool.Add(pooled);
  pool.SowFor(0);
  pool.SowAtEffectTick(0);
  REQUIRE(pool.nodes().front().instance_beats_left == life - 1 - kSowTick);
  for (std::int32_t step = 0; step < life - 1 - kSowTick; ++step) {
    pool.Advance(static_cast<std::uint32_t>(kEffectTickMs));
  }
  REQUIRE(pool.size() == 1);
  pool.Advance(static_cast<std::uint32_t>(kEffectTickMs));
  CHECK(pool.size() == 0);
}

TEST_CASE("Troupe : le fondu d'un noeud lit le reste de son instance, son age deja cru",
          "[render][effectnode]") {
  // A SA PREMIERE MISE A JOUR, le noeud seme a l'age t d'une instance de vie L
  // lit L - t - 1 : l'instance a deja vieilli de ce passage. Une image sans tic
  // fait vieillir l'instance qui bat a chaque image, et pas son noeud : le
  // fondu, juge a chaque passage, peut alors s'engager plus tard.
  constexpr std::uint32_t kPoisonPuffEffect = 53;
  REQUIRE(ro::render::EffectInstanceBeatsEveryFrame(kPoisonPuffEffect));
  REQUIRE_FALSE(ro::render::EffectBeatsEveryFrame(kPoisonPuffEffect));
  REQUIRE(ro::render::EffectInstanceLifeIsKnown(kPoisonPuffEffect));
  const std::int32_t life = ro::render::EffectInstanceLifeFor(kPoisonPuffEffect);
  EffectNodePool pool;
  PooledEffectNode pooled;
  pooled.node.life_ticks = life;
  pooled.node.alpha = 200.0F;
  pooled.node.fades_with_its_instance = true;
  pool.SowFor(kPoisonPuffEffect);
  pool.Add(pooled);
  pool.SowFor(0);
  pool.Advance(static_cast<std::uint32_t>(kEffectTickMs));
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes().front().node.instance_ticks_left == life - 1);
  CHECK(pool.nodes().front().node.owner_fade_at_age == ro::render::kEffectNodeOwnerFadeUnjudged);
  // Une image sans tic, puis le tic suivant.
  pool.Advance(1);
  pool.Advance(static_cast<std::uint32_t>(kEffectTickMs) - 1);
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes().front().node.owner_fade_at_age == 1);
  CHECK(pool.nodes().front().node.owner_fade_over_ticks == life - 3);
}

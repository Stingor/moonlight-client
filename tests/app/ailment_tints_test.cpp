// LA TEINTE QUE LES ALTÉRATIONS ÉCRIVENT, ET QUI RESTE, relue sur le monde.
//
// 🔴 UNE PANNE MUETTE EN JEU : recalculée à chaque image, la teinte d'un gelé
// guéri de son poison repassait au bleu, là où le client garde le poison
// jusqu'au dégel. Rien n'échouait.

#include "world/ailment_tints.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/ailment_sprites.h"
#include "ro/sim/world.h"

namespace {

constexpr std::uint32_t kHunter = 160'001;
constexpr std::uint16_t kOpt2Poison = 0x0001;

}  // namespace

TEST_CASE("Teinte qui reste : relue sur le monde, oubliée avec l'acteur", "[app][ailment]") {
  ro::sim::World world;
  ro::sim::Actor hunter;
  hunter.id = kHunter;
  world.Spawn(hunter);
  moonclient::AilmentTints tints;
  tints.Observe(world, 0);
  CHECK_FALSE(tints.TintOf(kHunter).has_value());

  REQUIRE(world.SetStates(kHunter, ro::sim::kBodyFrozen, 0, 0, 100));
  tints.Observe(world, 100);
  CHECK(tints.TintOf(kHunter) == ro::render::kFrozenTint);

  REQUIRE(world.SetStates(kHunter, ro::sim::kBodyFrozen, kOpt2Poison, 0, 200));
  tints.Observe(world, 200);
  CHECK(tints.TintOf(kHunter) == ro::render::kPoisonTint);

  // 🔴 GUÉRI DU POISON, TOUJOURS GELÉ : la teinte du poison reste.
  REQUIRE(world.SetStates(kHunter, ro::sim::kBodyFrozen, 0, 0, 300));
  tints.Observe(world, 300);
  CHECK(tints.TintOf(kHunter) == ro::render::kPoisonTint);

  REQUIRE(world.SetStates(kHunter, 0, 0, 0, 400));
  tints.Observe(world, 400);
  CHECK_FALSE(tints.TintOf(kHunter).has_value());

  // Un acteur qui paraît déjà gelé l'est dès son apparition, et qui part est
  // oublié.
  REQUIRE(world.SetStates(kHunter, ro::sim::kBodyFrozen, 0, 0, 500));
  REQUIRE(world.Remove(kHunter));
  tints.Observe(world, 500);
  CHECK_FALSE(tints.TintOf(kHunter).has_value());
  hunter.body_state = ro::sim::kBodyFrozen;
  world.Spawn(hunter);
  tints.Observe(world, 600);
  CHECK(tints.TintOf(kHunter) == ro::render::kFrozenTint);

  tints.Forget();
  CHECK_FALSE(tints.TintOf(kHunter).has_value());
}

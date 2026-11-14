// LES LUMIÈRES QUE PORTENT LES ACTEURS DE L'IMAGE : le réglage les allume ou
// les coupe, et le catalogue se lit à travers la table des classes.

#include "world/actor_lights_view.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using moonclient::SceneActorLights;
using ro::render::ActorLightBearer;

namespace {

constexpr std::uint32_t kSkeletonWorker = 1169;
constexpr double kAnyMs = 0.0;

[[nodiscard]] ro::render::SpriteTables Tables() {
  ro::render::SpriteTables tables;
  tables.class_sprites.emplace(kSkeletonWorker, "SKEL_WORKER");
  return tables;
}

}  // namespace

TEST_CASE("Lumières de la scène : le réglage coupé n'en allume aucune", "[app][actor_lights]") {
  const ro::render::SpriteTables tables = Tables();
  const ActorLightBearer worker{.seed = 1, .job = kSkeletonWorker};
  SceneActorLights lights;
  CHECK(lights.Gather({&worker, 1}, &tables, true, kAnyMs).size() == 1);
  CHECK(lights.Gather({&worker, 1}, &tables, false, kAnyMs).empty());
}

TEST_CASE("Lumières de la scène : sans table des classes, aucune", "[app][actor_lights]") {
  const ActorLightBearer worker{.seed = 1, .job = kSkeletonWorker};
  SceneActorLights lights;
  CHECK(lights.Gather({&worker, 1}, nullptr, true, kAnyMs).empty());
}

TEST_CASE("Lumières de la scène : une autre table se relit", "[app][actor_lights]") {
  const ro::render::SpriteTables first = Tables();
  const ro::render::SpriteTables empty;
  const ActorLightBearer worker{.seed = 1, .job = kSkeletonWorker};
  SceneActorLights lights;
  CHECK(lights.Gather({&worker, 1}, &first, true, kAnyMs).size() == 1);
  CHECK(lights.Gather({&worker, 1}, &empty, true, kAnyMs).empty());
}

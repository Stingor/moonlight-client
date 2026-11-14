// LA LÉVITATION DE CHAQUE ACTEUR QUI VOLE : seule l'option du vol de l'union la
// fait naître, et chaque partie peinte fait avancer son angle.

#include "world/union_hovers.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_layers.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/shown_job.h"
#include "ro/sim/actor.h"

namespace {

constexpr std::uint32_t kFlierId = 150'777;
// Le temps qu'il faut au palier pour atteindre vingt avec deux parties peintes :
// trois cents pas, cent cinquante images du client.
constexpr std::uint32_t kSettledMs = 3'000;

ro::sim::Actor Flier(std::uint32_t option) {
  ro::sim::Actor actor;
  actor.id = kFlierId;
  actor.option = option;
  return actor;
}

// Un corps et une tête, un calque chacun.
ro::render::ComposedActor BodyAndHead() {
  ro::render::ComposedActor composed;
  composed.quads.resize(2);
  composed.parts = {ro::render::ActorPart::kBody, ro::render::ActorPart::kHead};
  return composed;
}

}  // namespace

TEST_CASE("Lévitation : sans l'option du vol, personne ne flotte", "[union_hovers]") {
  moonclient::UnionHovers hovers;
  const ro::render::ComposedActor composed = BodyAndHead();
  const ro::sim::Actor walker = Flier(0);
  CHECK(hovers.PixelsOf(walker, ro::render::Motion::kIdle, false, composed, 0) == 0.0F);
  CHECK(hovers.PixelsOf(walker, ro::render::Motion::kIdle, false, composed, kSettledMs) == 0.0F);
}

TEST_CASE("Lévitation : en vol et debout, le corps se pose autour de vingt pixels",
          "[union_hovers]") {
  moonclient::UnionHovers hovers;
  const ro::render::ComposedActor composed = BodyAndHead();
  const ro::sim::Actor flier = Flier(ro::render::kOptionFlying);
  static_cast<void>(hovers.PixelsOf(flier, ro::render::Motion::kIdle, false, composed, 0));
  const float settled =
      hovers.PixelsOf(flier, ro::render::Motion::kIdle, false, composed, kSettledMs);
  CHECK(settled >= 16.0F);
  CHECK(settled <= 24.0F);
  // Un coup le pose : vingt pas suffisent, une image de deux parties en fait deux.
  constexpr std::uint32_t kLandedMs = kSettledMs + 500;
  CHECK(hovers.PixelsOf(flier, ro::render::Motion::kAttack, false, composed, kLandedMs) <= 4.0F);
}

TEST_CASE("Lévitation : un acteur que la scène ne compose plus perd sa lévitation",
          "[union_hovers]") {
  moonclient::UnionHovers hovers;
  const ro::render::ComposedActor composed = BodyAndHead();
  const ro::sim::Actor flier = Flier(ro::render::kOptionFlying);
  static_cast<void>(hovers.PixelsOf(flier, ro::render::Motion::kIdle, false, composed, 0));
  const float settled =
      hovers.PixelsOf(flier, ro::render::Motion::kIdle, false, composed, kSettledMs);
  REQUIRE(settled >= 16.0F);
  hovers.ForgetUnseen();
  // Composé à l'image précédente : il garde son palier.
  CHECK(hovers.PixelsOf(flier, ro::render::Motion::kIdle, false, composed, kSettledMs + 1) >=
        16.0F);
  hovers.ForgetUnseen();
  // Une image sans lui, puis il revient : tout repart de zéro.
  hovers.ForgetUnseen();
  CHECK(hovers.PixelsOf(flier, ro::render::Motion::kIdle, false, composed, kSettledMs + 2) <= 4.0F);
}

TEST_CASE("Lévitation : on compte les parties peintes, pas les calques", "[union_hovers]") {
  ro::render::ComposedActor composed;
  CHECK(moonclient::PartsDrawnOf(composed) == 0);
  composed.quads.resize(3);
  // Sans partie déclarée, chaque calque compte pour le corps.
  CHECK(moonclient::PartsDrawnOf(composed) == 1);
  composed.parts = {ro::render::ActorPart::kBody, ro::render::ActorPart::kBody,
                    ro::render::ActorPart::kWeapon};
  CHECK(moonclient::PartsDrawnOf(composed) == 2);
}
